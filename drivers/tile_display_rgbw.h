/**
 * @file   tile_display_rgbw.h
 * @brief  RGBW LED driver for the Display.RGBW tile (LP5811).
 * @version 2.1.0
 *
 * 4-channel LED driver with independent PWM + current control.
 * Channels: R (LED0), G (LED2), B (LED1), W (LED3).
 *
 * Quick start:
 * @code
 *   tile_t led;
 *   tile_display_rgbw_init(&hal, 0, &led, NULL);
 *   tile_display_rgbw_set_color(&led, 255, 0, 0);    // red
 *   tile_display_rgbw_pulse(&led, 0, 255, 0, 200);   // 200ms green flash
 *   tile_display_rgbw_off(&led);                     // all off
 * @endcode
 *
 * Version history:
 *   v2.1.0 — Tier-2 idiomatic helpers (set_color, pulse, breathe,
 *            flash, is_faulted) + section= tagging for Coverage Table.
 *   v2.0.0 — Initial tier-1 surface (set, off, current, faults).
 *
 * @studio tile label=Display.RGBW icon=◑
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * @studio unsupported severity=common category="Animation Engine Unit (AEU)"
 *   The LP5811 has 4 autonomous animation engines (one per LED, with
 *   AEU1/AEU2/AEU3 sub-engines per channel) that play breathe / pulse /
 *   ramp patterns without MCU intervention. The bytecode-style register
 *   layout for the engine isn't documented in detail in the LP5811
 *   datasheet — the register-map TRM lists addresses 0x080–0x0E7 but
 *   not the timing-and-PWM semantics needed to compose programs.
 *   Closing this gap requires a TI specification (or careful
 *   reverse-engineering against TI's evaluation tool); deliberately
 *   deferred until that source is available.
 *
 * @studio unsupported severity=niche category="Multi-address support (0x50–0x53)"
 *   Chip-gated. The four LP5811 addresses 0x50/0x51/0x52/0x53 are
 *   selected by Bit4/Bit3 of the chip-address byte, but those bits
 *   are fixed by the factory material variant (LP5811A/B/C/D, see
 *   datasheet §4 Device Comparison). They are not pin-strapped or
 *   register-configurable. The Display.RGBW (rev a) tile ships only
 *   the A variant. Adding the other three addresses requires a tile
 *   hardware revision that places the alternate part numbers on the
 *   PCB — not something the driver can close on its own.
 */

#ifndef INC_TILE_DISP_RGBW_H_
#define INC_TILE_DISP_RGBW_H_

#include "tiles.h"
#include <stdint.h>

/* ---- Driver version ---- */

#define TILE_DISP_RGBW_VERSION_MAJOR  2
#define TILE_DISP_RGBW_VERSION_MINOR  1
#define TILE_DISP_RGBW_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* ---- Instance mapping ---- */

/**
 * | Instance | ID   | Hardware config         |
 * |----------|------|-------------------------|
 * | 0        | 0x50 | LP5811A (Bit4=0,Bit3=0) |
 *
 * @note  The LP5811 has four factory-strapped material variants
 *        (LP5811A/B/C/D) with hard-wired I2C addresses 0x50/0x51/
 *        0x52/0x53. The Display.RGBW tile (rev a) ships with the A
 *        variant only — see the chip-gated note in the multi-address
 *        unsupported annotation.
 */
#define LP5811_I2C_ADDR_DEFAULT  0x50

/* ---- LP5811 registers (page-0 offsets unless noted) ---- */

#define LP5811_REG_CHIP_EN      0x00
#define LP5811_REG_CONFIG_0     0x01
#define LP5811_REG_CONFIG_2     0x03
#define LP5811_REG_CONFIG_12    0x0D
#define LP5811_REG_CMD_UPDATE   0x10
#define LP5811_REG_LED_EN       0x20
#define LP5811_REG_FAULT_CLEAR  0x22
#define LP5811_REG_RESET        0x23
#define LP5811_REG_DC_0         0x30  /* Current limit channel 0 (R) */
#define LP5811_REG_DC_1         0x31  /* Current limit channel 1 (B) */
#define LP5811_REG_DC_2         0x32  /* Current limit channel 2 (G) */
#define LP5811_REG_DC_3         0x33  /* Current limit channel 3 (W) */
#define LP5811_REG_PWM_0        0x40  /* PWM channel 0 (R) */
#define LP5811_REG_PWM_1        0x41  /* PWM channel 1 (B) */
#define LP5811_REG_PWM_2        0x42  /* PWM channel 2 (G) */
#define LP5811_REG_PWM_3        0x43  /* PWM channel 3 (W) */

/* Page-3 status registers (0x300+ — accessed via address-bump trick) */
#define LP5811_REG_TSD_STATUS   0x00  /* page 3, offset 0x00 (=0x300) */
#define LP5811_REG_LOD_STATUS_0 0x01  /* page 3, offset 0x01 (=0x301) */
#define LP5811_REG_LSD_STATUS_0 0x03  /* page 3, offset 0x03 (=0x303) */

#define LP5811_CONFIG_2_DEFAULT 0xE4  /* Used to verify chip is alive */

/* ---- Maximum-current selection (MC bit) ---- */

/**
 * @brief  Per-channel maximum-current selector (LP5811 MC bit).
 *
 * The LP5811 has one global "max current" bit that gates the upper
 * limit for every channel's current sink. Switching the bit also
 * rescales the 8-bit DC code (`tile_display_rgbw_set_current`) over
 * the new range — DC=255 always means full-scale. Default at init: 51 mA.
 */
typedef enum {
    DISP_RGBW_MAX_CURRENT_25_5_MA = 0,  /**< 25.5 mA full scale per channel */
    DISP_RGBW_MAX_CURRENT_51_MA   = 1,  /**< 51 mA full scale per channel */
} disp_rgbw_max_current_t;

/* ---- LED fault status ---- */

/**
 * @brief  LED open / short fault snapshot.
 *
 * Bit N of each mask corresponds to LED channel N. Mapping:
 *   bit0 = R, bit1 = B, bit2 = G, bit3 = W (matches `set()` order
 *   inside the chip). Faults are sticky in the chip — read once,
 *   then call `clear_faults()` to reset the latches.
 */
typedef struct {
    uint8_t open_mask;       /**< Bits 3:0 — channels with open-circuit fault. */
    uint8_t short_mask;      /**< Bits 3:0 — channels with short-circuit fault. */
    uint8_t thermal_shutdown;/**< 1 = chip in thermal shutdown (TSD). */
    uint8_t config_error;    /**< 1 = configuration error reported by chip. */
} disp_rgbw_faults_t;

/**
 * @brief  Short-circuit detection threshold (fraction of VOUT).
 */
typedef enum {
    DISP_RGBW_LSD_TH_0_35 = 0,  /**< 0.35 × VOUT (most sensitive) */
    DISP_RGBW_LSD_TH_0_45 = 1,  /**< 0.45 × VOUT */
    DISP_RGBW_LSD_TH_0_55 = 2,  /**< 0.55 × VOUT */
    DISP_RGBW_LSD_TH_0_65 = 3,  /**< 0.65 × VOUT (least sensitive — driver default) */
} disp_rgbw_lsd_threshold_t;

/* ---- Public API ---- */

/** @brief  Check if a Disp.RGBW is present on the bus. */
uint8_t tile_display_rgbw_find(tiles_pal_t *hal, uint8_t instance);

/**
 * Optional init config. Pass NULL for defaults.
 * Reserved for future use (e.g., initial brightness, current limits).
 */
typedef struct {
    uint8_t reserved;   /**< Placeholder — no options yet. */
} disp_rgbw_cfg_t;

/**
 * @brief  Initialize the LP5811 LED driver.
 *
 * Enables the chip, configures boost voltage to 4.5V, sets max
 * current to 51mA, enables all 4 LED channels, and sets current
 * limits to 50%. LSD action is left at "no shutdown" (driver-level
 * choice) so a transient short doesn't latch the device into OFAF
 * state without firmware seeing it. Pass cfg=NULL for defaults.
 *
 * @param  hal       Platform abstraction handle
 * @param  instance  Device instance (0 = default address 0x50)
 * @param  tile      Tile handle to populate
 * @param  cfg       Optional config, or NULL for defaults
 */
void tile_display_rgbw_init(tiles_pal_t *hal, uint8_t instance, tile_t *tile,
                         const disp_rgbw_cfg_t *cfg);

/**
 * @brief Set RGBW output levels.
 *
 * @studio expose category=tile icon=◑ name=set section=runtime
 * @param r [0..255] Red PWM.
 * @param g [0..255] Green PWM.
 * @param b [0..255] Blue PWM.
 * @param w [0..255] White PWM.
 */
void tile_display_rgbw_set(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * @brief Turn all LEDs off (PWM = 0).
 *
 * @studio expose category=tile icon=◑ name=off section=runtime
 */
void tile_display_rgbw_off(tile_t *tile);

/**
 * @brief Set per-channel current limit.
 *
 * @studio expose category=tile icon=◑ name=set_current section=runtime
 * @param r [0..255] Red current (fraction of full-scale max).
 * @param g [0..255] Green current.
 * @param b [0..255] Blue current.
 * @param w [0..255] White current.
 */
void tile_display_rgbw_set_current(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * @brief Set the global maximum-current range (MC bit).
 *
 * Selects 25.5 mA or 51 mA full-scale per channel. After changing,
 * the 8-bit per-channel DC codes (set via `set_current()`) re-scale
 * automatically — DC=255 always means full-scale current. Useful
 * when wiring lower-rated LEDs (drop to 25.5 mA to keep DC resolution
 * fine) or when running cooler / saving power.
 *
 * The driver writes `Dev_Config_0` and re-issues the `CMD_Update`
 * latch (0x55) the chip requires for config-register writes to
 * actually take effect.
 *
 * @studio expose category=tile icon=◑ name=set_max_current section=config
 * @param  tile  Initialised tile handle
 * @param  mode  25.5 mA (0) or 51 mA (1)
 */
void tile_display_rgbw_set_max_current(tile_t *tile, disp_rgbw_max_current_t mode);

/**
 * @brief Read the per-channel open / short / thermal fault state.
 *
 * Pulls TSD_Config_Status, LOD_Status_0, and LSD_Status_0 from the
 * chip's page-3 register space (the LP5811 multiplexes register
 * pages onto two extra address bits — the driver handles this
 * transparently). Faults latch in the chip until cleared via
 * `clear_faults()`.
 *
 * Open-circuit threshold (VLOD_TH) is fixed by the chip at ~70 mV
 * (25.5 mA mode) or ~180 mV (51 mA mode). Short-circuit threshold
 * (VLSD_TH) is configurable via `set_short_threshold()`.
 *
 * @studio expose category=tile icon=◑ name=read_faults section=runtime
 * @param  tile  Initialised tile handle
 * @param  out   Caller-allocated fault snapshot (zeroed on entry)
 */
void tile_display_rgbw_read_faults(tile_t *tile, disp_rgbw_faults_t *out);

/**
 * @brief Clear all latched LED open / short / TSD fault flags.
 *
 * Writes 0x07 to Fault_Clear (W1C — write 1 to clear). After this
 * call, `read_faults()` reflects only currently-active faults.
 *
 * @studio expose category=tile icon=◑ name=clear_faults section=runtime
 * @param  tile  Initialised tile handle
 */
void tile_display_rgbw_clear_faults(tile_t *tile);

/**
 * @brief Set the short-circuit detection threshold (fraction of VOUT).
 *
 * Lower thresholds catch milder partial-shorts; higher thresholds
 * tolerate more LED forward-voltage variation without false alarms.
 * Driver default (init) is 0.65 × VOUT — most permissive, least
 * likely to mis-fire on cold LEDs whose Vf hasn't settled.
 *
 * @studio expose category=tile icon=◑ name=set_short_threshold section=config
 * @param  tile       Initialised tile handle
 * @param  threshold  One of DISP_RGBW_LSD_TH_*
 */
void tile_display_rgbw_set_short_threshold(tile_t *tile,
                                           disp_rgbw_lsd_threshold_t threshold);

/**
 * @brief Configure whether a short-circuit fault auto-disables outputs.
 *
 * When enabled, an LSD fault sends the chip into OFAF (one-fail-all-
 * fail) state — every channel turns off until LSD_Clear is written.
 * When disabled (driver default), the chip flags the fault but keeps
 * driving — firmware decides what to do.
 *
 * @studio expose category=tile icon=◑ name=set_short_shutdown section=config
 * @param  tile     Initialised tile handle
 * @param  enabled  1 = chip auto-shuts-down on LSD, 0 = report only
 */
void tile_display_rgbw_set_short_shutdown(tile_t *tile, uint8_t enabled);

/**
 * @brief Configure whether an open-circuit fault auto-disables a sink.
 *
 * When enabled (chip default), a per-channel LOD fault turns off
 * that single current sink. When disabled, the chip flags the fault
 * via `read_faults()` but keeps driving the channel — useful when
 * the open is intermittent (e.g., a flexing wire) and firmware
 * wants to retry rather than relying on the chip to recover.
 *
 * @studio expose category=tile icon=◑ name=set_open_shutdown section=config
 * @param  tile     Initialised tile handle
 * @param  enabled  1 = chip auto-shuts-down a single sink, 0 = report only
 */
void tile_display_rgbw_set_open_shutdown(tile_t *tile, uint8_t enabled);

/**
 * @brief Enter sleep (disable chip).
 *
 * @studio expose category=tile icon=◑ name=sleep section=lifecycle
 */
void tile_display_rgbw_sleep(tile_t *tile);

/**
 * @brief Wake (re-enable chip, LEDs retain previous state).
 *
 * @studio expose category=tile icon=◑ name=wake section=lifecycle
 */
void tile_display_rgbw_wake(tile_t *tile);

/**
 * @brief  Software reset. Must call init() again after.
 *
 * @studio expose category=tile icon=◑ name=reset section=lifecycle
 */
void tile_display_rgbw_reset(tile_t *tile);

/* ============================================================== */
/* Runtime — tier-2 idiomatic helpers                              */
/*                                                                  */
/* These compose the tier-1 surface above into "do the thing the   */
/* user wants to do" calls. Most apps drive RGB only and want      */
/* simple verbs — set_color / pulse / breathe / flash — not raw    */
/* PWM channel writes. White is left at zero in these helpers; if  */
/* you need RGBW, drop down to the tier-1 `set()` call.            */
/* ============================================================== */

/**
 * @brief  Set a solid RGB colour (W channel forced off).
 *
 * @studio expose category=tile icon=◑ name=set_color section=runtime
 *
 * Convenience wrapper around @ref tile_display_rgbw_set for the
 * common case where only the RGB channels matter. White is held at
 * zero — drop to `set()` directly if you want to mix white in.
 *
 * @param  tile  Initialised tile handle
 * @param  r     Red PWM   [0..255]
 * @param  g     Green PWM [0..255]
 * @param  b     Blue PWM  [0..255]
 */
void tile_display_rgbw_set_color(tile_t *tile, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Show a colour for `ms` milliseconds, then turn off.
 *
 * @studio expose category=tile icon=◑ name=pulse section=runtime
 *
 * Simple alert / acknowledgement idiom. Blocking — the call returns
 * after `ms` has elapsed and the LEDs have been turned back off. White
 * is forced off (RGB-only); use @ref tile_display_rgbw_set + manual
 * delay if you need full RGBW pulse control.
 *
 * @param  tile  Initialised tile handle
 * @param  r     Red PWM   [0..255]
 * @param  g     Green PWM [0..255]
 * @param  b     Blue PWM  [0..255]
 * @param  ms    Duration in milliseconds
 */
void tile_display_rgbw_pulse(tile_t *tile, uint8_t r, uint8_t g, uint8_t b,
                             uint16_t ms);

/**
 * @brief  Continuous fade in/out (one breath cycle).
 *
 * @studio expose category=tile icon=◑ name=breathe section=runtime
 *
 * The "thinking" indicator. Plays a single up-and-down PWM ramp over
 * `period_ms` (i.e., dark → peak → dark = one full breath). Loop in
 * the caller for sustained breathing. Implementation is a software
 * loop — the LP5811 has on-chip animation engines (AEU) that could
 * run this autonomously, but the AEU bytecode/timing semantics aren't
 * fully documented in the public datasheet (see @ref tile_display_rgbw.h
 * "unsupported AEU" annotation). The software loop is fine for
 * indicator-grade breathing at v2.1; revisit when AEU lands.
 *
 * @note  Blocking. Spends `period_ms` in `delay_ms()`. Call from a
 *        dedicated task or accept the stall — the function does not
 *        yield to other peripherals.
 *
 * @note  White is forced off (RGB-only). Step granularity is fixed
 *        at 32 PWM levels per half-cycle; with `period_ms < 64` the
 *        delay-per-step rounds to 1 ms and the breath becomes choppy.
 *
 * @param  tile        Initialised tile handle
 * @param  r           Peak red PWM   [0..255]
 * @param  g           Peak green PWM [0..255]
 * @param  b           Peak blue PWM  [0..255]
 * @param  period_ms   One full breath duration in milliseconds (>= 64 recommended)
 */
void tile_display_rgbw_breathe(tile_t *tile, uint8_t r, uint8_t g, uint8_t b,
                               uint16_t period_ms);

/**
 * @brief  N quick on/off blinks at the given colour.
 *
 * @studio expose category=tile icon=◑ name=flash section=runtime
 *
 * Notification idiom — alarm, error indication, "got it" ack. Each
 * blink is 100 ms on, 100 ms off (200 ms per cycle). White is forced
 * off (RGB-only). Leaves the LEDs off after the final blink.
 *
 * @note  Blocking. Total runtime is approximately `count * 200` ms.
 *        Call from a dedicated task or accept the stall.
 *
 * @param  tile   Initialised tile handle
 * @param  r      Red PWM   [0..255]
 * @param  g      Green PWM [0..255]
 * @param  b      Blue PWM  [0..255]
 * @param  count  Number of on/off cycles (1..255)
 */
void tile_display_rgbw_flash(tile_t *tile, uint8_t r, uint8_t g, uint8_t b,
                             uint8_t count);

/**
 * @brief  Quick yes/no on whether any LP5811 fault is currently latched.
 *
 * @studio expose category=tile icon=◑ name=is_faulted returns=bool section=runtime
 *
 * Wraps @ref tile_display_rgbw_read_faults and returns 1 if any fault
 * bit is set — open-circuit (LOD), short-circuit (LSD), thermal
 * shutdown (TSD), or configuration error. For per-channel detail
 * (which LED is open / shorted), use `read_faults()` directly. Faults
 * stay latched until cleared via @ref tile_display_rgbw_clear_faults.
 *
 * @param  tile  Initialised tile handle
 * @return 1 if any fault bit is set, 0 if healthy
 */
uint8_t tile_display_rgbw_is_faulted(tile_t *tile);

#endif /* INC_TILE_DISP_RGBW_H_ */
