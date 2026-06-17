/**
 * val-power-u -- Validation: Sleep, stop, standby on Core.U
 *
 * Core.ST.L4.2, clock=max
 * Pad 8 = GPIO.IN with EXTI falling edge
 *
 * Exercises: core_init, core_sleep, core_stop_for,
 *            core_pad_on_change (pad, EDGE_FALLING, cb, NULL),
 *            core_woke_from_standby, core_clear_standby_flag
 *
 * wakeup_cb signature: void(void *ctx)
 */

#include "core.h"
#include "core_power.h"
#include "core_gpio.h"

static volatile uint32_t wakeup_count;

/* Wakeup callback -- signature: void(void *ctx) */
static void wakeup_cb(void *ctx)
{
    (void)ctx;
    wakeup_count++;
}

int main(void)
{
    core_init();
    core_led_init();

    /* Check if we woke from standby (full reset on standby wake) */
    if (core_woke_from_standby()) {
        core_clear_standby_flag();
        /* Indicate standby recovery with rapid blink */
        for (int i = 0; i < 6; i++) {
            LED_TOGGLE();
            core_delay_ms(100);
        }
    }

    /* EXTI on pad 8 -- falling edge wakeup */
    core_pad_on_change(8, EDGE_FALLING, wakeup_cb, NULL);

    /* Light sleep -- wakes on any interrupt */
    core_sleep();
    LED_TOGGLE();

    /* Stop mode with RTC wakeup after 3 seconds */
    core_stop_for(3);
    LED_TOGGLE();

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);
    }
}
