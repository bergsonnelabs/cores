/**
 * core_recovery.h — watchdog-strike brick recovery (ROM-DFU builds)
 *
 * A soft-bricked Core (bad app that hangs before USB comes up) resets every
 * IWDG timeout but never enumerates, so it can't be reflashed. This counts
 * consecutive watchdog resets that never reached healthy operation; after
 * CORE_RECOVERY_STRIKE_LIMIT of them, core_init() stops launching the app and
 * drops to the ST ROM bootloader (0483:DF11) so it can always be reflashed.
 *
 * The strike state lives in the reserved-SRAM words next to the DFU magic
 * (hal_dfu.h): it survives a warm/IWDG reset and clears on a cold power-on, so
 * a power-cycle is always a fresh start. Only meaningful on ROM_DFU builds of a
 * USB-capable core (where hal_dfu.h defines the reserved addresses).
 *
 * Flow, all pre-clock in core_init() before any user code:
 *   note_boot()  → read HW reset cause, stash it, inc/reset strikes, clear RMVF
 *   over_limit() → decide whether to bail to DFU
 *   (app) clear() → once healthy, zero the counter so transient resets don't
 *                   accumulate toward a false park (the starter template calls it)
 */

#ifndef CORE_RECOVERY_H
#define CORE_RECOVERY_H

#include "core_watchdog.h"
#include "hal_dfu.h"

/* Consecutive watchdog resets (with no healthy boot in between) before we stop
 * launching the app and park in ROM DFU. ~N × the IWDG timeout to recover. */
#ifndef CORE_RECOVERY_STRIKE_LIMIT
#define CORE_RECOVERY_STRIKE_LIMIT 3u
#endif

#ifdef DFU_STRIKE_TAG_ADDR

/**
 * Account for this boot and return the running strike count.
 * Call once, very early in core_init(), before clocks/user code.
 */
static inline uint32_t core_recovery_note_boot(void)
{
    uint32_t strikes = hal_recovery_strikes();
    int wd = ll_iwdg_caused_reset();     /* read the raw HW cause (IWDGRSTF) */
    hal_recovery_stash_cause(wd);        /* preserve for core_watchdog_caused_reset() */
    strikes = wd ? (strikes + 1u) : 0u;  /* any non-watchdog reset = fresh start */
    hal_recovery_set_strikes(strikes);
    ll_rcc_clear_reset_flags();          /* RMVF, else the flag latches and miscounts */
    return strikes;
}

/** True once we've had enough consecutive watchdog resets to give up on the app. */
static inline int core_recovery_over_limit(uint32_t strikes)
{
    return strikes >= CORE_RECOVERY_STRIKE_LIMIT;
}

/**
 * Clear the strike counter — call from the app once it's proven healthy (e.g.
 * after feeding the watchdog past ~2× its timeout) so a transient hang that
 * recovers never accumulates toward a false DFU park.
 */
static inline void core_recovery_clear(void)
{
    hal_recovery_set_strikes(0);
}

#endif /* DFU_STRIKE_TAG_ADDR */

#endif /* CORE_RECOVERY_H */
