/**
 * core_dac.h — DAC output (analog voltage generation)
 *
 * Output a 12-bit analog voltage on a DAC-capable pad.
 * Available on Core.H pad 9 (PA5, DAC1 channel 2).
 *
 * Usage:
 *   core_dac_init();
 *   core_dac_write_mv(1650);   // Output 1.65V
 *   core_dac_write(2048);      // Raw 12-bit value
 */

#ifndef CORE_DAC_H
#define CORE_DAC_H

#if defined(STM32H523xx)

#include "ll_dac.h"
#include "ll_rcc.h"
#include "ll_gpio.h"

/**
 * Initialize the DAC. Enables clock, configures the output pin
 * as analog, and starts the DAC channel.
 */
static inline void core_dac_init(void)
{
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_gpio_config_analog(GPIOA, 5);  /* PA5 = pad 9 = DAC1_OUT2 */
    ll_rcc_ahb2_clk_enable(LL_AHB2_DAC1);
    ll_dac_init();
}

/** Write a raw 12-bit value (0–4095). Output = val / 4095 × VREF+. */
static inline void core_dac_write(uint16_t val)
{
    ll_dac_write(val);
}

/** Write a voltage in millivolts (0–3300). Assumes VREF+ = 3.3V. */
static inline void core_dac_write_mv(uint16_t mv)
{
    ll_dac_write_mv(mv);
}

/** Read back the current DAC output register value (12-bit). */
static inline uint16_t core_dac_read(void)
{
    return ll_dac_read();
}

#endif /* STM32H523xx */

#endif /* CORE_DAC_H */
