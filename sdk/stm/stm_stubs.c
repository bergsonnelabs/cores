/**
 * stm_stubs.c — Stub implementations for ST middleware symbols
 *
 * Some ST middleware files reference macros (UTILS_ENTER_CRITICAL_SECTION, etc.)
 * that should be defined via utilities_conf.h but aren't always included.
 * When the preprocessor doesn't expand them, the linker sees them as undefined
 * function calls. We provide weak stub implementations here.
 */

#include <stdint.h>

/* Critical section stubs — these should normally be macros in utilities_conf.h
   but some ST middleware .c files don't include it consistently. */
__attribute__((weak)) void UTILS_ENTER_CRITICAL_SECTION(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

__attribute__((weak)) void UTILS_EXIT_CRITICAL_SECTION(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

/* Debug signal stubs */
__attribute__((weak)) void SYSTEM_DEBUG_SIGNAL_SET(uint32_t signal)
{
    (void)signal;
}

__attribute__((weak)) void SYSTEM_DEBUG_SIGNAL_RESET(uint32_t signal)
{
    (void)signal;
}

/* assert_param — normally defined in stm32_assert.h as a macro */
__attribute__((weak)) void assert_param(uint32_t expr)
{
    (void)expr;
}

/* UNUSED macro — normally a macro, stub if used as function */
__attribute__((weak)) void UNUSED(uint32_t x)
{
    (void)x;
}

/* UTIL_PowerDriver — low power driver interface.
   Since we don't use low power (CFG_LPM_LEVEL == 0), provide empty stubs. */
typedef struct {
    void (*Init)(void);
    void (*DeInit)(void);
    void (*EnterSleepMode)(void);
    void (*ExitSleepMode)(void);
    void (*EnterStopMode)(void);
    void (*ExitStopMode)(void);
    void (*EnterOffMode)(void);
    void (*ExitOffMode)(void);
} LPM_Driver_t;

static void lpm_stub(void) {}

const LPM_Driver_t UTIL_PowerDriver = {
    .Init = lpm_stub,
    .DeInit = lpm_stub,
    .EnterSleepMode = lpm_stub,
    .ExitSleepMode = lpm_stub,
    .EnterStopMode = lpm_stub,
    .ExitStopMode = lpm_stub,
    .EnterOffMode = lpm_stub,
    .ExitOffMode = lpm_stub,
};

/* UTIL_TraceDriver now provided by adv_trace_usart_if.c */

/* UTIL_SYSTIMDriver — system time driver interface stub */
typedef struct {
    void (*Init)(void);
    uint32_t (*GetCalendarTime)(uint16_t *);
    uint32_t (*BkUp_Write_Seconds)(uint32_t);
    uint32_t (*BkUp_Read_Seconds)(void);
    uint32_t (*BkUp_Write_SubSeconds)(uint32_t);
    uint32_t (*BkUp_Read_SubSeconds)(void);
    uint32_t (*GetMinimumTimeout)(void);
} SYSTIM_Driver_t;

static void systim_init(void) {}
static uint32_t systim_get_cal(uint16_t *ms) { if(ms) *ms = 0; return 0; }
static uint32_t systim_bkup_w_s(uint32_t v) { (void)v; return 0; }
static uint32_t systim_bkup_r_s(void) { return 0; }
static uint32_t systim_bkup_w_ss(uint32_t v) { (void)v; return 0; }
static uint32_t systim_bkup_r_ss(void) { return 0; }
static uint32_t systim_min_to(void) { return 1; }

const SYSTIM_Driver_t UTIL_SYSTIMDriver = {
    .Init = systim_init,
    .GetCalendarTime = systim_get_cal,
    .BkUp_Write_Seconds = systim_bkup_w_s,
    .BkUp_Read_Seconds = systim_bkup_r_s,
    .BkUp_Write_SubSeconds = systim_bkup_w_ss,
    .BkUp_Read_SubSeconds = systim_bkup_r_ss,
    .GetMinimumTimeout = systim_min_to,
};
