/**
 * ll_dac.h — Low-level DAC (Digital-to-Analog Converter)
 *
 * Minimal driver for the 12-bit DAC on STM32H5. Single channel,
 * direct output to a GPIO pin (PA5 on Core.ST.H5 pad 9).
 *
 * Usage:
 *   ll_rcc_ahb2_clk_enable(LL_AHB2_DAC1);
 *   ll_gpio_config_analog(GPIOA, 5);       // DAC pin must be analog
 *   ll_dac_init();
 *   ll_dac_write(2048);                     // mid-scale → ~1.65V
 */

#ifndef LL_DAC_H
#define LL_DAC_H

#include "ll_common.h"

#if defined(STM32H523xx)

/* DAC1 base address: AHB2 + 0x08400 */
#define DAC1_BASE           0x42028400UL

/* DAC registers */
#define DAC_CR              REG32(DAC1_BASE + 0x00UL)
#define DAC_DHR12R1         REG32(DAC1_BASE + 0x08UL)   /* Channel 1 (PA4) */
#define DAC_DHR12R2         REG32(DAC1_BASE + 0x14UL)   /* Channel 2 (PA5) */
#define DAC_DOR1            REG32(DAC1_BASE + 0x2CUL)
#define DAC_DOR2            REG32(DAC1_BASE + 0x30UL)
#define DAC_MCR             REG32(DAC1_BASE + 0x44UL)

/* CR bits — channel 1 (PA4) */
#define DAC_CR_EN1          (1UL << 0)
#define DAC_CR_TEN1         (1UL << 1)

/* CR bits — channel 2 (PA5, pad 9 on Core.ST.H5) */
#define DAC_CR_EN2          (1UL << 16)
#define DAC_CR_TEN2         (1UL << 17)

/* MCR MODE bits — channel 1 [2:0], channel 2 [18:16] */
#define DAC_MCR_MODE1_MASK  (7UL << 0)
#define DAC_MCR_MODE2_MASK  (7UL << 16)
#define DAC_MCR_MODE1_NOBUF (2UL << 0)
#define DAC_MCR_MODE2_NOBUF (2UL << 16)

/* AHB2 clock enable bit for DAC1 */
#define LL_AHB2_DAC1        (1UL << 11)

/**
 * Initialize DAC channel 2 (PA5, pad 9 on Core.ST.H5).
 * Enables the DAC with output buffer connected to the pin.
 * GPIO must already be configured as analog.
 * AHB2 clock must be enabled: ll_rcc_ahb2_clk_enable(LL_AHB2_DAC1).
 */
static inline void ll_dac_init(void)
{
    /* Channel 2 (PA5, pad 9): buffer disabled, connected to external pin */
    MOD_BITS(DAC_MCR, DAC_MCR_MODE2_MASK, DAC_MCR_MODE2_NOBUF);

    /* Enable channel 2 (free-running, no trigger) */
    SET_BITS(DAC_CR, DAC_CR_EN2);

    /* Small stabilization delay (~1µs) */
    for (volatile int i = 0; i < 100; i++) ;
}

/**
 * Write a 12-bit value to DAC channel 2 (PA5).
 * Output voltage = (val / 4095) × VREF+ (typically 3.3V).
 *
 * @param val  0–4095
 */
static inline void ll_dac_write(uint16_t val)
{
    DAC_DHR12R2 = val & 0xFFF;
}

/**
 * Write a voltage in millivolts to DAC channel 2 (PA5).
 * Assumes VREF+ = 3300 mV.
 *
 * @param mv  0–3300
 */
static inline void ll_dac_write_mv(uint16_t mv)
{
    uint32_t val = ((uint32_t)mv * 4095 + 1650) / 3300;
    if (val > 4095) val = 4095;
    DAC_DHR12R2 = val;
}

/**
 * Read back the current DAC channel 2 (PA5) output register value.
 */
static inline uint16_t ll_dac_read(void)
{
    return (uint16_t)(DAC_DOR2 & 0xFFF);
}

/**
 * Disable DAC channel 2 (PA5).
 */
static inline void ll_dac_deinit(void)
{
    CLR_BITS(DAC_CR, DAC_CR_EN2);
}

#endif /* STM32H523xx */

#endif /* LL_DAC_H */
