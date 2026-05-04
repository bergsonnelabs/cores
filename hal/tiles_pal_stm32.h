/**
 * @file   tiles_pal_stm32.h
 * @brief  STM32 HAL for tile drivers.
 *
 * Works with all STM32 families that use the STM32 HAL driver library
 * (F0/F1/F3/F4/G0/G4/L0/L4/H7/U5/WB/WL).
 *
 * Usage:
 * @code
 *   #include "tiles.h"
 *   #include "tiles_pal_stm32.h"
 *
 *   tiles_pal_stm32_cfg_t cfg = {
 *       .i2c = &hi2c1,
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_pal_t hal;
 *   tiles_pal_stm32_init(&hal, &cfg);
 * @endcode
 */

#ifndef TILES_PAL_STM32_H_
#define TILES_PAL_STM32_H_

#include "tiles.h"

/* Pull in the right STM32 HAL header — set by your project defines */
#ifdef STM32WBxx
#include "stm32wbxx_hal.h"
#elif defined(STM32L4xx)
#include "stm32l4xx_hal.h"
#elif defined(STM32G4xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#elif defined(STM32H7xx)
#include "stm32h7xx_hal.h"
#elif defined(STM32U5xx)
#include "stm32u5xx_hal.h"
#elif defined(STM32G0xx)
#include "stm32g0xx_hal.h"
#elif defined(STM32F0xx)
#include "stm32f0xx_hal.h"
#else
/* Fallback — your project must provide the appropriate HAL include */
#endif

/**
 * @brief  STM32 HAL configuration.
 *
 * Set the bus handles you need and list CS port/pin pairs for SPI tiles.
 * Bus handles (I2C, SPI) must be initialized by your project before
 * calling tiles_pal_stm32_init.
 */
typedef struct {
    I2C_HandleTypeDef*  i2c;              /**< I2C peripheral (e.g. &hi2c1)     */
    SPI_HandleTypeDef*  spi;              /**< SPI peripheral (e.g. &hspi1)     */
    GPIO_TypeDef*       spi_cs_ports[8];  /**< GPIO port for each CS line       */
    uint16_t            spi_cs_pins[8];   /**< GPIO pin for each CS line        */
    uint8_t             buses;            /**< TILES_BUS_I2C | TILES_BUS_SPI    */
} tiles_pal_stm32_cfg_t;

/**
 * @brief  Initialize a tiles_pal_t from an STM32 configuration.
 *
 * @param  hal  Pointer to tiles_pal_t to populate
 * @param  cfg  STM32-specific configuration
 */
void tiles_pal_stm32_init(tiles_pal_t* hal, const tiles_pal_stm32_cfg_t* cfg);

#endif /* TILES_PAL_STM32_H_ */
