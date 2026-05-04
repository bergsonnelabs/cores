/**
 * @file   tiles_pal_arduino.h
 * @brief  Arduino HAL for tile drivers.
 *
 * Works on AVR, SAMD, ESP32-Arduino, RP2040, and other
 * Arduino-compatible boards.
 *
 * Usage:
 * @code
 *   #include "tiles.h"
 *   #include "tiles_pal_arduino.h"
 *
 *   tiles_pal_arduino_cfg_t cfg = {
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_pal_t hal;
 *   tiles_pal_arduino_init(&hal, &cfg);
 * @endcode
 */

#ifndef TILES_PAL_ARDUINO_H_
#define TILES_PAL_ARDUINO_H_

#include "tiles.h"
#include <Wire.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Arduino HAL configuration.
 *
 * Set .wire to NULL (or omit) to use the default Wire instance.
 * For SPI, list the Arduino digital pin numbers used as chip-selects.
 */
typedef struct {
    TwoWire*  wire;              /**< I2C instance (NULL = default Wire)      */
    uint8_t   spi_cs_pins[8];   /**< Arduino pin numbers for SPI CS lines    */
    uint8_t   buses;             /**< TILES_BUS_I2C | TILES_BUS_SPI | ...     */
} tiles_pal_arduino_cfg_t;

/**
 * @brief  Initialize a tiles_pal_t from an Arduino configuration.
 *
 * Call Wire.begin() (and SPI.begin() if using SPI) before this.
 * Configure CS pins as OUTPUT before using SPI-based tiles.
 *
 * @param  hal  Pointer to tiles_pal_t to populate
 * @param  cfg  Arduino-specific configuration
 */
void tiles_pal_arduino_init(tiles_pal_t* hal, const tiles_pal_arduino_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* TILES_PAL_ARDUINO_H_ */
