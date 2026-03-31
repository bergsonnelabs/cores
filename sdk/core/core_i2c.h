/**
 * core_i2c.h -- I2C bus communication
 *
 * Wraps hal_i2c with friendlier names. Instance and timing
 * resolution still requires explicit init (auto-resolve from
 * project.json is planned).
 */

#ifndef CORE_I2C_H
#define CORE_I2C_H

#include "hal_i2c.h"

/* ---- Init ---- */

/** Initialize I2C in master mode. Same signature as hal_i2c_init. */
static inline hal_status_t core_i2c_init(hal_i2c_t *h,
                                          I2C_TypeDef *instance,
                                          const hal_i2c_config_t *cfg)
{
    return hal_i2c_init(h, instance, cfg);
}

/* ---- Raw master operations ---- */

/** Write data to a 7-bit address device. */
static inline hal_status_t core_i2c_write(hal_i2c_t *h, uint8_t addr,
                                           const uint8_t *data, uint32_t len)
{
    return hal_i2c_write(h, addr, data, len);
}

/** Read data from a 7-bit address device. */
static inline hal_status_t core_i2c_read(hal_i2c_t *h, uint8_t addr,
                                          uint8_t *buf, uint32_t len)
{
    return hal_i2c_read(h, addr, buf, len);
}

/* ---- Register-level helpers ---- */

/** Write to a register on an I2C device. */
static inline hal_status_t core_i2c_write_reg(hal_i2c_t *h, uint8_t addr,
                                               uint16_t reg,
                                               const uint8_t *data,
                                               uint32_t len)
{
    return hal_i2c_write_reg(h, addr, reg, data, len);
}

/** Read from a register on an I2C device. */
static inline hal_status_t core_i2c_read_reg(hal_i2c_t *h, uint8_t addr,
                                              uint16_t reg, uint8_t *buf,
                                              uint32_t len)
{
    return hal_i2c_read_reg(h, addr, reg, buf, len);
}

/** Write a single byte to a register. */
static inline hal_status_t core_i2c_write_byte(hal_i2c_t *h, uint8_t addr,
                                                uint16_t reg, uint8_t value)
{
    return hal_i2c_write_byte(h, addr, reg, value);
}

/** Read a single byte from a register. */
static inline hal_status_t core_i2c_read_byte(hal_i2c_t *h, uint8_t addr,
                                               uint16_t reg, uint8_t *value)
{
    return hal_i2c_read_byte(h, addr, reg, value);
}

/* ---- Bus utilities ---- */

/** Check if a device responds at addr. Returns HAL_OK or HAL_NACK. */
static inline hal_status_t core_i2c_probe(hal_i2c_t *h, uint8_t addr)
{
    return hal_i2c_probe(h, addr);
}

/** Scan the I2C bus (0x08-0x77). Fills found[] with responding addresses. */
static inline void core_i2c_scan(hal_i2c_t *h, uint8_t *found,
                                  uint8_t *count, uint8_t max_count)
{
    hal_i2c_scan(h, found, count, max_count);
}

#endif /* CORE_I2C_H */
