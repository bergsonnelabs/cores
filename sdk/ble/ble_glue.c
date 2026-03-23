/**
 * ble_glue.c -- Remaining stub implementations for BLE stack dependencies
 *
 * Real implementations now live in their own files:
 *   bleplat.c       — BLEPLAT_* (BLE platform adaptation layer)
 *   ble_timer.c     — BLE_TIMER_* (BLE timer using UTIL_TIMER + AMM)
 *   stm32_timer.c   — UTIL_TIMER_* (timer server)
 *   advanced_memory_manager.c — AMM_* (advanced memory manager)
 *   stm32_mm.c      — UTIL_MM_* (basic memory manager)
 *   linklayer_plat.c — LINKLAYER_PLAT_* functions
 *   ll_sys_if.c     — ll_sys_bg_process_init, ll_sys_schedule_bg_process, etc.
 *   stm32_seq.c     — UTIL_SEQ_* sequencer
 *   host_stack_if.c — HostStack/BleStackCB
 *
 * This file provides:
 *   - ll_sys_* stubs that are NOT in ll_sys_if.c
 *   - NVM/SNVMA memory stubs
 *   - BAES/BPKA/crypto stubs
 *   - SCM/OTP/Flash/FM stubs
 *   - HW_RNG_Get (hardware RNG access)
 *   - AMM_RegisterBasicMemoryManager / AMM_ProcessRequest (glue for AMM)
 *   - LINKLAYER_DEBUG_SIGNAL_* stubs
 *   - System stubs (_sbrk, etc.)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hal_common.h"
#include "app_conf.h"
#include "stm32_mm.h"
#include "advanced_memory_manager.h"
#include "stm32_seq.h"

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
 * BAES — BLE AES stubs (fine for advertising)
 * ============================================================ */

void BAES_Reset(void)
{
}

void BAES_EcbCrypt(const uint8_t *key, const uint8_t *input,
                   uint8_t *output, int encrypt)
{
    (void)key; (void)input; (void)encrypt;
    memset(output, 0, 16);
}

void BAES_CmacSetKey(const uint8_t *key)
{
    (void)key;
}

void BAES_CmacCompute(const uint8_t *input, uint32_t input_length,
                      uint8_t *output_tag)
{
    (void)input; (void)input_length;
    if (output_tag)
        memset(output_tag, 0, 16);
}

int BAES_CcmCrypt(uint8_t mode, const uint8_t *key,
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

/* ============================================================
 * BPKA — BLE PKA stubs (fine for advertising)
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

void BPKA_BG_Process(void)
{
}

int BPKA_StartP256Key(const uint32_t *local_private_key)
{
    (void)local_private_key;
    return 0;
}

void BPKA_ReadP256Key(uint32_t *local_public_key)
{
    (void)local_public_key;
}

int BPKA_StartDhKey(const uint32_t *local_private_key,
                     const uint32_t *remote_public_key)
{
    (void)local_private_key; (void)remote_public_key;
    return 0;
}

int BPKA_ReadDhKey(uint32_t *dh_key)
{
    (void)dh_key;
    return 0;
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
    LINKLAYER_PLAT_ClockInit();
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
    LINKLAYER_PLAT_GetRNG(ptr_rnd, len);
}

void ll_sys_radio_ack_ctrl(uint8_t enable)
{
    LINKLAYER_PLAT_AclkCtrl(enable);
}

void ll_sys_radio_wait_for_busclkrdy(void)
{
    LINKLAYER_PLAT_WaitHclkRdy();
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
    if (start) {
        LINKLAYER_PLAT_StartRadioEvt();
    } else {
        LINKLAYER_PLAT_StopRadioEvt();
    }
}

void ll_sys_rco_clbr_not(uint8_t start)
{
    if (start) {
        LINKLAYER_PLAT_RCOStartClbr();
    } else {
        LINKLAYER_PLAT_RCOStopClbr();
    }
}

void ll_sys_request_temperature(void)
{
}

void ll_sys_schldr_timing_update_not(void *p_evnt_timing)
{
    (void)p_evnt_timing;
}

volatile uint32_t dbg_bg_process_called = 0;
volatile uint32_t dbg_bg_process_active = 0;
volatile uint32_t dbg_emngr_can_sleep = 0;

void ll_sys_bg_process(void)
{
    extern uint8_t emngr_can_mcu_sleep(void);
    extern void emngr_handle_all_events(void);
    extern void HostStack_Process(void);

    dbg_bg_process_called++;

    uint8_t can_sleep = emngr_can_mcu_sleep();
    dbg_emngr_can_sleep = can_sleep;

    if (can_sleep == 0)
    {
        dbg_bg_process_active++;
        ll_sys_dp_slp_exit();
        emngr_handle_all_events();
        HostStack_Process();
    }
    if (emngr_can_mcu_sleep() == 0)
    {
        ll_sys_schedule_bg_process();
    }
}

void ll_sys_enable_irq(void)
{
    LINKLAYER_PLAT_EnableIRQ();
}

void ll_sys_disable_irq(void)
{
    LINKLAYER_PLAT_DisableIRQ();
}

void ll_sys_enable_specific_irq(uint8_t isr_type)
{
    LINKLAYER_PLAT_EnableSpecificIRQ(isr_type);
}

void ll_sys_disable_specific_irq(uint8_t isr_type)
{
    LINKLAYER_PLAT_DisableSpecificIRQ(isr_type);
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
 * NVM / SNVMA — Non-volatile memory stubs
 * ============================================================ */

int NVM_Init(void *buf, uint32_t offset, uint32_t size)
{
    (void)buf; (void)offset; (void)size;
    return 0;
}

int NVM_Add(uint8_t type, const uint8_t *data, uint16_t size,
            const uint8_t *extra_data, uint16_t extra_size)
{
    (void)type; (void)data; (void)size;
    (void)extra_data; (void)extra_size;
    return 0;
}

int NVM_Get(int mode, uint8_t type, uint16_t offset,
            uint8_t *data, uint16_t size)
{
    (void)mode; (void)type; (void)offset; (void)data; (void)size;
    return -1; /* EOF */
}

int NVM_Compare(uint16_t offset, const uint8_t *data, uint16_t size)
{
    (void)offset; (void)data; (void)size;
    return 1; /* comparison failed (no NVM) */
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
 * AMM glue — AMM_RegisterBasicMemoryManager / AMM_ProcessRequest
 *
 * These are called by advanced_memory_manager.c. We wire them
 * to UTIL_MM (stm32_mm.c) and UTIL_SEQ (stm32_seq.c).
 * ============================================================ */

static void AMM_WrapperInit(uint32_t * const p_PoolAddr, const uint32_t PoolSize)
{
    UTIL_MM_Init((uint8_t *)p_PoolAddr, ((size_t)PoolSize * sizeof(uint32_t)));
}

static uint32_t * AMM_WrapperAllocate(const uint32_t BufferSize)
{
    return (uint32_t *)UTIL_MM_GetBuffer(((size_t)BufferSize * sizeof(uint32_t)));
}

static void AMM_WrapperFree(uint32_t * const p_BufferAddr)
{
    UTIL_MM_ReleaseBuffer((void *)p_BufferAddr);
}

void AMM_RegisterBasicMemoryManager(AMM_BasicMemoryManagerFunctions_t * const p_BasicMemoryManagerFunctions)
{
    p_BasicMemoryManagerFunctions->Init = AMM_WrapperInit;
    p_BasicMemoryManagerFunctions->Allocate = AMM_WrapperAllocate;
    p_BasicMemoryManagerFunctions->Free = AMM_WrapperFree;
}

void AMM_ProcessRequest(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_AMM, CFG_SEQ_PRIO_0);
}

/* ============================================================
 * AMM initialization — called from hal_ble.c or startup
 * ============================================================ */

static uint32_t AMM_Pool[CFG_AMM_POOL_SIZE];
static AMM_VirtualMemoryConfig_t vmConfig[CFG_AMM_VIRTUAL_MEMORY_NUMBER] =
{
    {
        .Id = CFG_AMM_VIRTUAL_STACK_BLE,
        .BufferSize = CFG_AMM_VIRTUAL_STACK_BLE_BUFFER_SIZE
    },
    {
        .Id = CFG_AMM_VIRTUAL_APP_BLE,
        .BufferSize = CFG_AMM_VIRTUAL_APP_BLE_BUFFER_SIZE
    },
};

static AMM_InitParameters_t ammInitConfig =
{
    .p_PoolAddr = AMM_Pool,
    .PoolSize = CFG_AMM_POOL_SIZE,
    .VirtualMemoryNumber = CFG_AMM_VIRTUAL_MEMORY_NUMBER,
    .p_VirtualMemoryConfigList = vmConfig
};

void ble_amm_init(void)
{
    AMM_Init(&ammInitConfig);
    UTIL_SEQ_RegTask(1U << CFG_TASK_AMM, UTIL_SEQ_RFU, AMM_BackgroundProcess);
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
