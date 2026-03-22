/**
 * ble_glue.c — Remaining stub implementations for BLE stack dependencies
 *
 * The real implementations of LINKLAYER_PLAT_*, ll_sys_if, UTIL_SEQ_*,
 * and HostStack/BleStackCB are now in their own files:
 *   linklayer_plat.c, ll_sys_if.c, stm32_seq.c, host_stack_if.c
 *
 * This file provides:
 *   - BLEPLAT_* (BLE platform adaptation layer)
 *   - ll_sys_* stubs that are NOT in ll_sys_if.c
 *   - UTIL_TIMER_* timer stubs
 *   - NVM/SNVMA/AMM/LST memory stubs
 *   - SCM/OTP/Flash/BPKA/FM stubs
 *   - HW_RNG_Get (hardware RNG access)
 *   - LINKLAYER_DEBUG_SIGNAL_* stubs
 *   - System stubs (_sbrk, etc.)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hal_common.h"
#include "app_conf.h"

/* ============================================================
 * BLEPLAT_* — BLE Platform Adaptation Layer
 * ============================================================ */

void BLEPLAT_Init(void)
{
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
 * HW_RNG_Get — Hardware RNG access for linklayer_plat.c
 * ============================================================ */

void HW_RNG_Get(uint8_t n, uint32_t *val)
{
    #define _RNG_BASE  0x420C0800UL
    #define _RNG_SR    (*(volatile uint32_t *)(_RNG_BASE + 0x04))
    #define _RNG_DR    (*(volatile uint32_t *)(_RNG_BASE + 0x08))

    for (uint8_t i = 0; i < n; i++)
    {
        while (!(_RNG_SR & 0x01)) ;  /* Wait for DRDY */
        val[i] = _RNG_DR;
    }
}

/* ============================================================
 * ll_sys_* — Link Layer system interface stubs
 *
 * NOTE: ll_sys_bg_process_init, ll_sys_schedule_bg_process,
 * ll_sys_schedule_bg_process_isr, ll_sys_config_params, and
 * ll_sys_reset are now in ll_sys_if.c
 * ============================================================ */

void ll_sys_init(void)
{
}

void ll_sys_delay_us(uint32_t delay)
{
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
}

void ll_sys_setup_radio_intr(void (*intr_cb)())
{
    extern void (*radio_callback)(void);
    radio_callback = intr_cb;
}

void ll_sys_setup_radio_sw_low_intr(void (*intr_cb)())
{
    extern void (*low_isr_callback)(void);
    low_isr_callback = intr_cb;
}

void ll_sys_radio_sw_low_intr_trigger(uint8_t priority)
{
    extern volatile uint8_t radio_sw_low_isr_is_running_high_prio;
    if (priority == 0)
    {
        radio_sw_low_isr_is_running_high_prio = 1;
        hal_nvic_set_priority(RADIO_SW_LOW_INTR_NUM, 0);
    }
    ll_nvic_set_pending(RADIO_SW_LOW_INTR_NUM);
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

void ll_sys_schldr_timing_update_not(void *p_evnt_timing)
{
    (void)p_evnt_timing;
}

void ll_sys_bg_process(void)
{
    /* Link layer background processing — called from sequencer */
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

/* Deep sleep — not used */
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

/* ============================================================
 * UTIL_TIMER_* — Timer management stubs
 * ============================================================ */

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
    return -1; /* fail */
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
 * BPKA — BLE PKA stubs
 * ============================================================ */

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

/* ll_sys_ble_cntrl_init is now in ll_sys_startup.c (ST's real implementation) */

/* HAL stubs */
void SystemInit(void)
{
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

/* missed_hci_event_flag — referenced by host_stack_if.c */
uint8_t missed_hci_event_flag = 0;
