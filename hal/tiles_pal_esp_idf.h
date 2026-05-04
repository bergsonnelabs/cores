/**
 * @file   tiles_pal_esp_idf.h
 * @brief  ESP-IDF HAL for tile drivers.
 *
 * Supports ESP-IDF v5.x i2c_master and spi_master APIs.
 *
 * Usage:
 * @code
 *   #include "tiles.h"
 *   #include "tiles_pal_esp_idf.h"
 *
 *   tiles_pal_esp_idf_cfg_t cfg = {
 *       .i2c_bus = bus_handle,
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_pal_t hal;
 *   tiles_pal_esp_idf_init(&hal, &cfg);
 * @endcode
 */

#ifndef TILES_PAL_ESP_IDF_H_
#define TILES_PAL_ESP_IDF_H_

#include "tiles.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

/**
 * @brief  ESP-IDF HAL configuration.
 */
typedef struct {
    i2c_master_bus_handle_t  i2c_bus;       /**< I2C master bus handle (from i2c_new_master_bus) */
    spi_host_device_t        spi_host;      /**< SPI host (SPI2_HOST, SPI3_HOST)                */
    uint32_t                 spi_clock_hz;  /**< SPI clock speed in Hz                          */
    uint8_t                  spi_cs_pins[8]; /**< GPIO numbers for SPI CS lines                 */
    uint8_t                  buses;          /**< TILES_BUS_I2C | TILES_BUS_SPI | ...            */
} tiles_pal_esp_idf_cfg_t;

/**
 * @brief  Initialize a tiles_pal_t from an ESP-IDF configuration.
 *
 * Create and configure I2C bus (i2c_new_master_bus) and/or SPI bus
 * (spi_bus_initialize) before calling this.
 *
 * @param  hal  Pointer to tiles_pal_t to populate
 * @param  cfg  ESP-IDF-specific configuration
 */
void tiles_pal_esp_idf_init(tiles_pal_t* hal, const tiles_pal_esp_idf_cfg_t* cfg);

#endif /* TILES_PAL_ESP_IDF_H_ */
