/**
 * ble_glue.c — Stub implementations for BLE stack dependencies
 *
 * The ST BLE stack binary (stm32wba_ble_stack_basic.a) and link layer
 * binary (LinkLayer_BLE_Basic_lib.a) expect many platform functions.
 * This file provides minimal stubs to get the build to link.
 *
 * Functions are grouped by subsystem. Stubs return success/0 where
 * appropriate. Critical functions will be implemented as needed.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hal_common.h"

/* CMSIS intrinsics for interrupt control */
static inline uint32_t __get_PRIMASK(void) {
    uint32_t r;
    __asm volatile ("MRS %0, primask" : "=r" (r));
    return r;
}
static inline void __set_PRIMASK(uint32_t v) {
    __asm volatile ("MSR primask, %0" :: "r" (v) : "memory");
}
static inline void __disable_irq(void) {
    __asm volatile ("cpsid i" ::: "memory");
}
static inline void __enable_irq(void) {
    __asm volatile ("cpsie i" ::: "memory");
}
static inline uint32_t __get_BASEPRI(void) {
    uint32_t r;
    __asm volatile ("MRS %0, basepri" : "=r" (r));
    return r;
}
static inline void __set_BASEPRI(uint32_t v) {
    __asm volatile ("MSR basepri, %0" :: "r" (v) : "memory");
}
static inline void __set_BASEPRI_MAX(uint32_t v) {
    __asm volatile ("MSR basepri_max, %0" :: "r" (v) : "memory");
}

/* Forward declarations for callbacks */
extern void (*radio_callback)(void);
extern void (*low_isr_callback)(void);
extern volatile uint8_t radio_sw_low_isr_is_running_high_prio;

/* ============================================================
 * BLEPLAT_* — BLE Platform Adaptation Layer
 * ============================================================ */

void BLEPLAT_Init(void)
{
    /* Nothing needed for minimal bring-up */
}

int BLEPLAT_NvmAdd(uint8_t type, const uint8_t *data, uint16_t size,
                   const uint8_t *extra_data, uint16_t extra_size)
{
    (void)type; (void)data; (void)size;
    (void)extra_data; (void)extra_size;
    return 0; /* BLEPLAT_OK */
}

int BLEPLAT_NvmGet(uint8_t mode, uint8_t type, uint16_t offset,
                   uint8_t *data, uint16_t size)
{
    (void)mode; (void)type; (void)offset; (void)data; (void)size;
    return -3; /* BLEPLAT_EOF */
}

int BLEPLAT_NvmCompare(uint16_t offset, const uint8_t *data, uint16_t size)
{
    (void)offset; (void)data; (void)size;
    return 1; /* comparison failed (no NVM) */
}

void BLEPLAT_NvmDiscard(uint8_t mode)
{
    (void)mode;
}

/* PKA — stub out public key operations */
int BLEPLAT_PkaStartP256Key(const uint32_t *local_private_key)
{
    (void)local_private_key;
    return 0;
}

void BLEPLAT_PkaReadP256Key(uint32_t *local_public_key)
{
    (void)local_public_key;
}

int BLEPLAT_PkaStartDhKey(const uint32_t *local_private_key,
                           const uint32_t *remote_public_key)
{
    (void)local_private_key; (void)remote_public_key;
    return 0;
}

int BLEPLAT_PkaReadDhKey(uint32_t *dh_key)
{
    (void)dh_key;
    return 0;
}

/* AES — stub out encryption */
void BLEPLAT_AesEcbEncrypt(const uint8_t *key, const uint8_t *input,
                           uint8_t *output)
{
    (void)key; (void)input;
    memset(output, 0, 16);
}

void BLEPLAT_AesCmacSetKey(const uint8_t *key)
{
    (void)key;
}

void BLEPLAT_AesCmacCompute(const uint8_t *input, uint32_t input_length,
                            uint8_t *output_tag)
{
    (void)input; (void)input_length;
    if (output_tag)
        memset(output_tag, 0, 16);
}

int BLEPLAT_AesCcmCrypt(uint8_t mode, const uint8_t *key,
                         uint8_t iv_length, const uint8_t *iv,
                         uint16_t add_length, const uint8_t *add,
                         uint32_t input_length, const uint8_t *input,
                         uint8_t tag_length, uint8_t *tag,
                         uint8_t *output)
{
    (void)mode; (void)key; (void)iv_length; (void)iv;
    (void)add_length; (void)add; (void)input_length; (void)input;
    (void)tag_length; (void)tag; (void)output;
    return 0;
}

/* RNG */
void BLEPLAT_RngGet(uint8_t n, uint32_t *val)
{
    /* Minimal PRNG using SysTick — NOT cryptographically secure,
       but enough for link-time bring-up */
    extern volatile uint32_t _systick_ticks;
    uint32_t seed = _systick_ticks;
    for (uint8_t i = 0; i < n; i++)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        val[i] = seed;
    }
}

/* Timer */
uint8_t BLEPLAT_TimerStart(uint16_t id, uint32_t timeout)
{
    (void)id; (void)timeout;
    return 0;
}

void BLEPLAT_TimerStop(uint16_t id)
{
    (void)id;
}

/* ============================================================
 * ll_sys_* — Link Layer system interface
 * ============================================================ */

void ll_sys_init(void)
{
}

void ll_sys_reset(void)
{
}

void ll_sys_delay_us(uint32_t delay)
{
    /* Crude busy wait — assumes ~100MHz */
    volatile uint32_t count = delay * 25;
    while (count--)
        ;
}

void ll_sys_assert(uint8_t condition)
{
    if (!condition)
    {
        while (1)
            ;
    }
}

void ll_sys_get_rng(uint8_t *ptr_rnd, uint32_t len)
{
    extern volatile uint32_t _systick_ticks;
    uint32_t seed = _systick_ticks;
    for (uint32_t i = 0; i < len; i++)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        ptr_rnd[i] = (uint8_t)(seed & 0xFF);
    }
}

void ll_sys_radio_ack_ctrl(uint8_t enable)
{
    (void)enable;
}

void ll_sys_radio_wait_for_busclkrdy(void)
{
    /* Wait for AHB5 bus clock ready — simplified busy-wait */
}

void ll_sys_setup_radio_intr(void (*intr_cb)())
{
    radio_callback = intr_cb;
}

void ll_sys_setup_radio_sw_low_intr(void (*intr_cb)())
{
    low_isr_callback = intr_cb;
}

void ll_sys_radio_sw_low_intr_trigger(uint8_t priority)
{
    if (priority == 0)
    {
        /* High priority: elevate HASH IRQ priority temporarily */
        radio_sw_low_isr_is_running_high_prio = 1;
        hal_nvic_set_priority(HAL_IRQ_HASH, 0);
    }
    /* Trigger the HASH IRQ (used as SW low ISR) */
    ll_nvic_set_pending(HAL_IRQ_HASH);
}

void ll_sys_radio_evt_not(uint8_t start)
{
    (void)start;
}

void ll_sys_rco_clbr_not(uint8_t start)
{
    (void)start;
}

void ll_sys_request_temperature(void)
{
}

/* Evnt_timing_t forward declaration — from evnt_schdlr_gnrc_if.h */
struct Evnt_timing_s;
void ll_sys_schldr_timing_update_not(struct Evnt_timing_s *p_evnt_timing)
{
    (void)p_evnt_timing;
}

void ll_sys_bg_process(void)
{
    /* Link layer background processing — called from sequencer */
}

void ll_sys_bg_process_init(void)
{
    /* Register link layer background process as sequencer task */
    extern void UTIL_SEQ_RegTask(uint32_t task_id_bm, uint32_t flags, void (*func)(void));
    UTIL_SEQ_RegTask(1U << 1, 0, ll_sys_bg_process);  /* CFG_TASK_LINK_LAYER = 1 */
}

void ll_sys_schedule_bg_process(void)
{
    /* Trigger link layer background task via sequencer */
    extern void UTIL_SEQ_SetTask(uint32_t task_id_bm, uint32_t prio);
    UTIL_SEQ_SetTask(1U << 1, 0);  /* CFG_TASK_LINK_LAYER = 1 */
}

void ll_sys_schedule_bg_process_isr(void)
{
}

void ll_sys_config_params(void)
{
    /* Configure link layer context: SW low ISR, schedule from ISR */
    extern void ll_intf_cmn_config_ll_ctx_params(uint8_t use_low_isr, uint8_t next_event_from_isr);
    ll_intf_cmn_config_ll_ctx_params(1, 1);

    /* Select TX power table (0 = max power table) */
    extern void ll_intf_cmn_select_tx_power_table(uint8_t table_id);
    ll_intf_cmn_select_tx_power_table(0);
}

void ll_sys_enable_irq(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

void ll_sys_disable_irq(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

void ll_sys_enable_specific_irq(uint8_t isr_type)
{
    (void)isr_type;
    ll_sys_enable_irq();
}

void ll_sys_disable_specific_irq(uint8_t isr_type)
{
    (void)isr_type;
    ll_sys_disable_irq();
}

void ll_sys_enable_os_context_switch(void)
{
}

void ll_sys_disable_os_context_switch(void)
{
}

/* Deep sleep — not used in beacon mode */
int ll_sys_dp_slp_init(void)
{
    return 0;
}

int ll_sys_dp_slp_enter(uint32_t dp_slp_duration)
{
    (void)dp_slp_duration;
    return 0;
}

int ll_sys_dp_slp_exit(void)
{
    return 0;
}

int ll_sys_dp_slp_get_state(void)
{
    return 0; /* LL_SYS_DP_SLP_DISABLED */
}

void ll_sys_dp_slp_wakeup_evt_clbk(void const *ptr_arg)
{
    (void)ptr_arg;
}

uint8_t ll_sys_get_concurrent_state_machines_num(void)
{
    return 1;
}

/* ll_intf_cmn_get_slptmr_value and ll_intf_is_ptr_in_ble_mem are
   provided by LinkLayer_BLE_Basic_lib.a (bsp.o) */

/* ============================================================
 * HostStack_Process — called by link layer to process host events
 * ============================================================ */

void HostStack_Process(void)
{
    /* Forward to BleStack_Process — already called from hal_ble_process() */
}

/* ============================================================
 * BleStackCB_Process — notification from BLE stack that events are pending
 * ============================================================ */

void BleStackCB_Process(void)
{
    /* Trigger the BLE host processing task via the sequencer */
    extern void UTIL_SEQ_SetTask(uint32_t task_id_bm, uint32_t prio);
    UTIL_SEQ_SetTask(1U << 4, 0);  /* CFG_TASK_BLE_HOST = 4 */
}

/* ============================================================
 * UTIL_SEQ_* — Minimal cooperative task sequencer
 *
 * The BLE stack registers tasks via RegTask and triggers them via
 * SetTask. Our main loop calls UTIL_SEQ_Run to execute pending tasks.
 * ============================================================ */

#define SEQ_MAX_TASKS  32

static void (*_seq_tasks[SEQ_MAX_TASKS])(void);
static volatile uint32_t _seq_pending = 0;
static volatile uint32_t _seq_evt = 0;

void UTIL_SEQ_RegTask(uint32_t task_id_bm, uint32_t flags, void (*func)(void))
{
    (void)flags;
    for (int i = 0; i < SEQ_MAX_TASKS; i++) {
        if (task_id_bm & (1UL << i)) {
            _seq_tasks[i] = func;
            break;
        }
    }
}

void UTIL_SEQ_SetTask(uint32_t task_id_bm, uint32_t prio)
{
    (void)prio;
    _seq_pending |= task_id_bm;
}

void UTIL_SEQ_Run(uint32_t mask_bm)
{
    uint32_t pending = _seq_pending & mask_bm;
    while (pending) {
        for (int i = 0; i < SEQ_MAX_TASKS; i++) {
            if ((pending & (1UL << i)) && _seq_tasks[i]) {
                _seq_pending &= ~(1UL << i);
                _seq_tasks[i]();
            }
        }
        pending = _seq_pending & mask_bm;
    }
}

void UTIL_SEQ_SetEvt(uint32_t evt_id_bm)
{
    _seq_evt |= evt_id_bm;
}

void UTIL_SEQ_WaitEvt(uint32_t evt_id_bm)
{
    while (!(_seq_evt & evt_id_bm))
        ;
    _seq_evt &= ~evt_id_bm;
}

void UTIL_SEQ_ClrEvt(uint32_t evt_id_bm)
{
    _seq_evt &= ~evt_id_bm;
}

void UTIL_SEQ_EvtIdle(uint32_t task_id_bm, uint32_t evt_waited_bm)
{
    (void)task_id_bm; (void)evt_waited_bm;
}

void UTIL_SEQ_Idle(void)
{
}

void UTIL_SEQ_PreIdle(void)
{
}

void UTIL_SEQ_PostIdle(void)
{
}

/* ============================================================
 * UTIL_TIMER_* — Timer management stubs
 * ============================================================ */

typedef struct {
    void *next;
    void (*callback)(void *);
    void *arg;
    uint32_t timeout;
} UTIL_TIMER_Object_t;

int UTIL_TIMER_Create(void *timer, uint32_t period, int mode,
                      void (*callback)(void *), void *arg)
{
    (void)timer; (void)period; (void)mode; (void)callback; (void)arg;
    return 0;
}

int UTIL_TIMER_Start(void *timer)
{
    (void)timer;
    return 0;
}

int UTIL_TIMER_StartWithPeriod(void *timer, uint32_t period)
{
    (void)timer; (void)period;
    return 0;
}

int UTIL_TIMER_Stop(void *timer)
{
    (void)timer;
    return 0;
}

int UTIL_TIMER_SetPeriod(void *timer, uint32_t period)
{
    (void)timer; (void)period;
    return 0;
}

uint32_t UTIL_TIMER_GetCurrentTime(void)
{
    extern volatile uint32_t _systick_ticks;
    return _systick_ticks;
}

uint32_t UTIL_TIMER_GetElapsedTime(uint32_t past)
{
    extern volatile uint32_t _systick_ticks;
    return _systick_ticks - past;
}

/* ============================================================
 * NVM / SNVMA — Non-volatile memory stubs
 * ============================================================ */

int NVM_Init(void *buf, uint32_t offset, uint32_t size)
{
    (void)buf; (void)offset; (void)size;
    return 0;
}

int NVM_Get(int mode, uint8_t type, uint16_t offset,
            uint8_t *data, uint16_t size)
{
    (void)mode; (void)type; (void)offset; (void)data; (void)size;
    return -1; /* EOF */
}

void NVM_Discard(int mode)
{
    (void)mode;
}

void SNVMA_Register(uint32_t id, uint32_t *buf, uint32_t size)
{
    (void)id; (void)buf; (void)size;
}

int SNVMA_Restore(uint32_t id)
{
    (void)id;
    return 0;
}

int SNVMA_Write(uint32_t id, void *cb)
{
    (void)id; (void)cb;
    return 0;
}

/* ============================================================
 * AMM — Advanced Memory Manager stubs
 * ============================================================ */

int AMM_Init(void *cfg)
{
    (void)cfg;
    return 0;
}

int AMM_Alloc(uint32_t id, uint32_t size, uint32_t **ptr, void *cb)
{
    (void)id; (void)size; (void)ptr; (void)cb;
    return -1; /* fail — not implemented */
}

void AMM_Free(uint32_t *ptr)
{
    (void)ptr;
}

int AMM_BackgroundProcess(void)
{
    return 0;
}

/* ============================================================
 * Linked list — minimal stubs used by BLE event queue
 * ============================================================ */

typedef struct list_node {
    struct list_node *next;
} tListNode;

void LST_init_head(tListNode *head)
{
    head->next = head;
}

uint8_t LST_is_empty(tListNode *head)
{
    return (head->next == head) ? 1 : 0;
}

void LST_insert_tail(tListNode *head, tListNode *node)
{
    /* Simple append — walk to end */
    tListNode *p = head;
    while (p->next != head)
        p = p->next;
    p->next = node;
    node->next = head;
}

void LST_remove_head(tListNode *head, tListNode **node)
{
    if (head->next != head) {
        *node = head->next;
        head->next = (*node)->next;
    } else {
        *node = NULL;
    }
}

void LST_insert_head(tListNode *head, tListNode *node)
{
    node->next = head->next;
    head->next = node;
}

void LST_remove_node(tListNode *node)
{
    /* Not properly implemented — would need prev pointer */
    (void)node;
}

void LST_get_next_node(tListNode *node, tListNode **next)
{
    *next = node->next;
}

int LST_get_size(tListNode *head)
{
    int count = 0;
    tListNode *p = head->next;
    while (p != head) {
        count++;
        p = p->next;
    }
    return count;
}

/* ============================================================
 * SCM — System Clock Manager stubs
 * ============================================================ */

void scm_hserdy_isr(void)
{
}

void scm_setup(void)
{
}

void scm_init(void)
{
}

void scm_pll_config(void)
{
}

void scm_setsystemclock(uint32_t clock)
{
    (void)clock;
}

uint32_t scm_getspeedstatus(void)
{
    return 0;
}

void scm_setwaitstates(uint32_t ws)
{
    (void)ws;
}

void scm_notifyradioalivestatus(uint8_t status)
{
    (void)status;
}

/* ============================================================
 * OTP — One-Time Programmable memory stubs
 * ============================================================ */

void *OTP_Read(uint32_t id)
{
    (void)id;
    return NULL;
}

/* ============================================================
 * Flash driver stubs
 * ============================================================ */

int FD_WriteData(uint32_t dest, uint64_t *src, uint32_t n_dwords)
{
    (void)dest; (void)src; (void)n_dwords;
    return 0;
}

int FD_EraseSectors(uint32_t first_sector, uint32_t n_sectors)
{
    (void)first_sector; (void)n_sectors;
    return 0;
}

/* ============================================================
 * Additional stubs that may be needed
 * ============================================================ */

/* BPKA — BLE PKA stubs */
void BPKA_Init(void)
{
}

int BPKA_IsReady(void)
{
    return 1;
}

void BPKA_Reset(void)
{
}

/* Flash manager */
void FM_Init(void)
{
}

int FM_BackgroundProcess(void)
{
    return 0;
}

/* Debug */
void APP_DBG_MSG(const char *fmt, ...)
{
    (void)fmt;
}

/* Low power manager stubs */
void UTIL_LPM_Init(void)
{
}

void UTIL_LPM_SetStopMode(uint32_t lpm_id, uint32_t stop_mode)
{
    (void)lpm_id; (void)stop_mode;
}

void UTIL_LPM_SetOffMode(uint32_t lpm_id, uint32_t off_mode)
{
    (void)lpm_id; (void)off_mode;
}

void UTIL_LPM_EnterLowPower(void)
{
}

/* ============================================================
 * LINKLAYER_PLAT_* — Link Layer platform interface
 *
 * These functions are called by the link layer binary to control
 * the radio hardware. Register addresses from STM32WBA55 RM0493.
 * ============================================================ */

/* ---- WBA55 radio registers via ll_rcc.h ---- */
#include "ll_rcc.h"

/* RCC_RADIOENR: offset 0x208 — BBCLKEN bit 1 */
#define _RCC_RADIOENR      REG32(RCC_BASE + 0x208UL)
#define _RADIOENR_BBCLKEN  (1UL << 1)

/* RCC_AHB5SMENR: offset 0x0D8 — RADIOSMEN bit 0 */
#define _RCC_AHB5SMENR     REG32(RCC_BASE + 0x0D8UL)

/* Interrupt nesting counters (matching reference implementation) */
static uint32_t _primask_bit = 0;
static volatile int32_t _irq_counter = 0;
static volatile int32_t _prio_high_isr_counter = 0;
static volatile int32_t _prio_low_isr_counter = 0;
static volatile int32_t _prio_sys_isr_counter = 0;
static volatile uint32_t _local_basepri = 0;

/* AHB5 bus clock tracking */
static uint8_t _ahb5_switched_off = 0;
static uint32_t _radio_sleep_timer_val = 0;

/* Forward decl — provided by LinkLayer_BLE_Basic_lib.a */
extern uint32_t ll_intf_cmn_get_slptmr_value(void);

void LINKLAYER_PLAT_ClockInit(void)
{
    /* Radio sleep timer clock source should already be set by hal_ble_init.
       Verify it's not NONE, fallback to HSE/1024. */
    if (ll_rcc_get_radio_sleep_clk() == LL_RCC_RADIOSLEEPSOURCE_NONE) {
        ll_pwr_enable_backup_access();
        ll_rcc_set_radio_sleep_clk(LL_RCC_RADIOSLEEPSOURCE_HSE_DIV);
    }

    /* Enable AHB5 clock for RADIO peripheral */
    ll_rcc_ahb5_clk_enable(LL_AHB5_RADIO);
}

void LINKLAYER_PLAT_AclkCtrl(uint8_t enable)
{
    if (enable) {
        /* Enable radio baseband clock */
        SET_BITS(_RCC_RADIOENR, _RADIOENR_BBCLKEN);

        /* Wait for HSE to be ready (radio needs it) */
        while (!ll_rcc_hse_ready())
            ;
    } else {
        /* Disable radio baseband clock */
        CLR_BITS(_RCC_RADIOENR, _RADIOENR_BBCLKEN);
    }
}

void LINKLAYER_PLAT_WaitHclkRdy(void)
{
    if (_ahb5_switched_off) {
        _ahb5_switched_off = 0;
        /* Wait until sleep timer value changes (indicates AHB5 is clocked) */
        while (_radio_sleep_timer_val == ll_intf_cmn_get_slptmr_value())
            ;
    }
}

void LINKLAYER_PLAT_DisableIRQ(void)
{
    if (_irq_counter == 0)
        _primask_bit = __get_PRIMASK();
    __disable_irq();
    _irq_counter++;
}

void LINKLAYER_PLAT_EnableIRQ(void)
{
    _irq_counter--;
    if (_irq_counter <= 0) {
        _irq_counter = 0;
        __set_PRIMASK(_primask_bit);
    }
}

void LINKLAYER_PLAT_DisableSpecificIRQ(uint8_t isr_type)
{
    if (isr_type & 0x01) {  /* LL_HIGH_ISR_ONLY */
        _prio_high_isr_counter++;
        if (_prio_high_isr_counter == 1)
            hal_nvic_disable_irq(HAL_IRQ_RADIO);
    }
    if (isr_type & 0x02) {  /* LL_LOW_ISR_ONLY */
        _prio_low_isr_counter++;
        if (_prio_low_isr_counter == 1)
            hal_nvic_disable_irq(HAL_IRQ_HASH);
    }
    if (isr_type & 0x04) {  /* SYS_LOW_ISR */
        _prio_sys_isr_counter++;
        if (_prio_sys_isr_counter == 1) {
            _local_basepri = __get_BASEPRI();
            __set_BASEPRI_MAX(4 << 4);  /* Mask below radio low priority */
        }
    }
}

void LINKLAYER_PLAT_EnableSpecificIRQ(uint8_t isr_type)
{
    if (isr_type & 0x01) {
        _prio_high_isr_counter--;
        if (_prio_high_isr_counter == 0)
            hal_nvic_enable_irq(HAL_IRQ_RADIO);
    }
    if (isr_type & 0x02) {
        _prio_low_isr_counter--;
        if (_prio_low_isr_counter == 0)
            hal_nvic_enable_irq(HAL_IRQ_HASH);
    }
    if (isr_type & 0x04) {
        _prio_sys_isr_counter--;
        if (_prio_sys_isr_counter == 0)
            __set_BASEPRI(_local_basepri);
    }
}

void LINKLAYER_PLAT_DisableOsContextSwitch(void)
{
}

void LINKLAYER_PLAT_EnableOsContextSwitch(void)
{
}

void LINKLAYER_PLAT_Assert(uint8_t condition)
{
    if (!condition)
        while (1) ;
}

void LINKLAYER_PLAT_DelayUs(uint32_t delay)
{
    volatile uint32_t count = delay * 8;  /* ~32MHz */
    while (count--) ;
}

void LINKLAYER_PLAT_GetRNG(uint8_t *ptr_rnd, uint32_t len)
{
    /* Use hardware RNG if available */
    #define _RNG_BASE  0x420C0800UL
    #define _RNG_SR    (*(volatile uint32_t *)(_RNG_BASE + 0x04))
    #define _RNG_DR    (*(volatile uint32_t *)(_RNG_BASE + 0x08))

    uint32_t remaining = len;
    while (remaining >= 4) {
        while (!(_RNG_SR & 0x01)) ;  /* Wait for DRDY */
        uint32_t rng_val = _RNG_DR;
        memcpy(ptr_rnd + (len - remaining), &rng_val, 4);
        remaining -= 4;
    }
    if (remaining > 0) {
        while (!(_RNG_SR & 0x01)) ;
        uint32_t rng_val = _RNG_DR;
        memcpy(ptr_rnd + (len - remaining), &rng_val, remaining);
    }
}

void LINKLAYER_PLAT_SetupRadioIT(void (*cb)(void))
{
    extern void (*radio_callback)(void);
    radio_callback = cb;
    hal_nvic_set_priority(HAL_IRQ_RADIO, 0);
    hal_nvic_enable_irq(HAL_IRQ_RADIO);
}

void LINKLAYER_PLAT_SetupSwLowIT(void (*cb)(void))
{
    extern void (*low_isr_callback)(void);
    low_isr_callback = cb;
    hal_nvic_set_priority(HAL_IRQ_HASH, 4);
    hal_nvic_enable_irq(HAL_IRQ_HASH);
}

void LINKLAYER_PLAT_TriggerSwLowIT(uint8_t priority)
{
    extern volatile uint8_t radio_sw_low_isr_is_running_high_prio;
    uint8_t low_prio = 4;

    if (priority == 0) {
        low_prio = 4;
    } else {
        radio_sw_low_isr_is_running_high_prio = 1;
    }

    hal_nvic_set_priority(HAL_IRQ_HASH, low_prio);
    ll_nvic_set_pending(HAL_IRQ_HASH);
}

void LINKLAYER_PLAT_EnableRadioIT(void)
{
    hal_nvic_enable_irq(HAL_IRQ_RADIO);
}

void LINKLAYER_PLAT_DisableRadioIT(void)
{
    hal_nvic_disable_irq(HAL_IRQ_RADIO);
}

void LINKLAYER_PLAT_StartRadioEvt(void)
{
    /* Enable radio clock in sleep mode + set high priority */
    _RCC_AHB5SMENR |= (1UL << 0);  /* RADIOSMEN */
    hal_nvic_set_priority(HAL_IRQ_RADIO, 0);
}

void LINKLAYER_PLAT_StopRadioEvt(void)
{
    /* Disable radio clock in sleep mode + lower priority */
    _RCC_AHB5SMENR &= ~(1UL << 0);
    hal_nvic_set_priority(HAL_IRQ_RADIO, 4);
}

void LINKLAYER_PLAT_RCOStartClbr(void)
{
    /* RCO calibration needs HSE — ensure it's running */
    while (!ll_rcc_hse_ready()) ;  /* Wait HSERDY */
}

void LINKLAYER_PLAT_RCOStopClbr(void)
{
}

void LINKLAYER_PLAT_RadioEvtNot(uint8_t start)
{
    (void)start;
}

void LINKLAYER_PLAT_RcoClbrNot(uint8_t start)
{
    (void)start;
}

void LINKLAYER_PLAT_RequestTemperature(void)
{
}

void LINKLAYER_PLAT_NotifyWFIEnter(void)
{
    if (ll_pwr_get_radio_mode() != LL_PWR_RADIO_ACTIVE_MODE) {
        _ahb5_switched_off = 1;
    }
}

void LINKLAYER_PLAT_NotifyWFIExit(void)
{
    if (_ahb5_switched_off) {
        _radio_sleep_timer_val = ll_intf_cmn_get_slptmr_value();
    }
}

void LINKLAYER_PLAT_SCHLDR_TIMING_UPDATE_NOT(uint32_t *p)
{
    (void)p;
}

uint32_t LINKLAYER_PLAT_GetSTCompanyID(void)
{
    /* STM32 UID96 register — use first word as company ID */
    return *(volatile uint32_t *)0x0BFA0700UL & 0x00FFFFFF;
}

uint32_t LINKLAYER_PLAT_GetUDN(void)
{
    /* STM32 UID96 — use second word as unique device number */
    return *(volatile uint32_t *)0x0BFA0704UL;
}

/* ============================================================
 * LINKLAYER_DEBUG_SIGNAL_* — Debug signal stubs
 * ============================================================ */

void LINKLAYER_DEBUG_SIGNAL_SET(uint32_t signal)
{
    (void)signal;
}

void LINKLAYER_DEBUG_SIGNAL_RESET(uint32_t signal)
{
    (void)signal;
}

void LINKLAYER_DEBUG_SIGNAL_TOGGLE(uint32_t signal)
{
    (void)signal;
}

/* ============================================================
 * ll_sys_ble_cntrl_init — BLE controller init (called by stack)
 * ============================================================ */

void ll_sys_ble_cntrl_init(void)
{
    /* Called by the stack to initialize the link layer controller.
       In the reference project this sets up SCM, configures
       radio clock, etc. For minimal bring-up, do nothing. */
}

/* HAL stubs that the stack may reference */
void SystemInit(void)
{
    /* Already handled by tile_init */
}

/* Weak _sbrk for malloc (nano.specs) */
void *_sbrk(int incr)
{
    extern char end;
    static char *heap_end = 0;
    if (heap_end == 0)
        heap_end = &end;
    char *prev = heap_end;
    heap_end += incr;
    return prev;
}
