/**
 * hal_i2c.h — I2C HAL driver
 *
 * Master-mode I2C with polling, register-level helpers,
 * and bus scanning. Wraps ll_i2c with ergonomic API.
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "hal_common.h"
#include "ll_i2c.h"

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    I2C_TypeDef *instance;
    uint32_t timeout_ms;    /* Default: 100 */
} hal_i2c_t;

typedef struct {
    uint32_t timing;        /* TIMINGR value (use LL_I2C_TIMING_* defines) */
    uint32_t timeout_ms;    /* 0 = use default (100ms) */
} hal_i2c_config_t;

/* ============================================================
 * API declarations (implemented in hal_i2c.c)
 * ============================================================ */

/**
 * Initialize I2C in master mode.
 * Prerequisites:
 *   - I2C peripheral clock enabled
 *   - SDA/SCL pins configured as AF open-drain with pull-up
 */
hal_status_t hal_i2c_init(hal_i2c_t *h, I2C_TypeDef *instance,
                          const hal_i2c_config_t *cfg);

void hal_i2c_deinit(hal_i2c_t *h);

/* ---- Raw master operations ---- */

/** Write data to a 7-bit address device */
hal_status_t hal_i2c_write(hal_i2c_t *h, uint8_t addr,
                           const uint8_t *data, uint32_t len);

/** Read data from a 7-bit address device */
hal_status_t hal_i2c_read(hal_i2c_t *h, uint8_t addr,
                          uint8_t *buf, uint32_t len);

/* ---- Register-level helpers (most common I2C pattern) ---- */

/** Write to a register on an I2C device */
hal_status_t hal_i2c_write_reg(hal_i2c_t *h, uint8_t addr,
                               uint8_t reg, const uint8_t *data, uint32_t len);

/** Read from a register on an I2C device */
hal_status_t hal_i2c_read_reg(hal_i2c_t *h, uint8_t addr,
                              uint8_t reg, uint8_t *buf, uint32_t len);

/* ---- Single-byte convenience ---- */

/** Write a single byte to a register */
hal_status_t hal_i2c_write_byte(hal_i2c_t *h, uint8_t addr,
                                uint8_t reg, uint8_t value);

/** Read a single byte from a register */
hal_status_t hal_i2c_read_byte(hal_i2c_t *h, uint8_t addr,
                               uint8_t reg, uint8_t *value);

/* ---- Bus utilities ---- */

/** Check if a device responds at addr. Returns HAL_OK or HAL_NACK. */
hal_status_t hal_i2c_probe(hal_i2c_t *h, uint8_t addr);

/**
 * Scan the I2C bus (0x08–0x77). Fills found[] with responding
 * addresses. Returns actual count via *count.
 */
void hal_i2c_scan(hal_i2c_t *h, uint8_t *found, uint8_t *count,
                  uint8_t max_count);

#endif /* HAL_I2C_H */
