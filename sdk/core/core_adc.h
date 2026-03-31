/**
 * core_adc.h — Pad-level ADC reads
 *
 * The primary ADC API. All functions use tile pad numbers.
 * Channels, instances, and clocks are resolved automatically
 * from the project configuration.
 */

#ifndef CORE_ADC_H
#define CORE_ADC_H

#include "tal_adc.h"

/* ============================================================
 * Sampling speed aliases
 * ============================================================ */

#define SAMP_FAST      HAL_ADC_SAMP_FAST
#define SAMP_MED       HAL_ADC_SAMP_MED
#define SAMP_SLOW      HAL_ADC_SAMP_SLOW
#define SAMP_VERY_SLOW HAL_ADC_SAMP_VERY_SLOW

/* ============================================================
 * Resolution aliases
 * ============================================================ */

#define ADC_6BIT   HAL_ADC_RES_6BIT
#define ADC_8BIT   HAL_ADC_RES_8BIT
#define ADC_10BIT  HAL_ADC_RES_10BIT
#define ADC_12BIT  HAL_ADC_RES_12BIT
#define ADC_14BIT  HAL_ADC_RES_14BIT

/* ============================================================
 * Init
 * ============================================================ */

/** Initialise the ADC at the given resolution. */
static inline hal_status_t core_adc_init(hal_adc_t *adc, uint32_t resolution)
{
    return tal_adc_init(adc, (hal_adc_res_t)resolution);
}

/* ============================================================
 * Channel registration
 * ============================================================ */

/** Register a pad as an ADC input with the given sampling speed. */
static inline hal_status_t core_adc_add(hal_adc_t *adc, uint8_t pad,
                                         uint32_t samp)
{
    return tal_adc_add_pad(adc, pad, (hal_adc_samp_t)samp);
}

/* ============================================================
 * Blocking reads
 * ============================================================ */

/** Single-shot read — returns raw ADC count. */
static inline uint16_t core_adc_read(hal_adc_t *adc, uint8_t pad)
{
    return tal_adc_read_pad(adc, pad);
}

/** Single-shot read — returns calibrated millivolts. */
static inline uint32_t core_adc_read_mv(hal_adc_t *adc, uint8_t pad)
{
    return tal_adc_read_pad_mv(adc, pad);
}

/* ============================================================
 * Internal channels
 * ============================================================ */

/** Die temperature in tenths of deg C (e.g. 253 = 25.3 C). */
static inline int32_t core_adc_temp(hal_adc_t *adc)
{
    return tal_adc_read_temp_decidegc(adc);
}

/** Actual VDD supply voltage in millivolts via VREFINT. */
static inline uint32_t core_adc_vdd(hal_adc_t *adc)
{
    return tal_adc_read_vdda_mv(adc);
}

/* ============================================================
 * DMA / continuous mode
 * ============================================================ */

/** Start continuous conversion with DMA circular buffer. */
static inline hal_status_t core_adc_start_dma(hal_adc_t *adc, uint16_t *buf,
                                               uint16_t len,
                                               void (*cb)(void))
{
    return tal_adc_start_dma(adc, buf, len, cb);
}

/** Stop continuous DMA conversion. */
static inline void core_adc_stop_dma(hal_adc_t *adc)
{
    tal_adc_stop_dma(adc);
}

/** Read most recent DMA result for a pad. */
static inline uint16_t core_adc_dma_read(hal_adc_t *adc, uint8_t pad)
{
    return tal_adc_dma_read_pad(adc, pad);
}

#endif /* CORE_ADC_H */
