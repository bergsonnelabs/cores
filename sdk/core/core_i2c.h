/**
 * core_i2c.h — I2C bus communication
 *
 * Provides a simplified I2C API with:
 *   - Speed constants: I2C_100K, I2C_400K, I2C_1M
 *   - Auto-resolved timing from the compile-time kernel clock
 *   - Friendly status aliases: I2C_OK, I2C_NACK, I2C_TIMEOUT
 *
 * Typical usage (coregen pre-creates core_i2c1, core_i2c3 handles):
 *
 *   uint8_t data[] = { 0x42 };
 *   core_i2c_write(&core_i2c1, 0x68, data, 1);
 *
 *   uint8_t val;
 *   core_i2c_read_byte(&core_i2c1, 0x68, 0x75, &val);  // WHO_AM_I
 *
 * For manual init (no coregen):
 *
 *   core_i2c_t bus;
 *   core_i2c_setup(&bus, I2C1, I2C_400K);
 */

#ifndef CORE_I2C_H
#define CORE_I2C_H

#include "hal_i2c.h"
#include "core_config.h"

/** Core-level I2C handle. Alias for hal_i2c_t — use this in application code. */
typedef hal_i2c_t core_i2c_t;

/* ---- Speed constants ---- */

#define I2C_100K    100000UL
#define I2C_400K    400000UL
#define I2C_1M     1000000UL

/* ---- Status aliases ---- */

#define I2C_OK       HAL_OK
#define I2C_NACK     HAL_NACK
#define I2C_TIMEOUT  HAL_TIMEOUT
#define I2C_ERROR    HAL_ERROR

/* ---- Auto-resolving init ---- */

/**
 * Resolve the TIMINGR value for a given speed using the compile-time
 * I2C kernel clock (I2C_KERNEL_CLK_MHZ from core_config.h).
 * Returns 0 if no pre-computed value exists.
 */
static inline uint32_t _core_i2c_timing(uint32_t speed_hz)
{
    switch (speed_hz) {
        case I2C_100K: return ll_i2c_timing_100k(I2C_KERNEL_CLK_MHZ);
        case I2C_400K: return ll_i2c_timing_400k(I2C_KERNEL_CLK_MHZ);
        case I2C_1M:   return ll_i2c_timing_1m(I2C_KERNEL_CLK_MHZ);
        default:       return 0;
    }
}

/**
 * Initialize an I2C bus with automatic timing resolution.
 *
 *   core_i2c_t bus;
 *   core_i2c_setup(&bus, I2C1, I2C_400K);
 *
 * Resolves the TIMINGR value from the compile-time kernel clock.
 * Enables FMP mode automatically when speed is I2C_1M.
 */
static inline hal_status_t core_i2c_setup(core_i2c_t *h,
                                           I2C_TypeDef *instance,
                                           uint32_t speed_hz)
{
    uint32_t timing = _core_i2c_timing(speed_hz);
    if (!timing) return HAL_ERROR;
    hal_i2c_config_t cfg = {
        .timing = timing,
        .timeout_ms = 100,
        .fmp = (speed_hz >= I2C_1M) ? 1 : 0,
    };
    return hal_i2c_init(h, instance, &cfg);
}

/**
 * Initialize I2C with explicit config (same as hal_i2c_init).
 * Use core_i2c_setup() for the simpler speed-based API.
 */
static inline hal_status_t core_i2c_init(core_i2c_t *h,
                                          I2C_TypeDef *instance,
                                          const hal_i2c_config_t *cfg)
{
    return hal_i2c_init(h, instance, cfg);
}

/* ---- Master operations ---- */

/** Write data to a 7-bit address device. */
static inline hal_status_t core_i2c_write(core_i2c_t *h, uint8_t addr,
                                           const uint8_t *data, uint32_t len)
{
    return hal_i2c_write(h, addr, data, len);
}

/** Read data from a 7-bit address device. */
static inline hal_status_t core_i2c_read(core_i2c_t *h, uint8_t addr,
                                          uint8_t *buf, uint32_t len)
{
    return hal_i2c_read(h, addr, buf, len);
}

/* ---- Register helpers ---- */

/** Write to a device register (8 or 16-bit register address). */
static inline hal_status_t core_i2c_write_reg(core_i2c_t *h, uint8_t addr,
                                               uint16_t reg,
                                               const uint8_t *data,
                                               uint32_t len)
{
    return hal_i2c_write_reg(h, addr, reg, data, len);
}

/** Read from a device register (repeated START). */
static inline hal_status_t core_i2c_read_reg(core_i2c_t *h, uint8_t addr,
                                              uint16_t reg, uint8_t *buf,
                                              uint32_t len)
{
    return hal_i2c_read_reg(h, addr, reg, buf, len);
}

/** Write a single byte to a register. */
static inline hal_status_t core_i2c_write_byte(core_i2c_t *h, uint8_t addr,
                                                uint16_t reg, uint8_t value)
{
    return hal_i2c_write_byte(h, addr, reg, value);
}

/** Read a single byte from a register. */
static inline hal_status_t core_i2c_read_byte(core_i2c_t *h, uint8_t addr,
                                               uint16_t reg, uint8_t *value)
{
    return hal_i2c_read_byte(h, addr, reg, value);
}

/* ---- Bus utilities ---- */

/** Check if a device responds at addr. Returns I2C_OK or I2C_NACK. */
static inline hal_status_t core_i2c_probe(core_i2c_t *h, uint8_t addr)
{
    return hal_i2c_probe(h, addr);
}

/** Scan the I2C bus (0x08–0x77). Fills found[] with responding addresses. */
static inline void core_i2c_scan(core_i2c_t *h, uint8_t *found,
                                  uint8_t *count, uint8_t max_count)
{
    hal_i2c_scan(h, found, count, max_count);
}

#endif /* CORE_I2C_H */
