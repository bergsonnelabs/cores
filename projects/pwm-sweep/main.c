/**
 * PWM Sweep — HAL timer test
 *
 * Ramps PWM duty cycle 0% → 100% → 0% on Pad 7 (TIM2.CH3)
 * at 1kHz. On a scope you'll see the pulse width smoothly
 * grow and shrink. With an LED on the pad you'd see a
 * breathing effect.
 *
 * Uses hal_timer for PWM setup and duty control,
 * hal_gpio for the onboard LED as a heartbeat.
 */

#include "core_init.h"
#include "core_config.h"
#include "core_board.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "ll_rcc.h"
#include "ll_systick.h"

int main(void)
{
    core_init();
    ll_systick_init(SYSCLK_HZ);

    /* Set up TIM2 for 1kHz PWM */
    ll_rcc_apb1_clk_enable(LL_APB1_TIM2);
    hal_timer_t pwm;
    hal_timer_pwm_init(&pwm, TIM2, SYSCLK_HZ, 1000);
    hal_timer_pwm_set_duty(&pwm, 3, 0);
    hal_timer_pwm_start(&pwm);

    /* LED heartbeat */
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    uint16_t duty = 0;
    int8_t direction = 1;  /* 1 = ramping up, -1 = ramping down */

    while (1) {
        hal_timer_pwm_set_duty(&pwm, 3, duty);

        duty += direction * 2;  /* Step by 0.2% */
        if (duty >= 1000) {
            duty = 1000;
            direction = -1;
        } else if (duty == 0) {
            direction = 1;
            LED_TOGGLE();  /* Toggle LED at each breath cycle */
        }

        ll_delay_ms(2);  /* ~1 second per full ramp */
    }
}
