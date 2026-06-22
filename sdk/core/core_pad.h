/**
 * core_pad.h — Pad-level GPIO for tile pad numbers
 *
 * The primary GPIO API. All functions take tile pad numbers
 * (matching silkscreen) and resolve to hardware automatically.
 *
 * Edge-triggered events fire for any pad whose config.json
 * `gpio.<pad>.exti` is set to `rising`, `falling`, or `both`. Coregen
 * wires the EXTI interrupts into a pending-bit bitmap; the Studio
 * main loop drains it and calls the DSL `on Core.Pad.*` handler once
 * per pending pad with the pad number as payload.
 *
 * Fires once per rising edge on any pad whose config.json gpio.<pad>.exti
 * is set to "rising" or "both". Likewise for falling. Coregen wires the
 * EXTI callbacks; the Studio main loop drains the pending bitmap and
 * calls the DSL handler with the pad number as payload.
 *
 * @studio category pad label=Core.Pad icon=◼
 * @studio event name=rising payload=pad:int
 * @studio event name=falling payload=pad:int
 *
 * @studio coverage
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
 * @studio expose category=pad name=write
 * @studio twin full
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
 * @studio expose category=pad name=read returns=bool
 * @studio twin full
 * @param pad [1..64] Tile pad number.
 */
static inline int core_pad_read(uint8_t pad)
{
    return hal_pad_read(pad);
}

/**
 * Toggle a pad output.
 *
 * @studio expose category=pad name=toggle
 * @studio twin full
 * @param pad [1..64] Tile pad number.
 */
static inline void core_pad_toggle(uint8_t pad)
{
    hal_pad_toggle(pad);
}

/* ---- Fast path (resolved handle + bulk BSRR) ----
 *
 * core_pad_write/read do a pad→port/pin table lookup every call. For tight
 * bitbang inner loops (e.g. a software SWD/CMSIS-DAP clock) that's too slow:
 * resolve the pad ONCE to a {port, mask} handle, then hit BSRR/IDR directly —
 * each op compiles to a single store/load. C-only (struct ABI), no DSL surface.
 */

typedef struct {
    GPIO_TypeDef *port;  /* NULL if the pad has no GPIO (power/reset/etc.) */
    uint32_t      mask;  /* (1u << pin) */
} core_pad_fast_t;

/** Resolve a pad to a fast handle once, outside the hot loop. */
static inline core_pad_fast_t core_pad_resolve(uint8_t pad)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    core_pad_fast_t h = { g.port, g.port ? (1UL << g.pin) : 0UL };
    return h;
}

/** Drive the resolved pad high — single BSRR write. */
static inline void core_pad_fast_set(core_pad_fast_t h)   { h.port->BSRR = h.mask; }
/** Drive the resolved pad low — single BSRR write. */
static inline void core_pad_fast_clear(core_pad_fast_t h) { h.port->BSRR = h.mask << 16; }
/** Drive the resolved pad to `state` (0/1) — single BSRR write. */
static inline void core_pad_fast_write(core_pad_fast_t h, int state)
{
    h.port->BSRR = state ? h.mask : (h.mask << 16);
}
/** Read the resolved pad — single IDR load. Returns 0 or 1. */
static inline int core_pad_fast_read(core_pad_fast_t h)
{
    return (h.port->IDR & h.mask) ? 1 : 0;
}

/**
 * Bulk update many pins on ONE port in a single atomic BSRR write: pins in
 * `set_mask` go high, pins in `clear_mask` go low (set wins on overlap, per
 * BSRR). Build the masks from the generated PAD_n_MASK defines. Useful for
 * synchronized buses / LED matrices without per-pad jitter.
 */
static inline void core_pad_bulk_write(GPIO_TypeDef *port,
                                       uint32_t set_mask, uint32_t clear_mask)
{
    port->BSRR = set_mask | (clear_mask << 16);
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

// @studio unsupported tier=2 value=M title="No DSL access to slew-rate / pull / drive-strength"
//   Tier 2 exposes write/read/toggle only. core_pad_speed,
//   core_pad_output_od pull-config, and the analog-mode setter are
//   reachable only by escaping to C. A Tier 2 wrapper (or @studio expose
//   annotation) for slew-rate would let DSL programs tune SPI/UART rise
//   times without leaving the IDE.
//
// @studio unsupported tier=2 value=L title="Bulk / fast pad write is C-only"
//   The single-pad DSL ops do a pad→port/pin lookup each call. A C fast
//   path now exists — core_pad_resolve() + core_pad_fast_* (resolved
//   {port,mask} handle, direct BSRR/IDR) and core_pad_bulk_write() (one
//   atomic BSRR for many pins) — used by bitbang inner loops like the
//   CMSIS-DAP probe. Still no DSL surface (pointer/struct ABI), so
//   synchronized-bus / LED-matrix updates from the IDE remain a gap.

#endif /* CORE_PAD_H */
