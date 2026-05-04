/**
 * core_timing.h — Delays and time tracking
 *
 * Blocking delays (ms/us) and a millisecond tick counter for
 * timeouts and general timekeeping.
 *
 * @tessera category timing label=Core.Timing icon=⏱
 *
 * @tessera coverage
 *   id:    timing
 *   name:  Timing — delays and millis
 *   blurb: Blocking delay_ms / delay_us, a free-running millis counter
 *          (wraps at ~49 days), and a timeout helper that compares
 *          against a stored start. delay_ms gets full Twin coverage —
 *          the simulator scales the busy-spin by the speed knob. The
 *          others are exposed but unobserved (no virtual clock to
 *          advance).
 */

#ifndef CORE_TIMING_H
#define CORE_TIMING_H

#include "ll_systick.h"
#include "hal_common.h"

/**
 * Blocking delay in milliseconds.
 *
 * @tessera expose category=timing name=delay_ms
 * @tessera twin full
 * @param ms [1..60000] Delay duration in milliseconds.
 */
static inline void core_delay_ms(uint32_t ms)
{
    ll_delay_ms(ms);
}

/**
 * Blocking delay in microseconds. For delays > 1 ms prefer
 * `delay_ms` — it won't starve the rest of the system as long.
 *
 * @tessera expose category=timing name=delay_us
 * @tessera twin noop
 * @param us [1..10000] Delay duration in microseconds.
 */
static inline void core_delay_us(uint32_t us)
{
    ll_delay_us(us);
}

/**
 * Milliseconds since boot (wraps at ~49 days).
 *
 * @tessera expose category=timing name=millis returns=int
 * @tessera twin noop
 */
static inline uint32_t core_millis(void)
{
    return hal_tick();
}

/**
 * Check if a timeout has elapsed.
 *   start: value returned by core_millis() at the beginning
 *   ms:    timeout duration in milliseconds
 * Returns 1 if expired, 0 otherwise.
 *
 * @tessera expose category=timing name=timeout returns=bool
 * @tessera twin noop
 * @param start Value previously returned from `millis()`.
 * @param ms [1..3600000] Timeout duration in milliseconds.
 */
static inline int core_timeout(uint32_t start, uint32_t ms)
{
    return hal_timeout_expired(start, ms);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=H title="Twin doesn't simulate millis / timeout"
//   delay_ms has a full handler that busy-spins the worker by the
//   requested amount (scaled by the speed knob), but core_millis
//   always returns 0 and core_timeout always returns false. DSL
//   programs that branch on elapsed-time logic (cooldown, debouncing,
//   timeout-or-bust) can't be exercised end-to-end in the IDE.
//
// @tessera unsupported tier=2 value=M title="delay_us not honored in the Twin"
//   The runtime only special-cases core_delay_ms — delay_us is a
//   plain logged host call. DSL programs that rely on microsecond
//   spacing run as tight no-op loops in the simulator. For accuracy
//   below 1 ms the worker would need a finer busy-spin path.
//
// @tessera unsupported tier=1 value=L title="No monotonic 64-bit clock"
//   millis wraps at ~49.7 days. Long-running systems (industrial /
//   datalogger) need either a 64-bit upcounter or a wrap-aware
//   helper for diff-since-start.

#endif /* CORE_TIMING_H */
