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

/* ---- Pad GPIO API ---- */

/** Configure a pad as push-pull output. */
static inline void core_pad_output(uint8_t pad)
{
    hal_pad_output(pad);
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

#endif /* CORE_PAD_H */
