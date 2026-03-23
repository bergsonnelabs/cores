/**
 * hal_ble.c — BLE HAL wrapper for STM32WBA55
 *
 * Wraps the ST BLE stack binary (stm32wba_ble_stack_basic.a) with
 * a clean C API. Handles stack init, GAP/GATT setup, and advertising.
 *
 * System initialization (HSE tuning, SCM, LPM, RNG, flash, BPKA, SNVMA)
 * is handled by app_entry.c (sdk/stm/) via MX_APPE_Config/Init.
 */

#include "hal_ble.h"
#include "hal_common.h"
#include "ll_common.h"
#include "ll_rcc.h"
#include "ll_systick.h"
#include "tile_board.h"
#include "blestack.h"
#include "auto/ble_gap_aci.h"
#include "auto/ble_gatt_aci.h"
#include "auto/ble_hal_aci.h"
#include "ble_defs.h"
#include "ble_bufsize.h"
#include "bleplat.h"
#include "app_conf.h"
#include "stm32_timer.h"

/* Forward declarations */
static void ble_evt_queue_init(void);
static void _ble_host_task(void);

/* ============================================================
 * Configuration
 * ============================================================ */

/* Minimal config — proven to not hang aci_gap_init */
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
#define CFG_TX_POWER                 0x19  /* -0.3 dBm */

#ifndef DIVC
#define DIVC(x, y)  (((x) + (y) - 1) / (y))
#endif

#ifndef MAX
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#endif

/* ============================================================
 * Stack memory buffers — placed in SRAM2 via linker section
 * ============================================================ */

#define BLE_GATT_BUF_SIZE \
    BLE_TOTAL_BUFFER_SIZE_GATT(CFG_BLE_NUM_GATT_ATTRIBUTES, \
                               CFG_BLE_NUM_GATT_SERVICES, \
                               CFG_BLE_ATT_VALUE_ARRAY_SIZE)

#define MBLOCK_COUNT (BLE_MBLOCKS_CALC(PREP_WRITE_LIST_SIZE, \
                                        CFG_BLE_ATT_MTU_MAX, \
                                        CFG_BLE_NUM_LINK) \
                      + CFG_BLE_MBLOCK_COUNT_MARGIN)

#define BLE_DYN_ALLOC_SIZE BLE_TOTAL_BUFFER_SIZE(CFG_BLE_NUM_LINK, MBLOCK_COUNT)

static uint32_t ble_buffer[DIVC(BLE_DYN_ALLOC_SIZE, 4)]
    __attribute__((section(".bss_ble"), aligned(4)));

static uint32_t gatt_buffer[DIVC(BLE_GATT_BUF_SIZE, 4)]
    __attribute__((section(".bss_ble"), aligned(4)));

/* ============================================================
 * State
 * ============================================================ */

static volatile int ble_connected = 0;
static uint16_t ble_conn_handle = 0xFFFF;

static uint16_t gap_service_handle;
static uint16_t gap_dev_name_handle;
static uint16_t gap_appearance_handle;

static hal_ble_connect_cb_t    user_connect_cb = (void *)0;
static hal_ble_disconnect_cb_t user_disconnect_cb = (void *)0;

static void _ble_host_task(void);

/* ============================================================
 * Init — called AFTER MX_APPE_Init has set up the system
 * ============================================================ */

void hal_ble_init(void)
{
    /* Zero the BLE buffers in SRAM2 (startup only zeros SRAM1 BSS) */
    extern uint32_t _sbss_ble, _ebss_ble;
    for (uint32_t *p = &_sbss_ble; p < &_ebss_ble; p++)
        *p = 0;

    /* Initialize async event queue + sequencer tasks BEFORE BleStack_Init.
       Events may be produced during init. */
    ble_evt_queue_init();

    /* Initialize BLE stack */
    BleStack_init_t init_params;
    init_params.numAttrRecord           = CFG_BLE_NUM_GATT_ATTRIBUTES;
    init_params.numAttrServ             = CFG_BLE_NUM_GATT_SERVICES;
    init_params.attrValueArrSize        = CFG_BLE_ATT_VALUE_ARRAY_SIZE;
    init_params.prWriteListSize         = PREP_WRITE_LIST_SIZE;
    init_params.attMtu                  = CFG_BLE_ATT_MTU_MAX;
    init_params.max_coc_nbr             = CFG_BLE_COC_NBR_MAX;
    init_params.max_coc_mps             = CFG_BLE_COC_MPS_MAX;
    init_params.max_coc_initiator_nbr   = CFG_BLE_COC_INITIATOR_NBR_MAX;
    init_params.numOfLinks              = CFG_BLE_NUM_LINK;
    init_params.mblockCount             = MBLOCK_COUNT;
    init_params.bleStartRamAddress      = (uint8_t *)ble_buffer;
    init_params.total_buffer_size       = BLE_DYN_ALLOC_SIZE;
    init_params.bleStartRamAddress_GATT = (uint8_t *)gatt_buffer;
    init_params.total_buffer_size_GATT  = BLE_GATT_BUF_SIZE;
    init_params.options                 = CFG_BLE_OPTIONS;
    init_params.debug                   = 0U;

    BleStack_Init(&init_params);

    /* Set PUBLIC BD address */
    {
        uint8_t bd_addr[6] = {0x34, 0x12, 0x2A, 0xE1, 0x08, 0x00};
        aci_hal_write_config_data(0x00, 6, bd_addr);  /* CONFIG_DATA_PUBADDR_OFFSET */
    }

    /* Set TX power to maximum */
    aci_hal_set_tx_power_level(1, 0x19);

    /* Initialize GATT + GAP */
    aci_gatt_init();
    aci_gap_init(0x01, 0x00, 16,
                 &gap_service_handle,
                 &gap_dev_name_handle,
                 &gap_appearance_handle);

    /* Initialize service controller AFTER BleStack_Init + GAP/GATT */
    extern void SVCCTL_Init(void);
    SVCCTL_Init();

    /* Register BLE host processing as a sequencer task.
       The BLE stack triggers this via UTIL_SEQ_SetTask internally. */
    extern void UTIL_SEQ_RegTask(uint32_t task_id_bm, uint32_t flags, void (*func)(void));
    UTIL_SEQ_RegTask(1U << CFG_TASK_BLE_HOST, 0, (void (*)(void))_ble_host_task);
}

/* BLE host background task — called via sequencer.
   Matches the working project's BleStack_Process_BG exactly. */
static void _ble_host_task(void)
{
    extern void BleStackCB_Process(void);
    if (BleStack_Process() == 0x0)
    {
        BleStackCB_Process();
    }
}

/* ============================================================
 * Process — call regularly from main loop
 * ============================================================ */

void hal_ble_process(void)
{
    /* Only run the sequencer — matching the working project's MX_APPE_Process().
       BleStack_Process + BleStackCB_Process are called via the sequencer task
       (BleStack_Process_BG / _ble_host_task). Calling them directly from here
       in ADDITION to the sequencer causes double-processing that corrupts
       the BLE stack's internal state. */
    extern void UTIL_SEQ_Run(uint32_t mask);
    UTIL_SEQ_Run(~0UL);
}

/* ============================================================
 * Advertise
 * ============================================================ */

hal_status_t hal_ble_advertise(const char *name)
{
    /* Use GAP-level API — same as working project */
    extern tBleStatus aci_gap_set_discoverable(
        uint8_t Advertising_Type, uint16_t Advertising_Interval_Min,
        uint16_t Advertising_Interval_Max, uint8_t Own_Address_Type,
        uint8_t Advertising_Filter_Policy, uint8_t Local_Name_Length,
        const uint8_t *Local_Name, uint8_t Service_Uuid_length,
        const uint8_t *Service_Uuid_List, uint16_t Conn_Interval_Min,
        uint16_t Conn_Interval_Max);

    uint8_t name_len = 0;
    while (name[name_len] && name_len < 20) name_len++;

    /* Local name with AD type prefix (0x09 = Complete Local Name) */
    uint8_t local_name[22];
    local_name[0] = 0x09;
    for (uint8_t i = 0; i < name_len; i++)
        local_name[i + 1] = (uint8_t)name[i];

    tBleStatus ret = aci_gap_set_discoverable(
        0x00,           /* ADV_IND */
        0x0080,         /* min interval: 80ms */
        0x00A0,         /* max interval: 100ms */
        0x00,           /* public address */
        0x00,           /* no filter */
        name_len + 1,   /* local name length (includes AD type) */
        local_name,     /* name with 0x09 prefix */
        0, NULL,        /* no service UUIDs */
        0x0006, 0x0010  /* conn interval min/max */
    );

    return (ret == 0) ? HAL_OK : HAL_ERROR;
}

hal_status_t hal_ble_stop_advertise(void)
{
    extern tBleStatus aci_gap_set_non_discoverable(void);
    tBleStatus ret = aci_gap_set_non_discoverable();
    return (ret == 0) ? HAL_OK : HAL_ERROR;
}

/* ============================================================
 * Connection state
 * ============================================================ */

int hal_ble_connected(void)
{
    return ble_connected;
}

void hal_ble_on_connect(hal_ble_connect_cb_t cb)
{
    user_connect_cb = cb;
}

void hal_ble_on_disconnect(hal_ble_disconnect_cb_t cb)
{
    user_disconnect_cb = cb;
}

hal_status_t hal_ble_notify(uint16_t conn_handle, uint16_t char_handle,
                            const uint8_t *data, uint16_t len)
{
    (void)conn_handle;
    (void)char_handle;
    (void)data;
    (void)len;
    /* TODO: implement aci_gatt_update_char_value */
    return HAL_ERROR;
}

/* ============================================================
 * Async Event Queue — buffers HCI events from BLE stack
 *
 * The BLE host stack produces events via BLECB_Indication.
 * These are queued here and drained by Ble_UserEvtRx (sequencer task).
 * Without this queue, events are dropped and advertising never activates.
 * ============================================================ */

#include "stm_list.h"
#include "svc_ctl.h"
#include <string.h>

/* HCI event packet types */
#define HCI_EVENT_PKT_TYPE   0x04

typedef struct {
    tListNode node;         /* doubly-linked list pointers (must be first) */
    struct {
        uint8_t type;       /* HCI_EVENT_PKT_TYPE */
        struct {
            uint8_t evtcode;
            uint8_t plen;
            uint8_t payload[256];
        } evt;
    } evtserial;
} BleEvtPacket_t;

/* Static event pool — simple fixed-block allocator. */
#define EVT_POOL_SIZE  8
static BleEvtPacket_t evt_pool[EVT_POOL_SIZE];
static uint8_t evt_pool_used[EVT_POOL_SIZE];

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

/* Async event queue */
static tListNode BleAsynchEventQueue;
static uint8_t ble_evt_queue_initialized = 0;

/* Forward declaration */
static void Ble_UserEvtRx(void);

/* Initialize the event queue — called from hal_ble_init */
static void ble_evt_queue_init(void)
{
    LST_init_head(&BleAsynchEventQueue);
    memset(evt_pool_used, 0, sizeof(evt_pool_used));
    ble_evt_queue_initialized = 1;

    /* Register the drain task with the sequencer */
    extern void UTIL_SEQ_RegTask(uint32_t task_id_bm, uint32_t flags, void (*func)(void));
    UTIL_SEQ_RegTask(1U << CFG_TASK_HCI_ASYNCH_EVT_ID, 0, Ble_UserEvtRx);
}

/* ============================================================
 * BLECB_Indication — THE CRITICAL CALLBACK
 *
 * Called by BleStack_Process() for every HCI event the host
 * stack produces. We allocate a buffer, copy the event,
 * queue it, and trigger the async drain task.
 *
 * Returning 0 (BLE_STATUS_SUCCESS) tells the stack the event
 * was consumed. Returning non-zero means it was dropped.
 * ============================================================ */

__attribute__((weak))
uint8_t BLECB_Indication(const uint8_t *data, uint16_t length,
                         const uint8_t *ext_data, uint16_t ext_length)
{
    (void)ext_data;
    (void)ext_length;

    if (!ble_evt_queue_initialized) return 1;
    if (data[0] != HCI_EVENT_PKT_TYPE) return 1;

    BleEvtPacket_t *pkt = evt_pool_alloc();
    if (!pkt) return 1;

    /* Copy event data */
    pkt->evtserial.type = HCI_EVENT_PKT_TYPE;
    pkt->evtserial.evt.evtcode = data[1];
    pkt->evtserial.evt.plen = data[2];
    uint16_t copy_len = data[2];
    if (copy_len > sizeof(pkt->evtserial.evt.payload))
        copy_len = sizeof(pkt->evtserial.evt.payload);
    memcpy(pkt->evtserial.evt.payload, &data[3], copy_len);

    /* Queue the event */
    LST_insert_tail(&BleAsynchEventQueue, (tListNode *)pkt);

    /* Trigger the drain task */
    extern void UTIL_SEQ_SetTask(uint32_t task_id_bm, uint32_t prio);
    UTIL_SEQ_SetTask(1U << CFG_TASK_HCI_ASYNCH_EVT_ID, 0);

    return 0;  /* BLE_STATUS_SUCCESS — event consumed */
}

/* ============================================================
 * Ble_UserEvtRx — Drain the async event queue
 *
 * Called by the sequencer when events are pending. Pulls each
 * event from the queue and routes it through SVCCTL.
 * ============================================================ */

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
        /* Flow disabled — put it back */
        LST_insert_head(&BleAsynchEventQueue, (tListNode *)pkt);
    }

    /* If more events pending, re-trigger */
    if (LST_is_empty(&BleAsynchEventQueue) == FALSE) {
        extern void UTIL_SEQ_SetTask(uint32_t task_id_bm, uint32_t prio);
        UTIL_SEQ_SetTask(1U << CFG_TASK_HCI_ASYNCH_EVT_ID, 0);
    }

    /* Trigger BLE host processing */
    extern void UTIL_SEQ_SetTask(uint32_t task_id_bm, uint32_t prio);
    UTIL_SEQ_SetTask(1U << CFG_TASK_BLE_HOST, 0);
}

/* ============================================================
 * SVCCTL application notification — required by svc_ctl.c
 * Called for GAP events and unhandled GATT events.
 * ============================================================ */

__attribute__((weak))
SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *p_pckt)
{
    (void)p_pckt;
    /* Accept all events — don't block the flow */
    return SVCCTL_UserEvtFlowEnable;
}
