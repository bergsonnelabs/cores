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
 * @tessera category led label=Core.LED icon=☀
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
 * Blink the LED n times. Each cycle turns the LED on for on_ms
 * milliseconds and off for off_ms milliseconds. Blocking — returns
 * after the last off period.
 *
 * @tessera expose category=led name=blink
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
 * @tessera expose category=led name=heartbeat
 * @param period_ms [0..60000] ms Delay after the toggle. 0 toggles without waiting.
 */
static inline void core_led_heartbeat(int period_ms)
{
    LED_TOGGLE();
    ll_delay_ms(period_ms);
}

#endif /* CORE_LED_H */
