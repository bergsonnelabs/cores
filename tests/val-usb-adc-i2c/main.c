/**
 * val-usb-adc-i2c -- Validation: USB CDC, ADC, I2C bus, timer tick
 *
 * Core.ST.L4.2, clock=max
 * USB enabled, Pad 4 = I2C1.CLK, Pad 5 = I2C1.DAT, Pad 8 = ADC5
 *
 * Exercises: core_init, core_usb_init, core_usb_printf,
 *            core_i2c_init (h, I2C1, I2C_400K) -- NOT core_i2c_setup,
 *            core_i2c_probe, core_adc_t, core_adc_init, core_adc_read,
 *            core_timer_t, core_timer_init_freq, core_timer_enable_tick
 */

#include "core.h"
#include "core_usb.h"
#include "core_i2c.h"
#include "core_adc.h"
#include "core_timer.h"

static volatile uint32_t tick_count;

static void tick_cb(void *ctx)
{
    (void)ctx;
    tick_count++;
}

int main(void)
{
    core_init();
    core_usb_init();

    /* I2C1 at 400 kHz -- core_i2c_init, NOT core_i2c_setup */
    core_i2c_t i2c;
    core_i2c_init(&i2c, I2C1, I2C_400K);

    /* ADC on pad 8 */
    core_adc_t adc;
    core_adc_init(&adc, ADC_12BIT);
    core_adc_add(&adc, 8, SAMP_MED);

    /* Timer tick at 100 Hz -- callback takes void *ctx */
    core_timer_t tim;
    core_timer_init_freq(&tim, TIM2, 100);
    core_timer_enable_tick(&tim, tick_cb, NULL);
    core_timer_start(&tim);

    while (1) {
        if (core_usb_connected()) {
            uint16_t raw = core_adc_read(&adc, 8);
            hal_status_t probe = core_i2c_probe(&i2c, 0x48);

            core_usb_printf("ADC=%u I2C@0x48=%s ticks=%lu\r\n",
                            raw,
                            (probe == I2C_OK) ? "ACK" : "NACK",
                            tick_count);
        }
        core_delay_ms(500);
    }
}
