/**
 * ll_systick.h — Low-level SysTick timer for delays
 *
 * Provides blocking millisecond and microsecond delays using the
 * ARM Cortex-M SysTick timer. SysTick is identical across all
 * Cortex-M cores (M0+, M4, M33).
 *
 * Call ll_systick_init() once at startup with the SYSCLK frequency.
 * The SYSCLK_HZ define from tile_config.h works perfectly here.
 */

#ifndef LL_SYSTICK_H
#define LL_SYSTICK_H

#include <stdint.h>

/* ---- SysTick registers (ARM Cortex-M, same on all cores) ---- */

#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK_CSR         REG32(SYSTICK_BASE + 0x00UL)  /* Control and Status */
#define SYSTICK_RVR         REG32(SYSTICK_BASE + 0x04UL)  /* Reload Value */
#define SYSTICK_CVR         REG32(SYSTICK_BASE + 0x08UL)  /* Current Value */

#define SYSTICK_CSR_ENABLE  (1UL << 0)
#define SYSTICK_CSR_TICKINT (1UL << 1)
#define SYSTICK_CSR_CLKSRC  (1UL << 2)  /* 1 = processor clock */
#define SYSTICK_CSR_COUNTFLAG (1UL << 16)

/* ---- State ---- */

/* Ticks per microsecond, set by ll_systick_init() */
static uint32_t _systick_us_ticks;

/**
 * Initialize SysTick for delay functions.
 *   sysclk_hz: System clock frequency in Hz (e.g., 80000000 for 80MHz)
 */
static inline void ll_systick_init(uint32_t sysclk_hz)
{
    _systick_us_ticks = sysclk_hz / 1000000UL;
}

/**
 * Blocking delay in microseconds.
 * Uses SysTick in polling mode (no interrupt needed).
 * Max single delay: (2^24 - 1) / ticks_per_us microseconds.
 */
static inline void ll_delay_us(uint32_t us)
{
    uint32_t ticks = us * _systick_us_ticks;

    /* SysTick is a 24-bit down-counter, max reload = 0xFFFFFF */
    while (ticks > 0) {
        uint32_t chunk = (ticks > 0xFFFFFFUL) ? 0xFFFFFFUL : ticks;
        ticks -= chunk;

        SYSTICK_RVR = chunk - 1;
        SYSTICK_CVR = 0;  /* Writing any value clears the counter */
        SYSTICK_CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_CLKSRC;

        while (!(SYSTICK_CSR & SYSTICK_CSR_COUNTFLAG))
            ;

        SYSTICK_CSR = 0;  /* Stop SysTick */
    }
}

/**
 * Blocking delay in milliseconds.
 */
static inline void ll_delay_ms(uint32_t ms)
{
    while (ms--) {
        ll_delay_us(1000);
    }
}

#endif /* LL_SYSTICK_H */
