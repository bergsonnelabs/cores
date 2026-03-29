/**
 * adc-tier1 — ADC HAL Tier 1 demo
 *
 * Targets Core.U.2 (STM32L422TB, Core-U-2-a).
 *
 * Demonstrates:
 *   1. hal_adc_read_mv()          — VREFINT-calibrated millivolt read on a pad
 *   2. hal_adc_read_temp_decidegc() — die temperature via TS_CAL calibration
 *   3. hal_adc_set_oversample()   — hardware oversampling for noise reduction
 *   4. DMA circular buffer pattern (commented — shows the call pattern)
 *
 * Hardware connections:
 *   Pad 3 (PA1, ADC channel 6) — connect to a voltage source (0–3.3V).
 *   Output goes to SWO/ITM (pin 14) at 2 Mbit/s.
 *   Use 'make swo' to observe via OpenOCD.
 *
 * Note: the ADC input pad must be configured as analog before reading.
 *       hal_pad_analog(3) does this for pad 3.
 */

#include "core.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_debug.h"

/* ---- DMA demo buffer (not started in this example, just declared) ---- */
static uint16_t dma_buf[32];  /* 2 channels × 16 conversions */

/* ---- DMA callback stub ---- */
static void on_adc_dma(void)
{
    /* Called at half-complete and complete.
     * Double-buffer pattern:
     *   half-complete → process dma_buf[0..15]
     *   complete      → process dma_buf[16..31]
     * Use a flag or ping-pong counter to tell them apart. */
}

int main(void)
{
    core_init();
    core_led_init();

    /* ---- 1. Configure pad 3 (PA1, ADC channel 6) as analog input ---- */
    hal_pad_analog(3);

    /* ---- 2. Initialise ADC1 at 12-bit resolution ---- */
    hal_adc_t adc;
    hal_adc_init(&adc, ADC1, SYSCLK_HZ, HAL_ADC_RES_12BIT);

    /* ---- 3. Add the pad channel (channel 6 on Core.U.2, pad 3) ---- */
    hal_adc_add_channel(&adc, 6, HAL_ADC_SAMP_MED);

    /* ---- 4. Enable 64× hardware oversampling (+3 effective bits) ---- */
    hal_adc_set_oversample(&adc, HAL_ADC_OVERSAMPLE_64X);

    /* ---- DMA pattern (not started here — shows the call) ----
     *
     * hal_adc_add_channel(&adc, 6, HAL_ADC_SAMP_MED);
     * hal_adc_add_channel(&adc, 17, HAL_ADC_SAMP_SLOW);   // TEMP channel
     * hal_adc_start_dma(&adc, dma_buf, 32, on_adc_dma);
     *
     * // ... later, to stop:
     * hal_adc_stop_dma(&adc);
     */
    (void)dma_buf;
    (void)on_adc_dma;

    hal_debug_init(SYSCLK_HZ);

    while (1) {
        /* Read pad 3 in millivolts (VREFINT-calibrated, supply-independent) */
        uint32_t mv = hal_adc_read_mv(&adc, 6);

        /* Read die temperature in tenths of a degree C */
        int32_t temp_dc = hal_adc_read_temp_decidegc(&adc);

        /* Read actual VDD supply (useful on battery-powered designs) */
        uint32_t vdda_mv = hal_adc_read_vdda_mv(&adc);

        hal_debug_printf("pad3=%lumV  temp=%ld.%lddegC  vdda=%lumV\n",
                         mv,
                         temp_dc / 10, temp_dc < 0 ? -(temp_dc % 10) : (temp_dc % 10),
                         vdda_mv);

        LED_TOGGLE();
        ll_delay_ms(500);
    }
}
