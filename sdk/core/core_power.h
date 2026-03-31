/**
 * core_power.h — Sleep and low-power modes
 *
 * CPU sleep, deep sleep (Stop), shutdown (Standby), and
 * independent watchdog (IWDG) for fault recovery.
 */

#ifndef CORE_POWER_H
#define CORE_POWER_H

#include "ll_pwr.h"
#include "ll_iwdg.h"

/* ---- Sleep modes ---- */

/** Sleep until any interrupt (CPU stopped, peripherals running). */
static inline void core_sleep(void)
{
    ll_pwr_sleep_wfi();
}

/** Deep sleep / Stop mode (SRAM retained, wake via EXTI). */
static inline void core_deep_sleep(void)
{
    ll_pwr_stop();
}

/**
 * Shutdown / Standby (lowest power, SRAM lost).
 * On wake the MCU resets from Reset_Handler.
 */
static inline void core_shutdown(void)
{
    ll_pwr_standby();
}

/** Returns 1 if the MCU woke from a shutdown (standby). */
static inline int core_woke_from_shutdown(void)
{
    return ll_pwr_woke_from_standby();
}

/* ---- Watchdog ---- */

/**
 * Start the independent watchdog with a timeout in seconds.
 * Supported values: 1, 2, 5, 10. Other values clamp to the
 * nearest supported timeout.
 *
 * WARNING: Once started, the IWDG cannot be stopped.
 * Call core_watchdog_feed() regularly in your main loop.
 */
static inline void core_watchdog_start(uint32_t seconds)
{
    if (seconds <= 1)
        ll_iwdg_init_1s();
    else if (seconds <= 2)
        ll_iwdg_init_2s();
    else if (seconds <= 5)
        ll_iwdg_init_5s();
    else
        ll_iwdg_init_10s();
}

/** Feed (refresh) the watchdog to prevent a reset. */
static inline void core_watchdog_feed(void)
{
    ll_iwdg_refresh();
}

/** Returns 1 if the last reset was caused by the watchdog. */
static inline int core_watchdog_caused_reset(void)
{
    return ll_iwdg_caused_reset();
}

#endif /* CORE_POWER_H */
