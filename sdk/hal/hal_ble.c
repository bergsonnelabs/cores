/**
 * hal_ble.c — BLE HAL wrapper for STM32WBA55
 *
 * Wraps the ST BLE stack binary (stm32wba_ble_stack_basic.a) with
 * a clean C API. Handles stack init, GAP/GATT setup, and advertising.
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

/* ============================================================
 * Configuration
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
#define CFG_BLE_OPTIONS              (BLE_OPTIONS_DEV_NAME_READ_ONLY)
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

/* Debug: store return values for CubeProgrammer inspection */
volatile uint32_t dbg_blestack_init_ret = 0xDEAD;
volatile uint32_t dbg_gatt_init_ret = 0xDEAD;
volatile uint32_t dbg_gap_init_ret = 0xDEAD;
volatile uint32_t dbg_advertise_ret = 0xDEAD;
volatile uint32_t dbg_scan_rsp_ret = 0xDEAD;
static uint16_t ble_conn_handle = 0xFFFF;

static uint16_t gap_service_handle;
static uint16_t gap_dev_name_handle;
static uint16_t gap_appearance_handle;

static hal_ble_connect_cb_t    user_connect_cb = (void *)0;
static hal_ble_disconnect_cb_t user_disconnect_cb = (void *)0;

static void _ble_host_task(void);

/* RCC / PWR defines handled via ll_rcc.h LL functions */

/* ============================================================
 * Init
 * ============================================================ */

void hal_ble_init(void)
{
    /* NVIC priority grouping — required for BLE interrupt management.
       Group 4 = 4 bits preemption, 0 bits subpriority (matching HAL_Init). */
    #define SCB_AIRCR  (*(volatile uint32_t *)0xE000ED0CUL)
    SCB_AIRCR = (0x5FAUL << 16) | (3UL << 8);  /* PRIGROUP = 3 → 4-bit preemption */

    /* HSE tuning — must happen before radio PHY init.
       Default trim = 0x0C. Ideally read from OTP. */
    MOD_BITS(REG32(RCC_BASE + 0x210UL), 0x3FUL << 16, 0x0CUL << 16);

    /* Configure power supply as LDO (Core.W uses LDO, not SMPS).
       PWR_CR3 at offset 0x08, REGSEL bit 1: 0=LDO, 1=SMPS.
       Wait for REGS=0 in PWR_SVMSR (offset 0x3C) to confirm LDO active. */
    ll_rcc_ahb4_clk_enable(LL_AHB4_PWR);
    CLR_BITS(REG32(PWR_BASE_WBA + 0x08UL), (1UL << 1));  /* CR3.REGSEL = LDO */
    for (volatile uint32_t t = 0; t < 1000000; t++) {
        if (!(REG32(PWR_BASE_WBA + 0x3CUL) & (1UL << 1))) break;  /* SVMSR.REGS=0 */
    }

    /* Enable AHB5 clock for RADIO peripheral */
    ll_rcc_ahb5_clk_enable(LL_AHB5_RADIO);

    /* Enable radio power supply (PWR_VOSR RADIOSCEN) */
    ll_rcc_ahb4_clk_enable(LL_AHB4_PWR);
    SET_BITS(PWR_VOSR_REG, (1UL << 13));  /* RADIOSCEN */

    /* Select HSI as RNG clock source (default is LSE which isn't running) */
    /* RCC_CCIPR2 offset 0x0E4, RNGSEL bits [13:12]: 10 = HSI */
    MOD_BITS(REG32(RCC_BASE + 0x0E4UL), 0x3UL << 12, 0x2UL << 12);

    /* Enable hardware RNG */
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << 18));  /* RNG clock enable */
    (void)REG32(RCC_BASE);
    #define RNG_BASE_WBA  0x420C0800UL
    #define RNG_CR_REG    REG32(RNG_BASE_WBA + 0x00UL)
    #define RNG_SR_REG    REG32(RNG_BASE_WBA + 0x04UL)
    #define RNG_DR_REG    REG32(RNG_BASE_WBA + 0x08UL)
    /* RNG init: NIST config + CED disable + CONDRST (bit 30) */
    #define RNG_CR_NIST_VALUE  0x00F02D00UL
    #define RNG_CR_CONDRST     (1UL << 30)
    #define RNG_CR_CED         (1UL << 5)
    #define RNG_CR_RNGEN       (1UL << 2)
    /* Step 1: Write NIST config with CONDRST set */
    RNG_CR_REG = RNG_CR_NIST_VALUE | RNG_CR_CONDRST | RNG_CR_CED;
    /* Step 2: Clear CONDRST and enable RNG */
    RNG_CR_REG = RNG_CR_NIST_VALUE | RNG_CR_CED | RNG_CR_RNGEN;
    /* Don't wait for DRDY here — HW_RNG_Get will poll when needed */

    /* Configure RADIO and HASH interrupt priorities */
    hal_nvic_set_priority(HAL_IRQ_RADIO, 0);
    hal_nvic_enable_irq(HAL_IRQ_RADIO);
    hal_nvic_set_priority(HAL_IRQ_HASH, 5);
    hal_nvic_enable_irq(HAL_IRQ_HASH);

    /* Enable backup domain, start LSI, set radio sleep timer */
    ll_pwr_enable_backup_access();
    ll_rcc_lsi1_enable();
    while (!ll_rcc_lsi1_ready()) ;
    ll_rcc_set_radio_sleep_clk(LL_RCC_RADIOSLEEPSOURCE_LSI);
    ll_rcc_radio_slp_tmr_clk_enable();

    /* Initialize link layer platform clocks + enable baseband clock.
       The baseband clock MUST be enabled before BleStack_Init because
       ll_intf_init() reads radio registers immediately. */
    extern void LINKLAYER_PLAT_ClockInit(void);
    LINKLAYER_PLAT_ClockInit();
    extern void LINKLAYER_PLAT_AclkCtrl(uint8_t enable);
    LINKLAYER_PLAT_AclkCtrl(1);

    /* Zero the BLE buffers in SRAM2 (startup only zeros SRAM1 BSS) */
    extern uint32_t _sbss_ble, _ebss_ble;
    for (uint32_t *p = &_sbss_ble; p < &_ebss_ble; p++)
        *p = 0;

    /* Power table selection is done inside ll_sys_ble_cntrl_init
       (before ll_intf_init) — no need to do it here. */

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

    dbg_blestack_init_ret = BleStack_Init(&init_params);

    /* Set a random static BD address (upper 2 bits = 11, required by BLE spec) */
    {
        uint8_t bd_addr[6] = {0x01, 0xEF, 0xBE, 0x00, 0x55, 0xC0 | 0xC0};
        aci_hal_write_config_data(0x2E, 6, bd_addr);  /* offset 0x2E = random address */
    }

    /* Set TX power to maximum */
    aci_hal_set_tx_power_level(1, 0x19);

    /* Initialize GATT + GAP */
    dbg_gatt_init_ret = aci_gatt_init();
    dbg_gap_init_ret = aci_gap_init(0x01, 0x00, 16,
                 &gap_service_handle,
                 &gap_dev_name_handle,
                 &gap_appearance_handle);

    /* Register BLE host processing as a sequencer task.
       The BLE stack triggers this via UTIL_SEQ_SetTask internally. */
    extern void UTIL_SEQ_RegTask(uint32_t task_id_bm, uint32_t flags, void (*func)(void));
    extern void BleStackCB_Process(void);
    UTIL_SEQ_RegTask(1U << 4, 0, (void (*)(void))_ble_host_task);
}

/* BLE host background task — called via sequencer */
static void _ble_host_task(void)
{
    extern void BleStackCB_Process(void);
    if (BleStack_Process() == 0) {
        BleStackCB_Process();
    }
}

/* ============================================================
 * Process — call regularly from main loop
 * ============================================================ */

void hal_ble_process(void)
{
    /* Process BLE host stack + callbacks (matching reference pattern) */
    extern void BleStackCB_Process(void);
    if (BleStack_Process() == 0) {
        BleStackCB_Process();
    }

    /* Run pending sequencer tasks */
    extern void UTIL_SEQ_Run(uint32_t mask);
    UTIL_SEQ_Run(~0UL);
}

/* ============================================================
 * Advertise
 * ============================================================ */

hal_status_t hal_ble_advertise(const char *name)
{
    extern tBleStatus hci_le_set_advertising_parameters(
        uint16_t adv_interval_min, uint16_t adv_interval_max,
        uint8_t adv_type, uint8_t own_addr_type,
        uint8_t peer_addr_type, const uint8_t *peer_addr,
        uint8_t adv_channel_map, uint8_t adv_filter_policy);
    extern tBleStatus hci_le_set_advertising_data(uint8_t len, const uint8_t *data);
    extern tBleStatus hci_le_set_scan_response_data(uint8_t len, const uint8_t *data);
    extern tBleStatus hci_le_set_advertising_enable(uint8_t enable);

    /* ---- Bypass host stack — call link layer directly ---- */

    /* ll_intf functions use HCI-format parameters internally */
    extern uint8_t ll_intf_le_set_adv_params(uint16_t adv_interval_min,
        uint16_t adv_interval_max, uint8_t adv_type,
        uint8_t own_addr_type, uint8_t peer_addr_type,
        const uint8_t *peer_addr, uint8_t adv_channel_map,
        uint8_t adv_filter_policy);
    extern uint8_t ll_intf_le_set_adv_data(uint8_t len, const uint8_t *data);
    extern uint8_t ll_intf_le_set_adv_enable(uint8_t enable);
    extern uint8_t ll_intf_le_set_random_addr(const uint8_t *addr);

    /* Set a random static address */
    uint8_t bd_addr[6] = {0x01, 0xEF, 0xBE, 0x00, 0x55, 0xC0 | 0xC0};
    dbg_advertise_ret = ll_intf_le_set_random_addr(bd_addr);

    /* Advertising parameters: 100ms interval, ADV_IND, random address */
    dbg_advertise_ret = ll_intf_le_set_adv_params(
        0x00A0, 0x00A0,  /* 100ms interval */
        0x00,            /* ADV_IND */
        0x01,            /* own: random */
        0x00,            /* peer: public */
        (const uint8_t *)"\x00\x00\x00\x00\x00\x00",
        0x07,            /* all 3 channels */
        0x00             /* no filter */
    );

    /* Build advertising data */
    uint8_t name_len = 0;
    while (name[name_len] && name_len < 20) name_len++;

    uint8_t adv_data[31];
    uint8_t pos = 0;

    /* Flags: General Discoverable + BR/EDR Not Supported */
    adv_data[pos++] = 0x02;
    adv_data[pos++] = 0x01;
    adv_data[pos++] = 0x06;

    /* Complete Local Name */
    adv_data[pos++] = name_len + 1;
    adv_data[pos++] = 0x09;
    for (uint8_t i = 0; i < name_len; i++)
        adv_data[pos++] = (uint8_t)name[i];

    /* Manufacturer data */
    adv_data[pos++] = 0x05;
    adv_data[pos++] = 0xFF;
    adv_data[pos++] = 0x55;
    adv_data[pos++] = 0xBE;
    adv_data[pos++] = 0x42;
    adv_data[pos++] = 0x4C;

    dbg_gatt_init_ret = ll_intf_le_set_adv_data(pos, adv_data);

    /* Enable advertising — direct to link layer */
    dbg_scan_rsp_ret = ll_intf_le_set_adv_enable(0x01);

    return (dbg_scan_rsp_ret == 0) ? HAL_OK : HAL_ERROR;
}

hal_status_t hal_ble_stop_advertise(void)
{
    /* aci_gap_set_non_discoverable from ble_gap_aci.h */
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
 * BLE stack callback — called from BleStack_Process()
 * ============================================================ */

uint8_t BLECB_Indication(const uint8_t *data, uint16_t length,
                         const uint8_t *ext_data, uint16_t ext_length)
{
    (void)data;
    (void)length;
    (void)ext_data;
    (void)ext_length;
    /* TODO: parse HCI events for connect/disconnect */
    return 0;
}
