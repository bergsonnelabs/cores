/**
 * hal_gpio.h — Pad-based GPIO abstraction (header-only)
 *
 * Provides GPIO operations using tile pad numbers instead of
 * raw port/pin. Uses the PAD_n_PORT / PAD_n_PIN defines from
 * core_pads.h for compile-time pad→GPIO resolution.
 *
 * Usage:
 *   hal_pad_output(7);          // Configure pad 7 as output
 *   hal_pad_write(7, 1);        // Set pad 7 high
 *   hal_pad_toggle(7);          // Toggle pad 7
 *   int val = hal_pad_read(7);  // Read pad 7
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "ll_common.h"
#include "ll_gpio.h"
#include "ll_rcc.h"
#include "core_pads.h"

/* ============================================================
 * Pad-to-GPIO lookup
 *
 * Maps a pad number to its GPIO port and pin at compile time.
 * The macro approach lets the compiler optimize away the switch
 * when the pad number is a constant (which it almost always is).
 * ============================================================ */

typedef struct {
    GPIO_TypeDef *port;
    uint32_t pin;
} hal_pad_gpio_t;

/**
 * Look up the GPIO port and pin for a tile pad number.
 * Returns {NULL, 0} for pads with no GPIO (power, reset, etc.).
 */
static inline hal_pad_gpio_t hal_pad_lookup(uint8_t pad)
{
    hal_pad_gpio_t g = {NULL, 0};
    switch (pad) {
#ifdef PAD_2_PORT
    case 2:  g.port = PAD_2_PORT;  g.pin = PAD_2_PIN;  break;
#endif
#ifdef PAD_3_PORT
    case 3:  g.port = PAD_3_PORT;  g.pin = PAD_3_PIN;  break;
#endif
#ifdef PAD_4_PORT
    case 4:  g.port = PAD_4_PORT;  g.pin = PAD_4_PIN;  break;
#endif
#ifdef PAD_5_PORT
    case 5:  g.port = PAD_5_PORT;  g.pin = PAD_5_PIN;  break;
#endif
#ifdef PAD_6_PORT
    case 6:  g.port = PAD_6_PORT;  g.pin = PAD_6_PIN;  break;
#endif
#ifdef PAD_7_PORT
    case 7:  g.port = PAD_7_PORT;  g.pin = PAD_7_PIN;  break;
#endif
#ifdef PAD_8_PORT
    case 8:  g.port = PAD_8_PORT;  g.pin = PAD_8_PIN;  break;
#endif
#ifdef PAD_9_PORT
    case 9:  g.port = PAD_9_PORT;  g.pin = PAD_9_PIN;  break;
#endif
#ifdef PAD_10_PORT
    case 10: g.port = PAD_10_PORT; g.pin = PAD_10_PIN; break;
#endif
#ifdef PAD_11_PORT
    case 11: g.port = PAD_11_PORT; g.pin = PAD_11_PIN; break;
#endif
#ifdef PAD_12_PORT
    case 12: g.port = PAD_12_PORT; g.pin = PAD_12_PIN; break;
#endif
#ifdef PAD_13_PORT
    case 13: g.port = PAD_13_PORT; g.pin = PAD_13_PIN; break;
#endif
#ifdef PAD_14_PORT
    case 14: g.port = PAD_14_PORT; g.pin = PAD_14_PIN; break;
#endif
#ifdef PAD_15_PORT
    case 15: g.port = PAD_15_PORT; g.pin = PAD_15_PIN; break;
#endif
#ifdef PAD_16_PORT
    case 16: g.port = PAD_16_PORT; g.pin = PAD_16_PIN; break;
#endif
#ifdef PAD_17_PORT
    case 17: g.port = PAD_17_PORT; g.pin = PAD_17_PIN; break;
#endif
#ifdef PAD_18_PORT
    case 18: g.port = PAD_18_PORT; g.pin = PAD_18_PIN; break;
#endif
#ifdef PAD_19_PORT
    case 19: g.port = PAD_19_PORT; g.pin = PAD_19_PIN; break;
#endif
#ifdef PAD_20_PORT
    case 20: g.port = PAD_20_PORT; g.pin = PAD_20_PIN; break;
#endif
#ifdef PAD_21_PORT
    case 21: g.port = PAD_21_PORT; g.pin = PAD_21_PIN; break;
#endif
#ifdef PAD_22_PORT
    case 22: g.port = PAD_22_PORT; g.pin = PAD_22_PIN; break;
#endif
    default: break;
    }
    return g;
}

/* ============================================================
 * Pad configuration
 * ============================================================ */

/** Configure a pad as push-pull output. Enables the port clock. */
static inline void hal_pad_output(uint8_t pad)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    ll_rcc_gpio_clk_enable(g.port);
    ll_gpio_config_output(g.port, g.pin);
}

/** Configure a pad as input with pull selection. */
static inline void hal_pad_input(uint8_t pad, uint32_t pull)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    ll_rcc_gpio_clk_enable(g.port);
    ll_gpio_config_input(g.port, g.pin, pull);
}

/** Configure a pad as analog (for ADC, DAC, comparator). */
static inline void hal_pad_analog(uint8_t pad)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    ll_rcc_gpio_clk_enable(g.port);
    ll_gpio_config_analog(g.port, g.pin);
}

/** Configure a pad for alternate function. */
static inline void hal_pad_af(uint8_t pad, uint32_t af,
                               uint32_t otype, uint32_t speed, uint32_t pull)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    ll_rcc_gpio_clk_enable(g.port);
    ll_gpio_config_af(g.port, g.pin, af, otype, speed, pull);
}

/* ============================================================
 * Pad state control
 * ============================================================ */

/** Write a pad: 0 = low, non-zero = high */
static inline void hal_pad_write(uint8_t pad, int state)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    if (state)
        ll_gpio_set(g.port, 1UL << g.pin);
    else
        ll_gpio_clear(g.port, 1UL << g.pin);
}

/** Read a pad. Returns 0 or 1. */
static inline int hal_pad_read(uint8_t pad)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return 0;
    return ll_gpio_read(g.port, 1UL << g.pin) ? 1 : 0;
}

/** Toggle a pad. */
static inline void hal_pad_toggle(uint8_t pad)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    ll_gpio_toggle(g.port, 1UL << g.pin);
}

/* ============================================================
 * Raw port/pin API (pass-through to LL)
 * ============================================================ */

static inline void hal_gpio_write(GPIO_TypeDef *port, uint32_t pin_mask, int state)
{
    if (state) ll_gpio_set(port, pin_mask);
    else       ll_gpio_clear(port, pin_mask);
}

static inline int hal_gpio_read(GPIO_TypeDef *port, uint32_t pin_mask)
{
    return ll_gpio_read(port, pin_mask) ? 1 : 0;
}

static inline void hal_gpio_toggle(GPIO_TypeDef *port, uint32_t pin_mask)
{
    ll_gpio_toggle(port, pin_mask);
}

#endif /* HAL_GPIO_H */
