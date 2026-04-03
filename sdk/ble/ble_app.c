/**
 * ble_app.c — BLE application init, event queue, and advertising
 *
 * Replaces the ble-wip's hal_ble.c + app_entry.c with a single file
 * that uses our LL/HAL infrastructure. No ST HAL dependency.
 */

#include <stdint.h>
#include <string.h>
#include "app_common.h"         /* Must come first — defines MAX, MIN, etc. */
#include "core_config.h"
#include "ll_common.h"
#include "ll_rcc.h"
#include "blestack.h"
#include "auto/ble_gap_aci.h"
#include "auto/ble_gatt_aci.h"
#include "auto/ble_hal_aci.h"
#include "ble_defs.h"
#include "ble_bufsize.h"
#include "bleplat.h"
#include "app_conf.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "stm32_mm.h"
#include "advanced_memory_manager.h"
#include "stm_list.h"
#include "svc_ctl.h"
#include "ll_sys.h"
#include "ll_sys_if.h"
#include "bpka.h"
#include "hw.h"

/* Forward declarations for binary lib functions */
extern void BleStackCB_Process(void);
extern void ll_sys_ble_cntrl_init(void *hostCallback);

/* ============================================================
 * BLE stack memory config
 * ============================================================ */

#define CFG_BLE_NUM_LINK             1
#define CFG_BLE_NUM_GATT_SERVICES    2
#define CFG_BLE_NUM_GATT_ATTRIBUTES  9
#define CFG_BLE_ATT_VALUE_ARRAY_SIZE 100
#define CFG_BLE_ATT_MTU_MAX          23
#define CFG_BLE_MBLOCK_COUNT_MARGIN  0
#define CFG_BLE_COC_NBR_MAX          0
#define CFG_BLE_COC_MPS_MAX          0
#define CFG_BLE_COC_INITIATOR_NBR_MAX 0
#define PREP_WRITE_LIST_SIZE         0
#define CFG_BLE_OPTIONS              0

#ifndef DIVC
#define DIVC(x, y)  (((x) + (y) - 1) / (y))
#endif

/* Stack buffers */
#define BLE_GATT_BUF_SIZE \
    BLE_TOTAL_BUFFER_SIZE_GATT(CFG_BLE_NUM_GATT_ATTRIBUTES, \
                               CFG_BLE_NUM_GATT_SERVICES, \
                               CFG_BLE_ATT_VALUE_ARRAY_SIZE)

#define MBLOCK_COUNT (BLE_MBLOCKS_CALC(PREP_WRITE_LIST_SIZE, \
                                        CFG_BLE_ATT_MTU_MAX, \
                                        CFG_BLE_NUM_LINK) \
                      + CFG_BLE_MBLOCK_COUNT_MARGIN)

#define BLE_DYN_ALLOC_SIZE BLE_TOTAL_BUFFER_SIZE(CFG_BLE_NUM_LINK, MBLOCK_COUNT)

static uint32_t ble_buffer[DIVC(BLE_DYN_ALLOC_SIZE, 4)] __attribute__((aligned(4)));
static uint32_t gatt_buffer[DIVC(BLE_GATT_BUF_SIZE, 4)] __attribute__((aligned(4)));

/* GAP handles */
static uint16_t gap_service_handle;
static uint16_t gap_dev_name_handle;
static uint16_t gap_appearance_handle;

/* ============================================================
 * Async Event Queue — buffers HCI events from BLE stack
 * ============================================================ */

#define EVT_POOL_SIZE        8

typedef struct {
    tListNode node;
    struct {
        uint8_t type;
        struct {
            uint8_t evtcode;
            uint8_t plen;
            uint8_t payload[256];
        } evt;
    } evtserial;
} BleEvtPacket_t;

static BleEvtPacket_t evt_pool[EVT_POOL_SIZE];
static uint8_t evt_pool_used[EVT_POOL_SIZE];
static tListNode BleAsynchEventQueue;
static uint8_t ble_evt_queue_ready;

static BleEvtPacket_t *evt_pool_alloc(void)
{
    for (int i = 0; i < EVT_POOL_SIZE; i++) {
        if (!evt_pool_used[i]) {
            evt_pool_used[i] = 1;
            return &evt_pool[i];
        }
    }
    return (BleEvtPacket_t *)0;
}

static void evt_pool_free(BleEvtPacket_t *pkt)
{
    for (int i = 0; i < EVT_POOL_SIZE; i++) {
        if (&evt_pool[i] == pkt) {
            evt_pool_used[i] = 0;
            return;
        }
    }
}

/* Drain task — routes events through SVCCTL */
static void Ble_UserEvtRx(void)
{
    BleEvtPacket_t *pkt = (BleEvtPacket_t *)0;
    LST_remove_head(&BleAsynchEventQueue, (tListNode **)&pkt);
    if (!pkt) return;

    SVCCTL_UserEvtFlowStatus_t status;
    status = SVCCTL_UserEvtRx((void *)&(pkt->evtserial));

    if (status != SVCCTL_UserEvtFlowDisable) {
        evt_pool_free(pkt);
    } else {
        LST_insert_head(&BleAsynchEventQueue, (tListNode *)pkt);
    }

    if (LST_is_empty(&BleAsynchEventQueue) == FALSE) {
        UTIL_SEQ_SetTask(1U << CFG_TASK_HCI_ASYNCH_EVT_ID, 0);
    }

    UTIL_SEQ_SetTask(1U << CFG_TASK_BLE_HOST, 0);
}

/* BLECB_Indication — called by BleStack_Process for each HCI event */
uint8_t BLECB_Indication(const uint8_t *data, uint16_t length,
                         const uint8_t *ext_data, uint16_t ext_length)
{
    (void)length;
    (void)ext_data;
    (void)ext_length;

    if (!ble_evt_queue_ready) return 1;
    if (data[0] != HCI_EVENT_PKT_TYPE) return 1;

    BleEvtPacket_t *pkt = evt_pool_alloc();
    if (!pkt) return 1;

    pkt->evtserial.type = HCI_EVENT_PKT_TYPE;
    pkt->evtserial.evt.evtcode = data[1];
    pkt->evtserial.evt.plen = data[2];
    uint16_t copy_len = data[2];
    if (copy_len > sizeof(pkt->evtserial.evt.payload))
        copy_len = sizeof(pkt->evtserial.evt.payload);
    memcpy(pkt->evtserial.evt.payload, &data[3], copy_len);

    LST_insert_tail(&BleAsynchEventQueue, (tListNode *)pkt);
    UTIL_SEQ_SetTask(1U << CFG_TASK_HCI_ASYNCH_EVT_ID, 0);

    return 0;
}

/* BLE host background task */
static void ble_host_task(void)
{
    if (BleStack_Process() == 0x0) {
        BleStackCB_Process();
    }
}

/* ============================================================
 * HSE tuning from OTP
 * ============================================================ */

/* OTP memory on WBA55 */
#define OTP_AREA_BASE   0x0BFA0000UL

/* RCC_ECSCR1 at offset 0x210: HSETRIM bits [21:16] */
#define RCC_ECSCR1      REG32(RCC_BASE + 0x210UL)

static void config_hse_tuning(void)
{
    /* Apply default HSE trim (0x0C).
     * TODO: read from OTP once we find the correct non-secure OTP address.
     * 0x0BFA0000 is not accessible — may need secure access or different base. */
    uint8_t hsetune = 0x0C;

    /* Write HSE trim: RCC_ECSCR1 register, HSETRIM bits [21:16] */
    MOD_BITS(RCC_ECSCR1, 0x3FUL << 16, ((uint32_t)(hsetune & 0x3F)) << 16);
}

/* ============================================================
 * ble_app_init — THE main init function
 * ============================================================ */

static uint8_t ble_init_done;

void ble_app_init(void)
{
    /* 1. HSE tuning from OTP */
    config_hse_tuning();

    /* 1b. Match working project's peripheral clock config:
     *     - RNG clock source = HSI (CCIPR2 RNGSEL bits [13:12] = 0b10)
     *     - Enable HASH clock (AHB2ENR bit 16) — used as BLE SW low ISR
     *     - Enable RNG clock (AHB2ENR bit 18) */
    MOD_BITS(REG32(RCC_BASE + 0xE4UL), 0x3UL << 12, 0x2UL << 12);
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << 16) | (1UL << 18));  /* HASHEN + RNGEN */

    /* Configure RNG: enable conditioning, set NIST compliance */
    {
        #define RNG_BASE_NS  (PERIPH_BASE + 0x020C0800UL)  /* 0x420C0800 */
        #define RNG_CR_REG   REG32(RNG_BASE_NS + 0x00UL)
        /* Disable RNG before config */
        RNG_CR_REG = 0;
        /* RNGEN=1, CONDRST=1 (start conditioning reset) */
        RNG_CR_REG = (1UL << 2) | (1UL << 6);
        /* Clear CONDRST to complete conditioning config */
        RNG_CR_REG = (1UL << 2);
    }

    /* 2. Radio sleep clock setup — HSE/1024.
     *    Register dump of working CubeWBA project confirms RADIOSTSEL=0b10
     *    (HSE/1024) despite its code saying RCC_RADIOSTCLKSOURCE_LSI —
     *    the HAL value maps differently than the LL value. */
    ll_pwr_enable_backup_access();
    ll_rcc_lsi1_enable_wait();  /* LSI1 still needed by link layer */
    ll_rcc_set_radio_sleep_clk(LL_RCC_RADIOSLEEPSOURCE_HSE_DIV);

    /* 3. Initialize sequencer */
    UTIL_SEQ_Init();

    /* 4. Initialize timer server */
    UTIL_TIMER_Init();

    /* 5. Initialize link layer controller
     *    ll_sys_ble_cntrl_init calls ll_sys_bg_process_init and
     *    ll_sys_config_params internally. Pass HostStack_Process as the
     *    host callback. */
    extern void HostStack_Process(void);
    ll_sys_ble_cntrl_init((void *)HostStack_Process);

    /* 6. Initialize BLEPLAT (crypto, timer, RNG dispatch) */
    BLEPLAT_Init();

    /* 7. Initialize AMM */
    static AMM_InitParameters_t amm_init;
    static AMM_VirtualMemoryConfig_t amm_vms[CFG_AMM_VIRTUAL_MEMORY_NUMBER];
    amm_vms[0].Id = CFG_AMM_VIRTUAL_STACK_BLE;
    amm_vms[0].BufferSize = CFG_AMM_VIRTUAL_STACK_BLE_BUFFER_SIZE;
    amm_vms[1].Id = CFG_AMM_VIRTUAL_APP_BLE;
    amm_vms[1].BufferSize = CFG_AMM_VIRTUAL_APP_BLE_BUFFER_SIZE;
    amm_init.p_PoolAddr = NULL;  /* Set by AMM_RegisterBasicMemoryManager callback */
    amm_init.PoolSize = CFG_AMM_POOL_SIZE;
    amm_init.VirtualMemoryNumber = CFG_AMM_VIRTUAL_MEMORY_NUMBER;
    amm_init.p_VirtualMemoryConfigList = amm_vms;
    AMM_Init(&amm_init);

    /* Register AMM + BPKA background tasks */
    UTIL_SEQ_RegTask(1U << CFG_TASK_AMM, 0, AMM_BackgroundProcess);
    UTIL_SEQ_RegTask(1U << CFG_TASK_BPKA, 0, BPKA_BG_Process);

    /* 8. Event queue + host task */
    LST_init_head(&BleAsynchEventQueue);
    memset(evt_pool_used, 0, sizeof(evt_pool_used));
    ble_evt_queue_ready = 1;
    UTIL_SEQ_RegTask(1U << CFG_TASK_HCI_ASYNCH_EVT_ID, 0, Ble_UserEvtRx);
    UTIL_SEQ_RegTask(1U << CFG_TASK_BLE_HOST, 0, ble_host_task);

    /* 9. Initialize BLE stack */
    BleStack_init_t params;
    params.numAttrRecord           = CFG_BLE_NUM_GATT_ATTRIBUTES;
    params.numAttrServ             = CFG_BLE_NUM_GATT_SERVICES;
    params.attrValueArrSize        = CFG_BLE_ATT_VALUE_ARRAY_SIZE;
    params.prWriteListSize         = PREP_WRITE_LIST_SIZE;
    params.attMtu                  = CFG_BLE_ATT_MTU_MAX;
    params.max_coc_nbr             = CFG_BLE_COC_NBR_MAX;
    params.max_coc_mps             = CFG_BLE_COC_MPS_MAX;
    params.max_coc_initiator_nbr   = CFG_BLE_COC_INITIATOR_NBR_MAX;
    params.numOfLinks              = CFG_BLE_NUM_LINK;
    params.mblockCount             = MBLOCK_COUNT;
    params.bleStartRamAddress      = (uint8_t *)ble_buffer;
    params.total_buffer_size       = BLE_DYN_ALLOC_SIZE;
    params.bleStartRamAddress_GATT = (uint8_t *)gatt_buffer;
    params.total_buffer_size_GATT  = BLE_GATT_BUF_SIZE;
    params.options                 = CFG_BLE_OPTIONS;
    params.debug                   = 0U;

    BleStack_Init(&params);

    /* Set BD address (public address, matches working CubeWBA project) */
    {
        uint8_t bd_addr[6] = {0x34, 0x12, 0x2A, 0xE1, 0x08, 0x00};
        aci_hal_write_config_data(0x00 /* CONFIG_DATA_PUBADDR_OFFSET */, 6, bd_addr);
    }

    /* Set TX power */
    aci_hal_set_tx_power_level(1, 0x19);

    /* Init GATT + GAP */
    aci_gatt_init();
    aci_gap_init(0x01 /* GAP_PERIPHERAL_ROLE */, 0x00 /* privacy disabled */, 16,
                 &gap_service_handle,
                 &gap_dev_name_handle,
                 &gap_appearance_handle);

    /* Init service controller */
    SVCCTL_Init();

    ble_init_done = 1;
}

/* ============================================================
 * ble_app_advertise — start BLE advertising
 * ============================================================ */

int ble_app_advertise(const char *name)
{
    tBleStatus ret;

    /* Match the working CubeWBA project exactly:
     * 1. aci_gap_set_discoverable with NO name (all zeros)
     * 2. aci_gap_delete_ad_type to remove TX power level
     * 3. aci_gap_update_adv_data with explicit advertising data */

    ret = aci_gap_set_discoverable(
        0x00,           /* ADV_IND */
        0x0080,         /* min interval: 80ms */
        0x00A0,         /* max interval: 100ms */
        0x00,           /* public address */
        0x00,           /* no filter */
        0, NULL,        /* no local name in this call */
        0, NULL,        /* no service UUIDs */
        0x0006, 0x0010  /* conn interval min/max */
    );

    if (ret != 0) return -1;

    /* Remove TX power level from advertising data */
    #ifndef AD_TYPE_TX_POWER_LEVEL
    #define AD_TYPE_TX_POWER_LEVEL  0x0A
    #endif
    #ifndef AD_TYPE_COMPLETE_NAME
    #define AD_TYPE_COMPLETE_NAME   0x09
    #endif
    aci_gap_delete_ad_type(AD_TYPE_TX_POWER_LEVEL);

    /* Build advertising data with complete local name */
    uint8_t name_len = 0;
    while (name[name_len] && name_len < 20) name_len++;

    uint8_t adv_data[31];
    uint8_t pos = 0;

    /* AD element: Complete Local Name */
    adv_data[pos++] = name_len + 1;   /* length of this AD element */
    adv_data[pos++] = AD_TYPE_COMPLETE_NAME;
    for (uint8_t i = 0; i < name_len; i++)
        adv_data[pos++] = (uint8_t)name[i];

    ret = aci_gap_update_adv_data(pos, adv_data);

    return (ret == 0) ? 0 : -1;
}
