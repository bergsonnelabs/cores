/**
 * hal_led.h — Core LED pattern helpers
 *
 * Uses LED_ON/OFF/TOGGLE from core_board.h and ll_delay_ms
 * from ll_systick.h. Header-only — no .c file needed.
 *
 * All functions use the core_ prefix since the LED belongs
 * to the Core tile, not child tiles.
 */

#ifndef HAL_LED_H
#define HAL_LED_H

#include "core_board.h"
#include "ll_systick.h"
#include "ll_rcc.h"
#include "ll_gpio.h"

/** Initialize the onboard LED GPIO. Call once after core_clock_init(). */
static inline void core_led_init(void)
{
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);
    LED_OFF();
}

/** Blink the LED n times. */
static inline void core_led_blink(int n, int on_ms, int off_ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  ll_delay_ms(on_ms);
        LED_OFF(); ll_delay_ms(off_ms);
    }
}

/** SOS pattern — infinite loop (call on fatal error). */
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

/** Single heartbeat toggle — call in main loop for alive indicator. */
static inline void core_led_heartbeat(int period_ms)
{
    LED_TOGGLE();
    ll_delay_ms(period_ms);
}

#endif /* HAL_LED_H */
