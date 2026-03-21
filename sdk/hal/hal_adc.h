/**
 * hal_adc.h — ADC HAL driver
 *
 * Single-shot reads, averaged reads, pad-based convenience.
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "hal_common.h"
#include "ll_adc.h"

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    ADC_TypeDef *instance;
    uint32_t     vref_mv;       /* Reference voltage in mV (default 3300) */
} hal_adc_t;

/* ============================================================
 * API declarations (implemented in hal_adc.c)
 * ============================================================ */

/**
 * Initialize ADC with calibration.
 * The peripheral clock is auto-enabled. ADC pins must be
 * configured as analog (hal_pad_analog or ll_gpio_config_analog).
 */
hal_status_t hal_adc_init(hal_adc_t *h, ADC_TypeDef *instance);

void hal_adc_deinit(hal_adc_t *h);

/** Set reference voltage (default 3300mV) */
void hal_adc_set_vref(hal_adc_t *h, uint32_t vref_mv);

/* ---- Single-shot reads ---- */

/** Read a single ADC channel. Returns raw 12-bit value (0–4095). */
uint16_t hal_adc_read(hal_adc_t *h, uint8_t channel);

/** Read and convert to millivolts. */
uint32_t hal_adc_read_mv(hal_adc_t *h, uint8_t channel);

/** Read with averaging (multiple samples). */
uint16_t hal_adc_read_avg(hal_adc_t *h, uint8_t channel, uint8_t samples);

/** Read averaged and convert to millivolts. */
uint32_t hal_adc_read_avg_mv(hal_adc_t *h, uint8_t channel, uint8_t samples);

/* ---- Pad-based convenience ---- */

/**
 * Read an ADC value by pad number.
 * Automatically looks up the ADC channel for the given pad.
 * Returns raw 12-bit value, or 0 if the pad has no ADC channel.
 *
 * Note: The pad must be configured as analog first (hal_pad_analog).
 */
uint16_t hal_adc_read_pad(hal_adc_t *h, uint8_t pad);

/** Read pad and convert to millivolts. */
uint32_t hal_adc_read_pad_mv(hal_adc_t *h, uint8_t pad);

#endif /* HAL_ADC_H */
