/**
 * core_timing.h — Delays and time tracking
 *
 * Blocking delays (ms/us) and a millisecond tick counter for
 * timeouts and general timekeeping.
 *
 * @tessera category timing label=Core.Timing icon=⏱
 */

#ifndef CORE_TIMING_H
#define CORE_TIMING_H

#include "ll_systick.h"
#include "hal_common.h"

/**
 * Blocking delay in milliseconds.
 *
 * @tessera expose category=timing name=delay_ms
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
 * @param start Value previously returned from `millis()`.
 * @param ms [1..3600000] Timeout duration in milliseconds.
 */
static inline int core_timeout(uint32_t start, uint32_t ms)
{
    return hal_timeout_expired(start, ms);
}

#endif /* CORE_TIMING_H */
