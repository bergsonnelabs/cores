/**
 * hal_dac.h — DAC HAL driver
 *
 * 12-bit DAC output. Handles clock enable, GPIO config, and
 * peripheral init based on the channel/pin provided.
 *
 * Usage (via core_ API — see core_dac.h):
 *   core_dac_init();
 *   core_dac_write_mv(1650);
 */

#ifndef HAL_DAC_H
#define HAL_DAC_H

#include "hal_common.h"
#include "ll_dac.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include <stdint.h>

#if defined(STM32H523xx)

/**
 * DAC handle — stores the channel and pin config set by coregen.
 */
typedef struct {
    GPIO_TypeDef *port;     /* GPIO port for DAC output pin */
    uint8_t       pin;      /* GPIO pin number */
    uint8_t       channel;  /* DAC channel (1 or 2) */
} hal_dac_t;

/**
 * Initialize the DAC.
 * Enables clock, configures the GPIO pin as analog, and starts the channel.
 */
static inline void hal_dac_init(hal_dac_t *dac)
{
    ll_rcc_gpio_clk_enable(dac->port);
    ll_gpio_config_analog(dac->port, dac->pin);
    ll_rcc_ahb2_clk_enable(LL_AHB2_DAC1);
    ll_dac_init();
}

/** Write a raw 12-bit value (0–4095). */
static inline void hal_dac_write(hal_dac_t *dac, uint16_t val)
{
    (void)dac;
    ll_dac_write(val);
}

/** Write a voltage in millivolts (0–3300, assumes VREF+ = 3.3V). */
static inline void hal_dac_write_mv(hal_dac_t *dac, uint16_t mv)
{
    (void)dac;
    ll_dac_write_mv(mv);
}

/** Read back the current DAC output register value (12-bit). */
static inline uint16_t hal_dac_read(hal_dac_t *dac)
{
    (void)dac;
    return ll_dac_read();
}

#endif /* STM32H523xx */

#endif /* HAL_DAC_H */
