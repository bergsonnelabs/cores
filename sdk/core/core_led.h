/**
 * core_led.h — Onboard LED pattern helpers
 *
 * Every Core tile has an onboard LED connected to a dedicated GPIO pin.
 * This module provides a lightweight, header-only API for common patterns
 * like blinking, heartbeat, and SOS error signaling. The LED pin is
 * auto-detected from the tile definition and configured by core_led_init()
 * — it's not part of the configurable pad map.
 *
 * Uses LED_ON/OFF/TOGGLE from core_board.h and ll_delay_ms from ll_systick.h.
 *
 * @studio category led label=Core.LED icon=☀
 *
 * @studio coverage
 *   id:    led
 *   name:  LED — onboard status LED
 *   page:  /docs/sdk/led
 *   blurb: Header-only helpers for the dedicated onboard LED (auto-
 *          detected from the tile definition — not part of the pad
 *          map). Tier 2 covers on / off / toggle / blink / heartbeat;
 *          the simulator renders the LED state in real time, so DSL
 *          programs can iterate on blink patterns without flashing
 *          hardware. SOS pattern is Tier 1 (escape-to-C only — it's
 *          a `noreturn` blocker).
 */

#ifndef CORE_LED_H
#define CORE_LED_H

#include "core_board.h"
#include "ll_systick.h"
#include "ll_rcc.h"
#include "ll_gpio.h"

/**
 * Enable the GPIO clock and configure the LED pin as a push-pull output.
 * Call once after core_init(). The LED starts in the off state.
 */
static inline void core_led_init(void)
{
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);
    LED_OFF();
}

/**
 * Turn the onboard LED on. Simplest possible light control — useful
 * alongside blink/heartbeat when the user wants explicit on/off state
 * rather than a timed pattern.
 *
 * @studio expose category=led name=on
 * @studio twin full
 */
static inline void core_led_on(void)
{
    LED_ON();
}

/**
 * Turn the onboard LED off.
 *
 * @studio expose category=led name=off
 * @studio twin full
 */
static inline void core_led_off(void)
{
    LED_OFF();
}

/**
 * Flip the onboard LED's current state. Non-blocking — no delay.
 * Good for driving the LED from an event handler that fires at a
 * rate you've already chosen (e.g., a pad-edge ISR on a button).
 *
 * @studio expose category=led name=toggle
 * @studio twin full
 */
static inline void core_led_toggle(void)
{
    LED_TOGGLE();
}

/**
 * Blink the LED n times. Each cycle turns the LED on for on_ms
 * milliseconds and off for off_ms milliseconds. Blocking — returns
 * after the last off period.
 *
 * @studio expose category=led name=blink
 * @studio twin full
 * @param n [1..100] Number of times to blink.
 * @param on_ms [1..5000] ms LED-on duration per blink.
 * @param off_ms [1..5000] ms LED-off duration per blink.
 */
static inline void core_led_blink(int n, int on_ms, int off_ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  ll_delay_ms(on_ms);
        LED_OFF(); ll_delay_ms(off_ms);
    }
}

/**
 * Blink the SOS pattern (3 short, 3 long, 3 short) in an infinite loop.
 * Use for unrecoverable errors. This function never returns.
 */
static inline void core_led_sos(void) __attribute__((noreturn));
static inline void core_led_sos(void)
{
    while (1) {
        core_led_blink(3, 100, 100);
        core_led_blink(3, 400, 400);
        core_led_blink(3, 100, 100);
        ll_delay_ms(2000);
    }
}

/**
 * Toggle the LED and delay for period_ms. Call this in your main loop
 * for a continuous alive indicator. Vary the period to signal different
 * states (e.g., 50ms = fast/active, 500ms = idle).
 *
 * @studio expose category=led name=heartbeat
 * @studio twin full
 * @param period_ms [0..60000] ms Delay after the toggle. 0 toggles without waiting.
 */
static inline void core_led_heartbeat(int period_ms)
{
    LED_TOGGLE();
    ll_delay_ms(period_ms);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @studio unsupported tier=2 value=L title="No DSL access to SOS"
//   core_led_sos is `__attribute__((noreturn))` so it can't be
//   exposed safely — DSL programs that "call SOS" would deadlock
//   the worker. Reserved for fault-handler escape-to-C use.
//
// @studio unsupported tier=1 value=L title="No PWM brightness / color"
//   The onboard LED is hard-wired as a digital push-pull output. No
//   PWM dimming wrapper, no RGB/RGBW support (tiles with addressable
//   LEDs use Disp.RGBW or similar, not core_led).

#endif /* CORE_LED_H */
