/**
 * hal_led.h — Compatibility shim.
 *
 * The LED API moved to core_led.h (in sdk/core/) to match the rest of
 * the Core SDK naming convention — the LED belongs to the Core tile,
 * not child tiles, so it lives with the other core_<module>.h headers.
 *
 * This header is kept so existing `#include "hal_led.h"` users keep
 * working; it forwards to the new location.
 */

#ifndef HAL_LED_H
#define HAL_LED_H

#include "core_led.h"

#endif /* HAL_LED_H */
