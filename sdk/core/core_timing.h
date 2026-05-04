/**
 * core_timing.h — Delays and time tracking
 *
 * Blocking delays (ms/us) and a millisecond tick counter for
 * timeouts and general timekeeping.
 *
 * @studio category timing label=Core.Timing icon=⏱
 *
 * @studio coverage
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
 * @studio expose category=timing name=delay_ms
 * @studio twin full
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
 * @studio expose category=timing name=delay_us
 * @studio twin full
 * @param us [1..10000] Delay duration in microseconds.
 */
static inline void core_delay_us(uint32_t us)
{
    ll_delay_us(us);
}

/**
 * Milliseconds since boot (wraps at ~49 days).
 *
 * @studio expose category=timing name=millis returns=int
 * @studio twin full
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
 * @studio expose category=timing name=timeout returns=bool
 * @studio twin full
 * @param start Value previously returned from `millis()`.
 * @param ms [1..3600000] Timeout duration in milliseconds.
 */
static inline int core_timeout(uint32_t start, uint32_t ms)
{
    return hal_timeout_expired(start, ms);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @studio unsupported tier=1 value=L title="No monotonic 64-bit clock"
//   millis wraps at ~49.7 days. Long-running systems (industrial /
//   datalogger) need either a 64-bit upcounter or a wrap-aware
//   helper for diff-since-start.

#endif /* CORE_TIMING_H */
