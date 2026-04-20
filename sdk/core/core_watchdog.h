/**
 * core_watchdog.h — Independent Watchdog (IWDG)
 *
 * The IWDG runs on its own 32 kHz LSI clock, independent of SYSCLK.
 * Once started, it CANNOT be stopped — only a full MCU reset
 * disables it. If your code doesn't call core_watchdog_feed()
 * before the timeout, the MCU resets.
 *
 * Typical usage:
 *
 *   #include "core_watchdog.h"
 *
 *   if (core_watchdog_caused_reset()) {
 *       // handle recovery...
 *       core_watchdog_clear_flags();
 *   }
 *
 *   core_watchdog_start(2000);   // 2-second timeout
 *
 *   while (1) {
 *       do_work();
 *       core_watchdog_feed();
 *   }
 *
 * @tessera category watchdog label=Core.Watchdog icon=🐕
 */

#ifndef CORE_WATCHDOG_H
#define CORE_WATCHDOG_H

#include "ll_iwdg.h"

/**
 * Start the independent watchdog with a timeout in milliseconds.
 * Selects the best prescaler/reload combination automatically.
 *
 * Common values: 1000, 2000, 5000, 10000 (max ~28000).
 *
 * WARNING: Once started, the IWDG cannot be stopped.
 *
 * @tessera expose category=watchdog name=start
 * @param timeout_ms [100..28000] Timeout in milliseconds before reset.
 */
static inline void core_watchdog_start(uint32_t timeout_ms)
{
    /* LSI ≈ 32 kHz. Pick prescaler to fit timeout in 12-bit reload (0–4095).
       tick_ms = prescaler / 32.  reload = timeout_ms / tick_ms - 1.
       We try prescalers from small to large until reload fits. */
    static const uint32_t psc_vals[] = { 4, 8, 16, 32, 64, 128, 256 };
    static const uint32_t psc_regs[] = {
        LL_IWDG_PSC_4, LL_IWDG_PSC_8, LL_IWDG_PSC_16, LL_IWDG_PSC_32,
        LL_IWDG_PSC_64, LL_IWDG_PSC_128, LL_IWDG_PSC_256
    };

    for (int i = 0; i < 7; i++) {
        uint32_t reload = (timeout_ms * 32UL) / (psc_vals[i] * 1000UL);
        if (reload == 0) reload = 1;
        if (reload <= 4096) {
            ll_iwdg_init(psc_regs[i], reload - 1);
            return;
        }
    }
    /* Fallback: max timeout (~28 seconds) */
    ll_iwdg_init(LL_IWDG_PSC_256, 4095);
}

/**
 * Feed the watchdog. Must be called before the timeout expires.
 *
 * @tessera expose category=watchdog name=feed
 */
static inline void core_watchdog_feed(void)
{
    ll_iwdg_refresh();
}

/** Check if the last reset was caused by the watchdog. */
static inline int core_watchdog_caused_reset(void)
{
    return ll_iwdg_caused_reset();
}

/** Clear all reset flags (call after checking cause). */
static inline void core_watchdog_clear_flags(void)
{
    ll_rcc_clear_reset_flags();
}

#endif /* CORE_WATCHDOG_H */
