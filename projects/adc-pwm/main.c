/**
 * ADC → PWM — Read analog input, mirror as PWM duty cycle
 *
 * Pad 8 (PA0): ADC input — connect to a voltage divider or
 *              touch to VCC/GND to test.
 * Pad 7 (PA2): PWM output at 1kHz, duty tracks ADC reading.
 *
 * Duty is clamped 10%–90% so the PWM signal is always visible.
 */

#include "tile_init.h"
#include "tile_config.h"
#include "tile_board.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "hal_adc.h"
#include "ll_rcc.h"
#include "ll_systick.h"

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);

    /* ADC on Pad 8 (PA0, channel 5) */
    hal_pad_analog(8);
    hal_adc_t adc;
    hal_adc_init(&adc, ADC1);

    /* PWM on Pad 7 (TIM2.CH3) */
    ll_rcc_apb1_clk_enable(LL_APB1_TIM2);
    hal_timer_t pwm;
    hal_timer_pwm_init(&pwm, TIM2, SYSCLK_HZ, 1000);
    hal_timer_pwm_set_duty(&pwm, 3, 500);
    hal_timer_pwm_start(&pwm);

    /* LED heartbeat */
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);
    uint8_t loop_count = 0;

    while (1) {
        uint16_t raw = hal_adc_read_avg(&adc, 5, 8);
        uint16_t duty = 100 + (uint16_t)((uint32_t)raw * 800 / 4095);
        hal_timer_pwm_set_duty(&pwm, 3, duty);

        if (++loop_count >= 50) {
            LED_TOGGLE();
            loop_count = 0;
        }

        ll_delay_ms(10);
    }
}
