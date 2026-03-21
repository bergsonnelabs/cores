/**
 * PWM — 1kHz PWM output on Pad 7
 *
 * Outputs a 1kHz square wave at 50% duty cycle on Pad 7 (PA2 / TIM2.CH3).
 * Connect an oscilloscope to Pad 7 to verify.
 */

#include "tile_init.h"
#include "tile_config.h"
#include "tile_pins.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_tim.h"

int main(void)
{
    /* Clock tree: HSI16 → PLL → 80MHz */
    tile_init();

    /* Enable TIM2 peripheral clock (APB1) */
    ll_rcc_apb1_clk_enable(LL_APB1_TIM2);

    /* Configure TIM2 for 1kHz overflow:
       80MHz / (79+1) = 1MHz tick
       1MHz  / (999+1) = 1kHz overflow */
    ll_tim_config(TIM2, 79, 999);

    /* Configure channel 3 for PWM at 50% duty */
    ll_tim_pwm_config(TIM2, 3, 500);

    /* Start the timer */
    ll_tim_start(TIM2);

    /* Nothing else to do — PWM runs in hardware */
    while (1)
        ;
}
