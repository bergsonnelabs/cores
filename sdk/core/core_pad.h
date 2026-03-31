/**
 * core_pad.h — Pad-level GPIO for tile pad numbers
 *
 * The primary GPIO API. All functions take tile pad numbers
 * (matching silkscreen) and resolve to hardware automatically.
 */

#ifndef CORE_PAD_H
#define CORE_PAD_H

#include "hal_gpio.h"

/* ---- On/Off convenience ---- */

#ifndef ON
#define ON  1
#endif
#ifndef OFF
#define OFF 0
#endif

/* ---- Pull-resistor aliases ---- */

#define PULL_NONE  LL_GPIO_PULL_NONE
#define PULL_UP    LL_GPIO_PULL_UP
#define PULL_DOWN  LL_GPIO_PULL_DOWN

/* ---- Output-type aliases ---- */

#define PUSH_PULL   LL_GPIO_OTYPE_PP
#define OPEN_DRAIN  LL_GPIO_OTYPE_OD

/* ---- Speed aliases ---- */

#define SPEED_LOW    LL_GPIO_SPEED_LOW     /* ~2 MHz  — LEDs, relays */
#define SPEED_MED    LL_GPIO_SPEED_MED     /* ~10 MHz — general GPIO (default) */
#define SPEED_HIGH   LL_GPIO_SPEED_HIGH    /* ~50 MHz — SPI, UART */
#define SPEED_VHIGH  LL_GPIO_SPEED_VHIGH   /* ~100 MHz — SDMMC, high-speed */

/* ---- Pad GPIO API ---- */

/** Configure a pad as push-pull output (default). */
static inline void core_pad_output(uint8_t pad)
{
    hal_pad_output(pad);
}

/** Configure a pad as open-drain output with optional pull resistor.
 *  Use PULL_UP for a wired-AND / I2C-style bus, PULL_NONE for external pull. */
static inline void core_pad_output_od(uint8_t pad, uint32_t pull)
{
    hal_pad_output_od(pad, pull);
}

/** Configure a pad as input with pull resistor. */
static inline void core_pad_input(uint8_t pad, uint32_t pull)
{
    hal_pad_input(pad, pull);
}

/** Set a pad high (ON) or low (OFF). */
static inline void core_pad_write(uint8_t pad, int state)
{
    hal_pad_write(pad, state);
}

/** Read a pad. Returns 0 or 1. */
static inline int core_pad_read(uint8_t pad)
{
    return hal_pad_read(pad);
}

/** Toggle a pad output. */
static inline void core_pad_toggle(uint8_t pad)
{
    hal_pad_toggle(pad);
}

/** Configure a pad as analog (for ADC, DAC, comparator). */
static inline void core_pad_analog(uint8_t pad)
{
    hal_pad_analog(pad);
}

/** Set the output speed (slew rate) of a pad.
 *  Use SPEED_LOW / SPEED_MED / SPEED_HIGH / SPEED_VHIGH.
 *  Only affects outputs — input pads ignore this setting. */
static inline void core_pad_speed(uint8_t pad, uint32_t speed)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (!g.port) return;
    ll_gpio_set_speed(g.port, g.pin, speed);
}

#endif /* CORE_PAD_H */
