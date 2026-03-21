/**
 * hal_adc.c — ADC HAL driver implementation
 */

#include "hal_adc.h"
#include "ll_rcc.h"
#include "tile_pins.h"
#include <string.h>

/* ---- ADC clock enable ---- */

static void _adc_clk_enable(ADC_TypeDef *instance)
{
    (void)instance;
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x34UL), (1UL << 9));   /* APB2ENR: ADCEN */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x4CUL), (1UL << 13));  /* AHB2ENR: ADCEN */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << 10));  /* AHB2ENR: ADC4EN */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << 10));  /* AHB2ENR: ADCEN */
#endif
    (void)REG32(RCC_BASE);
}

/* ============================================================
 * Pad-to-ADC channel mapping
 *
 * This maps tile pad numbers to ADC channel numbers.
 * The mapping depends on which GPIO pin each pad is connected to,
 * and which ADC channel that pin serves.
 *
 * For the STM32L422 (Core.U tiles):
 *   PA0 = ADC_IN5,  PA1 = ADC_IN6,  PA2 = ADC_IN7,  PA3 = ADC_IN8
 *   PA4 = ADC_IN9,  PA5 = ADC_IN10, PA6 = ADC_IN11, PA7 = ADC_IN12
 *   PB0 = ADC_IN15, PB1 = ADC_IN16
 * ============================================================ */

static int _pad_to_adc_channel(uint8_t pad)
{
    /* Use the PAD_n_PORT and PAD_n_PIN defines to figure out the channel.
       We do this with a switch on pad number and compile-time port/pin checks. */
    switch (pad) {
#if defined(STM32L422xx)
    /* Map known ADC pins for L422 */
#ifdef PAD_8_PORT   /* PA0 */
    case 8:  return 5;
#endif
#ifdef PAD_3_PORT   /* PA1 */
    case 3:  return 6;
#endif
#ifdef PAD_7_PORT   /* PA2 */
    case 7:  return 7;
#endif
#ifdef PAD_6_PORT   /* PA3 */
    case 6:  return 8;
#endif
#ifdef PAD_19_PORT  /* PA4 */
    case 19: return 9;
#endif
#ifdef PAD_10_PORT  /* PA5 */
    case 10: return 10;
#endif
#ifdef PAD_9_PORT   /* PA6 */
    case 9:  return 11;
#endif
#ifdef PAD_2_PORT   /* PA7 */
    case 2:  return 12;
#endif
#ifdef PAD_11_PORT  /* PB0 */
    case 11: return 15;
#endif
#ifdef PAD_15_PORT  /* PB1 */
    case 15: return 16;
#endif
#endif /* STM32L422xx */
    default: return -1;
    }
}

/* ============================================================
 * Init / Deinit
 * ============================================================ */

hal_status_t hal_adc_init(hal_adc_t *h, ADC_TypeDef *instance)
{
    memset(h, 0, sizeof(*h));
    h->instance = instance;
    h->vref_mv = 3300;

    _adc_clk_enable(instance);
    ll_adc_init(instance, LL_ADC_SMPR_39_5);  /* Medium sampling time */
    return HAL_OK;
}

void hal_adc_deinit(hal_adc_t *h)
{
    if (h && h->instance) {
        ll_adc_disable(h->instance);
        h->instance = NULL;
    }
}

void hal_adc_set_vref(hal_adc_t *h, uint32_t vref_mv)
{
    h->vref_mv = vref_mv;
}

/* ============================================================
 * Single-shot reads
 * ============================================================ */

uint16_t hal_adc_read(hal_adc_t *h, uint8_t channel)
{
    return ll_adc_read(h->instance, channel);
}

uint32_t hal_adc_read_mv(hal_adc_t *h, uint8_t channel)
{
    return ll_adc_read_mv(h->instance, channel, h->vref_mv);
}

uint16_t hal_adc_read_avg(hal_adc_t *h, uint8_t channel, uint8_t samples)
{
    if (samples == 0) samples = 1;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += ll_adc_read(h->instance, channel);
    }
    return (uint16_t)(sum / samples);
}

uint32_t hal_adc_read_avg_mv(hal_adc_t *h, uint8_t channel, uint8_t samples)
{
    uint16_t raw = hal_adc_read_avg(h, channel, samples);
    return ((uint32_t)raw * h->vref_mv) / 4095;
}

/* ============================================================
 * Pad-based convenience
 * ============================================================ */

uint16_t hal_adc_read_pad(hal_adc_t *h, uint8_t pad)
{
    int ch = _pad_to_adc_channel(pad);
    if (ch < 0) return 0;
    return hal_adc_read(h, (uint8_t)ch);
}

uint32_t hal_adc_read_pad_mv(hal_adc_t *h, uint8_t pad)
{
    int ch = _pad_to_adc_channel(pad);
    if (ch < 0) return 0;
    return hal_adc_read_mv(h, (uint8_t)ch);
}
