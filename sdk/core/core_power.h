/**
 * core_power.h — Sleep and low-power modes
 *
 * Simple API:
 *   core_sleep()              — lightest, wakes on any interrupt
 *   core_stop_for(seconds)   — Stop + RTC wakeup, auto clock recovery
 *   core_standby_for(sec)  — Standby + RTC wakeup, full reset on wake
 *
 * Advanced:
 *   core_deep_sleep()         — enter Stop (caller manages wakeup + clock recovery)
 *   core_shutdown()           — enter Standby (caller manages wakeup)
 */

#ifndef CORE_POWER_H
#define CORE_POWER_H

#include "ll_pwr.h"
#include "ll_rtc.h"
#include "ll_rcc.h"
#include "ll_iwdg.h"
#include "core_pad.h"

/* EXTI addresses come from ll_exti.h */
#include "ll_exti.h"

/* ---- Simple API ---- */

/** Sleep until any interrupt (CPU stopped, peripherals running). */
static inline void core_sleep(void)
{
    ll_pwr_sleep_wfi();
}

/**
 * Enter Stop mode for a number of seconds, then wake and restore clocks.
 * Uses RTC wakeup timer (LSI). Returns after wake with PLL + SysTick restored.
 */
static inline void core_stop_for(uint32_t seconds)
{
    /* Enable PWR + backup domain */
    ll_rcc_pwr_clk_enable();
    ll_pwr_enable_backup_access();

    /* Init RTC on LSI and set wakeup alarm */
    ll_rtc_init(0);
    ll_rtc_wakeup_config(seconds);

    /* Unmask RTC wakeup for Stop mode wake.
     * L0/L4: EXTI line 20 (IMR + RTSR).
     * H5: RTC has direct NVIC interrupt (RTC_IRQn=2). Enable NVIC +
     *      EXTI line 19 (wakeup timer) for wakeup from Stop.
     *      Note: line 17 = RTC (non-secure, all events), line 19 = TAMP.
     *      RTC is a "direct" EXTI event — only IMR needed, no RTSR. */
#if defined(STM32L011xx) || defined(STM32L422xx)
    SET_BITS(REG32(EXTI_BASE + 0x00UL), (1UL << 20));  /* IMR */
    SET_BITS(REG32(EXTI_BASE + 0x08UL), (1UL << 20));  /* RTSR */
#elif defined(STM32H523xx)
    /* H5: EXTI line 17 = RTC non-secure (direct event, IMR only) */
    ll_nvic_set_priority(2, 0x30);  /* RTC_IRQn = 2 */
    ll_nvic_enable_irq(2);
    SET_BITS(REG32(EXTI_BASE + 0x80UL), (1UL << 17));  /* IMR1: line 17 */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(EXTI_BASE + 0x80UL), (1UL << 20));  /* IMR1 */
    SET_BITS(REG32(EXTI_BASE + 0x00UL), (1UL << 20));  /* RTSR1 */
#endif

    /* Enter Stop mode */
    ll_pwr_stop();

    /* --- Woke up --- */

    /* Clear RTC wakeup flag */
    ll_rtc_wakeup_clear_flag();
#if defined(STM32L011xx) || defined(STM32L422xx)
    REG32(EXTI_BASE + 0x14UL) = (1UL << 20);  /* PR: clear pending */
#elif defined(STM32H523xx)
    ll_nvic_disable_irq(2);
#elif defined(STM32WBA55xx)
    REG32(EXTI_BASE + 0x0CUL) = (1UL << 20);  /* RPR1: clear pending */
#endif

    /* Restore PLL + SysTick */
    extern void core_clock_init(void);
    core_clock_init();
}

/**
 * Enter Stop mode until a GPIO edge occurs on the given pad.
 * Configures the pad as input, enables EXTI, enters Stop, and restores
 * clocks on wake. Returns after the edge is detected.
 *
 * @param pad   Tile pad number
 * @param edge  EDGE_FALLING, EDGE_RISING, or EDGE_BOTH
 */
static inline void core_stop_until_on_change(uint8_t pad, uint32_t edge)
{
    extern void core_clock_init(void);

    core_pad_on_change(pad, edge, (hal_callback_t)0, (void *)0);

    ll_pwr_stop();

    core_clock_init();
}

/**
 * Enter Standby mode with RTC wakeup after the given seconds.
 * Does not return — MCU resets on wake. Check core_woke_from_standby()
 * at the top of main() to detect a Standby wake.
 */
static inline void core_standby_for(uint32_t seconds) __attribute__((noreturn));
static inline void core_standby_for(uint32_t seconds)
{
    ll_rcc_pwr_clk_enable();
    ll_pwr_enable_backup_access();

    ll_rtc_init(0);
    ll_rtc_wakeup_config(seconds);

#if defined(STM32L011xx) || defined(STM32L422xx)
    SET_BITS(REG32(EXTI_BASE + 0x00UL), (1UL << 20));
    SET_BITS(REG32(EXTI_BASE + 0x08UL), (1UL << 20));
#elif defined(STM32H523xx)
    SET_BITS(REG32(EXTI_BASE + 0x80UL), (1UL << 17));  /* IMR1: line 17 (RTC) */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(EXTI_BASE + 0x80UL), (1UL << 20));
    SET_BITS(REG32(EXTI_BASE + 0x00UL), (1UL << 20));
#endif

    ll_pwr_standby();
    __builtin_unreachable();
}

/**
 * Enter Standby mode until a GPIO edge on the given pad (via WKUP pin).
 * Does not return — MCU resets on wake. Not all pads support WKUP;
 * returns without entering Standby if the pad has no WKUP capability.
 *
 * On Core.U.2: pad 8 (PA0 = WKUP1) and pad 7 (PA2 = WKUP4).
 *
 * @param pad   Tile pad number
 * @param edge  EDGE_FALLING or EDGE_RISING
 */
static inline void core_standby_until_on_change(uint8_t pad, uint32_t edge)
{
#if defined(STM32L422xx)
    /* Resolve pad to GPIO */
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;

    /* Match GPIO to WKUP pin number (STM32L422 fixed mapping) */
    int wkup = -1;
    if (g.port == GPIOA && g.pin == 0) wkup = 0;       /* WKUP1 */
    else if (g.port == GPIOC && g.pin == 13) wkup = 1;  /* WKUP2 */
    else if (g.port == GPIOA && g.pin == 2) wkup = 3;   /* WKUP4 */
    if (wkup < 0) return;  /* pad not a WKUP pin */

    ll_rcc_pwr_clk_enable();

    /* PWR_CR3: enable WKUPx (bits 0-4) */
    SET_BITS(REG32(PWR_BASE + 0x08UL), (1UL << wkup));

    /* PWR_CR4: set polarity. Bit=0 → rising edge wake, bit=1 → falling edge */
    if (edge == EDGE_FALLING)
        SET_BITS(REG32(PWR_BASE + 0x0CUL), (1UL << wkup));
    else
        CLR_BITS(REG32(PWR_BASE + 0x0CUL), (1UL << wkup));

    /* PWR_SCR: clear wakeup flag */
    REG32(PWR_BASE + 0x14UL) = (1UL << wkup);

    ll_pwr_standby();
    __builtin_unreachable();
#else
    /* WKUP-pin standby not yet implemented on this core family. */
    (void)pad;
    (void)edge;
#endif
}

/* ---- Advanced API ---- */

/** Enter Stop mode (caller manages wakeup source + clock recovery). */
static inline void core_stop(void)
{
    ll_pwr_stop();
}

/** Enter Standby (caller manages wakeup source). Does not return. */
static inline void core_standby(void) __attribute__((noreturn));
static inline void core_standby(void)
{
    ll_pwr_standby();
    __builtin_unreachable();
}

/* Backward compatibility */
#define core_deep_sleep  core_stop
#define core_shutdown    core_standby

/** Returns 1 if the MCU woke from Standby. */
static inline int core_woke_from_standby(void)
{
    return ll_pwr_woke_from_standby();
}

/** Clear the Standby wake flag. Call after core_woke_from_standby() returns 1. */
static inline void core_clear_standby_flag(void)
{
    ll_pwr_clear_standby_flag();
}

/* ---- Watchdog ----
 * The canonical watchdog API lives in core_watchdog.h (millisecond precision,
 * auto-prescaler).  This header re-exports it for backward compatibility.
 */
#include "core_watchdog.h"

/**
 * Start the independent watchdog with a timeout in seconds (convenience).
 * For finer control use core_watchdog_start() from core_watchdog.h which
 * accepts milliseconds.
 */
static inline void core_watchdog_start_seconds(uint32_t seconds)
{
    core_watchdog_start(seconds * 1000);
}

#endif /* CORE_POWER_H */
