/**
 * @file   tile_TEMPLATE.h
 * @brief  tile driver for the TEMPLATE tile (CHIPNAME).
 *
 * BRIEF_DESCRIPTION_OF_TILE_AND_IC
 *
 * Datasheet: DATASHEET_URL
 *
 * @note   I2C default address: 0xXX (instance 0, address pins floating)
 *
 * Quick start:
 * @code
 *   tiles_pal_PLATFORM_cfg_t cfg = { .i2c = &I2C_HANDLE, .buses = TILES_BUS_I2C };
 *   tiles_pal_t hal;
 *   tiles_pal_PLATFORM_init(&hal, &cfg);
 *
 *   tile_t sensor;
 *   tile_TEMPLATE_init(&hal, 0, &sensor);
 *   if (tile_is_ready(&sensor)) {
 *       // Read data...
 *   }
 * @endcode
 */

#ifndef INC_TILE_TEMPLATE_H_
#define INC_TILE_TEMPLATE_H_

#include "tiles.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Driver version                                                    */
/* ------------------------------------------------------------------ */

#define TILE_TEMPLATE_VERSION_MAJOR  1
#define TILE_TEMPLATE_VERSION_MINOR  0
#define TILE_TEMPLATE_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* ------------------------------------------------------------------ */
/*  Instance mapping                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief  Instance-to-address mapping for TEMPLATE.
 *
 * | Instance | ID     | Bus  | Hardware config          |
 * |----------|--------|------|--------------------------|
 * | 0        | 0xXX   | I2C  | Address pin floating     |
 * | 1        | 0xXX   | I2C  | Address pin tied to GND  |
 */
#define CHIPNAME_I2C_ADDR_DEFAULT    0x00   /**< Instance 0 — pin floating */
#define CHIPNAME_I2C_ADDR_ALT       0x00   /**< Instance 1 — pin to GND   */

/* ------------------------------------------------------------------ */
/*  Chip identification                                               */
/* ------------------------------------------------------------------ */

#define CHIPNAME_WHOAMI_REG          0x00   /**< WHO_AM_I register address */
#define CHIPNAME_WHOAMI_DEFAULT      0x00   /**< Expected WHO_AM_I value   */

/* ------------------------------------------------------------------ */
/*  Register map                                                      */
/* ------------------------------------------------------------------ */

/* Add chip-specific register defines here.
 * Format: #define CHIPNAME_REG_NAME  0xXX
 */

/* ------------------------------------------------------------------ */
/*  Configuration enums                                               */
/* ------------------------------------------------------------------ */

/* Add tile-specific enums here. Prefix types and values with
 * the tile's family_name to avoid namespace collisions. Example:
 *
 * typedef enum {
 *     FAMILY_NAME_MODE_STANDBY  = 0x00,
 *     FAMILY_NAME_MODE_ACTIVE   = 0x01,
 * } family_name_mode_t;
 */

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief  Probe the I2C bus for a TEMPLATE tile.
 *
 * Sends an address probe (no initialization). Use this to detect
 * whether a tile is physically connected before committing to init.
 *
 * @param  hal       Tile HAL handle
 * @param  instance  Address index (0 = default, see address table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_TEMPLATE_find(tiles_pal_t* hal, uint8_t instance);

/**
 * @brief  Initialize a TEMPLATE tile.
 *
 * Probes the device, verifies the chip ID, and applies the default
 * configuration. The tile handle is populated in place.
 *
 * @param  hal       Tile HAL handle
 * @param  instance  Address index (0 = default, see address table)
 * @param  tile      Pointer to tile handle (populated by this function)
 *
 * @code
 *   tile_t dev;
 *   tile_TEMPLATE_init(&hal, 0, &dev);
 *   if (!tile_is_ready(&dev)) {
 *       // Handle init failure
 *   }
 * @endcode
 */
void tile_TEMPLATE_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile);

/**
 * @brief  Check if new data is available.
 *
 * Reads the device status register to determine if fresh data
 * can be read. Optional — you can skip this and read directly,
 * but may get stale or duplicate samples.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if new data is available, 0 otherwise
 */
uint8_t tile_TEMPLATE_data_ready(tile_t* tile);

/**
 * @brief  Soft-reset the TEMPLATE tile.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_TEMPLATE_reset(tile_t* tile);

/* --- Add tile-specific API functions below --- */

/* Example read function:
 *
 * void tile_TEMPLATE_get_value(tile_t* tile, int16_t* value);
 */

/* Example configuration function:
 *
 * void tile_TEMPLATE_set_mode(tile_t* tile, template_mode_t mode);
 */

#endif /* INC_TILE_TEMPLATE_H_ */
