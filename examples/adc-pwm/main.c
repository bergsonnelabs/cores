/**
 * ADC → PWM — Read analog input, mirror as PWM duty cycle
 *
 * Pad 8 (PA0, ADC channel 5): analog input — connect to a voltage divider or
 *                              touch to VCC/GND to test.
 * Pad 7 (PA2, TIM2.CH3):      PWM output at 1 kHz, duty tracks ADC reading.
 *
 * Duty is clamped 10%–90% so the PWM signal is always visible.
 */

#include "core.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_timer.h"

int main(void)
{
    core_init();
    core_led_init();

    /* ADC on Pad 8 (PA0, channel 5) */
    hal_pad_analog(8);
    hal_adc_t adc;
    hal_adc_init(&adc, ADC1, SYSCLK_HZ, HAL_ADC_RES_12BIT);
    hal_adc_add_channel(&adc, 5, HAL_ADC_SAMP_SLOW);

    /* PWM on Pad 7 (TIM2.CH3) — coregen enables TIM2 clock via pads config */
    hal_timer_t pwm;
    hal_timer_pwm_init(&pwm, TIM2, SYSCLK_HZ, 1000);
    hal_timer_pwm_set_duty(&pwm, 3, 500);
    hal_timer_pwm_start(&pwm);

    uint8_t loop_count = 0;

    while (1) {
        uint16_t raw  = hal_adc_read(&adc, 5);
        /* Scale 0–4095 → 100–900 (10%–90% of 1000-unit period) */
        uint16_t duty = 100 + (uint16_t)((uint32_t)raw * 800 / 4095);
        hal_timer_pwm_set_duty(&pwm, 3, duty);

        if (++loop_count >= 50) {
            LED_TOGGLE();
            loop_count = 0;
        }

        ll_delay_ms(10);
    }
}
