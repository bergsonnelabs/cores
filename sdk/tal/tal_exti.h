/**
 * tal_exti.h — TAL EXTI: pad-centric interrupt ergonomics
 *
 * Wraps hal_exti with automatic pad-as-input configuration.
 * The user only needs a pad number, an edge, and a callback.
 *
 * Typical usage:
 *
 *   #include "tal_exti.h"
 *
 *   void on_button(void *ctx) { hal_pad_toggle(LED_PAD); }
 *   tal_exti_enable(5, HAL_EXTI_FALLING, on_button, NULL);
 *
 * Compare to the HAL equivalent:
 *
 *   hal_pad_input(5, LL_GPIO_PULL_UP);
 *   hal_exti_enable(5, HAL_EXTI_FALLING, on_button, NULL);
 *
 * The TAL version auto-configures the pad as input with a
 * sensible pull resistor based on edge direction:
 *   rising  → pull-down (idle low,  detect rising)
 *   falling → pull-up   (idle high, detect falling)
 *   both    → no pull   (external bias expected)
 */

#ifndef TAL_EXTI_H
#define TAL_EXTI_H

#include "hal_exti.h"
#include "hal_gpio.h"
#include "ll_gpio.h"

/**
 * Enable an edge-triggered interrupt on a tile pad.
 *
 * Auto-configures the pad as input with appropriate pull.
 * See header comment for pull selection logic.
 *
 * @param pad   Tile pad number
 * @param edge  HAL_EXTI_RISING, HAL_EXTI_FALLING, or HAL_EXTI_BOTH
 * @param cb    Callback (called from ISR context)
 * @param ctx   Opaque pointer passed to cb
 */
static inline hal_status_t tal_exti_enable(uint8_t pad, uint32_t edge,
                                            hal_callback_t cb, void *ctx)
{
    uint32_t pull = LL_GPIO_PULL_NONE;
    if (edge == HAL_EXTI_RISING)  pull = LL_GPIO_PULL_DOWN;
    if (edge == HAL_EXTI_FALLING) pull = LL_GPIO_PULL_UP;
    hal_pad_input(pad, pull);

    return hal_exti_enable(pad, edge, cb, ctx);
}

/**
 * Disable the EXTI interrupt on a tile pad.
 */
static inline void tal_exti_disable(uint8_t pad)
{
    hal_exti_disable(pad);
}

#endif /* TAL_EXTI_H */
