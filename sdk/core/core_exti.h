/**
 * core_exti.h — Pad-level external interrupts
 *
 * Register edge-triggered callbacks on any GPIO pad.
 * The pad is automatically configured as input.
 */

#ifndef CORE_EXTI_H
#define CORE_EXTI_H

#include "tal_exti.h"

/* ---- Edge aliases ---- */

#define EDGE_RISING   HAL_EXTI_RISING
#define EDGE_FALLING  HAL_EXTI_FALLING
#define EDGE_BOTH     HAL_EXTI_BOTH

/* ---- Pad interrupt API ---- */

/**
 * Register an edge-triggered callback on a pad.
 *
 * The pad is automatically configured as input with a sensible
 * pull resistor based on edge direction (see tal_exti.h).
 *
 * @param pad   Tile pad number
 * @param edge  EDGE_RISING, EDGE_FALLING, or EDGE_BOTH
 * @param cb    Callback (called from ISR context)
 * @param ctx   Opaque pointer passed to cb
 */
static inline hal_status_t core_on_change(uint8_t pad, uint32_t edge,
                                           hal_callback_t cb, void *ctx)
{
    return tal_exti_enable(pad, edge, cb, ctx);
}

/** Stop the interrupt on a pad. */
static inline void core_on_change_stop(uint8_t pad)
{
    tal_exti_disable(pad);
}

#endif /* CORE_EXTI_H */
