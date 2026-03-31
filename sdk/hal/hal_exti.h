/**
 * hal_exti.h — Pad-based EXTI (external interrupt) driver
 *
 * Provides edge-triggered interrupts on GPIO pads with automatic
 * port/pin resolution via hal_pad_lookup().  Uses a static callback
 * registry indexed by EXTI line (pin number 0-15).
 *
 * Usage:
 *   void my_button(void *ctx) { hal_pad_toggle(LED_PAD); }
 *   hal_exti_enable(5, HAL_EXTI_FALLING, my_button, NULL);
 *
 * Each EXTI line supports one callback at a time.  Enabling a
 * second callback on the same line replaces the first.
 */

#ifndef HAL_EXTI_H
#define HAL_EXTI_H

#include "hal_common.h"
#include "hal_gpio.h"
#include "ll_exti.h"

/* ---- Edge aliases (match LL defines) ---- */

#define HAL_EXTI_RISING     LL_EXTI_RISING
#define HAL_EXTI_FALLING    LL_EXTI_FALLING
#define HAL_EXTI_BOTH       LL_EXTI_BOTH

/* ---- API ---- */

/**
 * Enable an edge-triggered interrupt on a tile pad.
 *
 * Configures the pad's GPIO pin as an EXTI source, stores the
 * callback, and enables the NVIC IRQ.  The pad must already be
 * configured as an input (see hal_pad_input or tal_exti_enable
 * which does this automatically).
 *
 * @param pad   Tile pad number (2-22)
 * @param edge  HAL_EXTI_RISING, HAL_EXTI_FALLING, or HAL_EXTI_BOTH
 * @param cb    Function called from ISR context on each edge
 * @param ctx   Opaque pointer passed to cb
 * @return      HAL_OK on success, HAL_ERROR if pad has no GPIO
 */
hal_status_t hal_exti_enable(uint8_t pad, uint32_t edge,
                              hal_callback_t cb, void *ctx);

/**
 * Disable the EXTI interrupt on a tile pad.
 *
 * Clears the callback and disables the EXTI line.  The NVIC IRQ
 * is only disabled when no other lines sharing the same IRQ are
 * active (relevant for grouped IRQs on L0/L4).
 */
void hal_exti_disable(uint8_t pad);

#endif /* HAL_EXTI_H */
