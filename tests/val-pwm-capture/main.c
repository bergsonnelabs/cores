/**
 * val-pwm-capture -- Validation: Timer PWM output + input capture
 *
 * Core.U (Core-U-2-a), clock=max
 * Pad 7 = TIM15.1, Pad 8 = TIM2.1
 *
 * Exercises: core_timer_t, core_timer_init_freq, core_timer_pwm_set (permil!),
 *            core_timer_start, core_timer_capture_init, core_timer_capture_read,
 *            core_pwm_init (no pclk_hz!), core_pwm_set (permil!)
 */

#include "core.h"
#include "core_timer.h"
#include "core_pwm.h"

int main(void)
{
    core_init();
    core_led_init();

    /* ---- TIM15 on pad 7: PWM at 1 kHz ---- */
    core_timer_t tim15;
    core_timer_init_freq(&tim15, TIM15, 1000);
    core_timer_pwm_set(&tim15, 1, 500);   /* 50% duty -- permil, NOT percent */
    core_timer_start(&tim15);

    /* ---- TIM2 on pad 8: capture + PWM ---- */

    /* First, use core_timer API for capture */
    core_timer_t tim2_cap;
    core_timer_init_freq(&tim2_cap, TIM2, 1000);
    core_timer_capture_init(&tim2_cap, 1);
    core_timer_start(&tim2_cap);

    core_delay_ms(100);
    uint32_t cap_val = core_timer_capture_read(&tim2_cap, 1);
    (void)cap_val;
    core_timer_stop(&tim2_cap);

    /* Now, use core_pwm API on TIM2 -- NO pclk_hz parameter */
    core_timer_t tim2_pwm;
    core_pwm_init(&tim2_pwm, TIM2, 2000);         /* 2 kHz, no pclk_hz! */
    core_pwm_set(&tim2_pwm, 1, 750);              /* 75% duty -- permil */
    core_pwm_start(&tim2_pwm);

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);
    }
}
