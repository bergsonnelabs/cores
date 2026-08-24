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
 * @studio category watchdog label=Core.Watchdog icon=🐕
 *
 * @studio coverage
 *   id:    watchdog
 *   name:  Watchdog — independent watchdog (IWDG)
 *   blurb: Auto-prescaler IWDG with millisecond timeouts (~100 ms to
 *          ~28 s). Tier 2 covers start + feed; the cause-of-reset and
 *          flag-clear helpers are Tier 1 (escape-to-C, typically used
 *          once at boot). Once started, the watchdog can't be stopped
 *          — the only way out is a full MCU reset.
 */

#ifndef CORE_WATCHDOG_H
#define CORE_WATCHDOG_H

#include "ll_iwdg.h"
#include "hal_dfu.h"  /* recovery stash: lets caused_reset() survive core_init's RMVF clear */

/**
 * Start the independent watchdog with a timeout in milliseconds.
 * Selects the best prescaler/reload combination automatically.
 *
 * Common values: 1000, 2000, 5000, 10000 (max ~28000).
 *
 * WARNING: Once started, the IWDG cannot be stopped.
 *
 * @studio expose category=watchdog name=start
 * @studio twin full
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
        /* tick_ms = psc/32 (LSI 32 kHz) → reload = timeout_ms / tick_ms
         *         = timeout_ms * 32 / psc. (The earlier form divided by an
         *         extra 1000, making every timeout ~1000x too short.) */
        uint32_t reload = (timeout_ms * 32UL) / psc_vals[i];
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
 * @studio expose category=watchdog name=feed
 * @studio twin full
 */
static inline void core_watchdog_feed(void)
{
    ll_iwdg_refresh();
}

/** Check if the last reset was caused by the watchdog.
 *  On ROM_DFU builds, core_init() reads and clears the hardware flag early (for
 *  the strike counter), stashing the cause in reserved SRAM — so prefer that
 *  when it's valid; otherwise fall back to the raw RCC_CSR flag. */
static inline int core_watchdog_caused_reset(void)
{
#ifdef DFU_STRIKE_TAG_ADDR
    if (hal_recovery_valid()) return (int)hal_recovery_stashed_cause();
#endif
    return ll_iwdg_caused_reset();
}

/** Clear all reset flags (call after checking cause). */
static inline void core_watchdog_clear_flags(void)
{
    ll_rcc_clear_reset_flags();
}

/** Freeze the IWDG while the core is halted under a debugger, so a breakpoint
 *  doesn't let the watchdog reset the chip out from under an SWD session.
 *  Firmware-side so it holds for any probe/toolchain (rev b exposes SWD on L4). */
static inline void core_watchdog_debug_freeze(void)
{
    /* DBG_IWDG_STOP is bit 12 everywhere; only the register moves. Note the two
     * Cortex-M33 parts do NOT put DBGMCU at 0xE0042000 — that is the CoreSight
     * CTI block on both (confirmed on a WBA55 by walking its ROM table). */
#if defined(STM32L011xx)
    /* RM0377 27.9.4: DBG_APB1_FZ is mapped at 0x40015808. */
    SET_BITS(REG32(0x40015808UL), (1UL << 12));
#elif defined(STM32L422xx)
    /* RM0394: DBGMCU_APB1FZR1 at 0xE0042008. */
    SET_BITS(REG32(0xE0042008UL), (1UL << 12));
#elif defined(STM32WBA55xx)
    /* RM0493 43.12.7: DBGMCU @ 0xE0044000, DBGMCU_APB1LFZR at offset 0x08.
     * The RM documents only the debugger base, so software access was verified
     * on hardware: firmware writes bit 12 and reads it back, and a 6 s halt
     * under the CoreProbe with a 2 s IWDG no longer resets the part. */
    SET_BITS(REG32(0xE0044008UL), (1UL << 12));
#elif defined(STM32H523xx)
    /* RM0481 59.12.4: DBGMCU is at 0xE00E4000 for the DEBUGGER and 0x44024000
     * for SOFTWARE; DBGMCU_APB1LFZR is at offset 0x08. Firmware must use the
     * software base. (This previously wrote 0xE004203C — the CTI block — so the
     * freeze never took effect on the H5.) */
    SET_BITS(REG32(0x44024008UL), (1UL << 12));
#endif
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @studio unsupported tier=2 value=L title="No DSL access to caused_reset / clear_flags"
//   These return / clear hardware flags that only matter on the very
//   first boot iteration. Exposing them needs a story for "before
//   studio_start runs" — Studio doesn't currently model that phase.
//
// @studio unsupported tier=1 value=L title="No window watchdog (WWDG)"
//   STM32 also has a windowed watchdog (must feed within a window,
//   not just before the deadline). Useful for catching feed-too-fast
//   bugs. Not wrapped here.

#endif /* CORE_WATCHDOG_H */
