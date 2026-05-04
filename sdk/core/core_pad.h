/**
 * core_pad.h — Pad-level GPIO for tile pad numbers
 *
 * The primary GPIO API. All functions take tile pad numbers
 * (matching silkscreen) and resolve to hardware automatically.
 *
 * Edge-triggered events fire for any pad whose config.json
 * `gpio.<pad>.exti` is set to `rising`, `falling`, or `both`. Coregen
 * wires the EXTI interrupts into a pending-bit bitmap; the Tessera
 * main loop drains it and calls the DSL `on Core.Pad.*` handler once
 * per pending pad with the pad number as payload.
 *
 * Fires once per rising edge on any pad whose config.json gpio.<pad>.exti
 * is set to "rising" or "both". Likewise for falling. Coregen wires the
 * EXTI callbacks; the Tessera main loop drains the pending bitmap and
 * calls the DSL handler with the pad number as payload.
 *
 * @tessera category pad label=Core.Pad icon=◼
 * @tessera event name=rising payload=pad:int
 * @tessera event name=falling payload=pad:int
 *
 * @tessera coverage
 *   id:    gpio
 *   name:  GPIO — pad-level I/O
 *   page:  /docs/sdk/gpio
 *   blurb: Pad-numbered GPIO API. Tier 2 helpers (write/read/toggle) take
 *          a pad number and resolve the port/pin from coregen's tables.
 *          Tier 1 helpers configure pad mode (output / open-drain / input
 *          / analog / speed) and EXTI callbacks — they sit below the DSL
 *          surface, called by the framework at init and by escape-to-C
 *          users who want explicit control.
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

/**
 * Set a pad high (ON) or low (OFF).
 *
 * @tessera expose category=pad name=write
 * @tessera twin full
 * @param pad [1..64] Tile pad number.
 * @param state [0..1] 0 for low, 1 for high.
 */
static inline void core_pad_write(uint8_t pad, int state)
{
    hal_pad_write(pad, state);
}

/**
 * Read a pad. Returns 0 or 1.
 *
 * @tessera expose category=pad name=read returns=bool
 * @tessera twin full
 * @param pad [1..64] Tile pad number.
 */
static inline int core_pad_read(uint8_t pad)
{
    return hal_pad_read(pad);
}

/**
 * Toggle a pad output.
 *
 * @tessera expose category=pad name=toggle
 * @tessera twin full
 * @param pad [1..64] Tile pad number.
 */
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

/* ---- Edge-triggered interrupts (EXTI) ---- */

#include "tal_exti.h"

/* Edge aliases */
#define EDGE_RISING   HAL_EXTI_RISING
#define EDGE_FALLING  HAL_EXTI_FALLING
#define EDGE_BOTH     HAL_EXTI_BOTH

/**
 * Register an edge-triggered callback on a pad.
 *
 * The pad is automatically configured as input with a sensible
 * pull resistor based on edge direction:
 *   rising  → pull-down (idle low)
 *   falling → pull-up   (idle high)
 *   both    → no pull   (external bias expected)
 *
 * @param pad   Tile pad number
 * @param edge  EDGE_RISING, EDGE_FALLING, or EDGE_BOTH
 * @param cb    Callback (called from ISR context)
 * @param ctx   Opaque pointer passed to cb
 */
static inline hal_status_t core_pad_on_change(uint8_t pad, uint32_t edge,
                                               hal_callback_t cb, void *ctx)
{
    return tal_exti_enable(pad, edge, cb, ctx);
}

/** Stop the interrupt on a pad. */
static inline void core_pad_on_change_stop(uint8_t pad)
{
    tal_exti_disable(pad);
}

/* ---- Backward compatibility ---- */
#define core_on_change       core_pad_on_change
#define core_on_change_stop  core_pad_on_change_stop

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=M title="No DSL access to slew-rate / pull / drive-strength"
//   Tier 2 exposes write/read/toggle only. core_pad_speed,
//   core_pad_output_od pull-config, and the analog-mode setter are
//   reachable only by escaping to C. A Tier 2 wrapper (or @tessera expose
//   annotation) for slew-rate would let DSL programs tune SPI/UART rise
//   times without leaving the IDE.
//
// @tessera unsupported tier=2 value=L title="No bulk pad write / read"
//   Each call writes one pad. A vector form (mask + value) would let
//   synchronized buses or LED matrices update pads in a single BSRR
//   write, avoiding per-pad jitter.

#endif /* CORE_PAD_H */
