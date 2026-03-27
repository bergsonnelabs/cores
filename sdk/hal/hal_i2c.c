/**
 * hal_i2c.c — I2C HAL driver implementation
 */

#include "hal_i2c.h"
#include "ll_rcc.h"
#include <string.h>

/* ---- Peripheral clock enable ---- */

static void _i2c_clk_enable(I2C_TypeDef *instance)
{
    if (instance == I2C1) ll_rcc_apb1_clk_enable(LL_APB1_I2C1);
#if defined(STM32L422xx)
    if (instance == I2C3) ll_rcc_apb1_clk_enable(LL_APB1_I2C3);
#elif defined(STM32WBA55xx)
    if (instance == I2C3) ll_rcc_apb7_clk_enable(LL_APB7_I2C3);
#elif defined(STM32H523xx)
    if (instance == I2C2) ll_rcc_apb1_clk_enable(LL_APB1_I2C2);
    if (instance == I2C3) ll_rcc_apb3_clk_enable(LL_APB3_I2C3);
#endif
    (void)REG32(RCC_BASE);
}

/* ============================================================
 * Init / Deinit
 * ============================================================ */

hal_status_t hal_i2c_init(hal_i2c_t *h, I2C_TypeDef *instance,
                          const hal_i2c_config_t *cfg)
{
    memset(h, 0, sizeof(*h));
    h->instance = instance;
    h->timeout_ms = cfg->timeout_ms ? cfg->timeout_ms : 100;

    _i2c_clk_enable(instance);
    ll_i2c_init(instance, cfg->timing);
    return HAL_OK;
}

void hal_i2c_deinit(hal_i2c_t *h)
{
    if (h && h->instance) {
        ll_i2c_disable(h->instance);
        h->instance = NULL;
    }
}

/* ============================================================
 * Status conversion
 * ============================================================ */

static hal_status_t _convert_status(int ll_result)
{
    switch (ll_result) {
    case LL_I2C_OK:      return HAL_OK;
    case LL_I2C_NACK:    return HAL_NACK;
    case LL_I2C_TIMEOUT: return HAL_TIMEOUT;
    default:             return HAL_ERROR;
    }
}

/* ============================================================
 * Raw master operations
 * ============================================================ */

hal_status_t hal_i2c_write(hal_i2c_t *h, uint8_t addr,
                           const uint8_t *data, uint32_t len)
{
    return _convert_status(ll_i2c_write(h->instance, addr, data, len));
}

hal_status_t hal_i2c_read(hal_i2c_t *h, uint8_t addr,
                          uint8_t *buf, uint32_t len)
{
    return _convert_status(ll_i2c_read(h->instance, addr, buf, len));
}

/* ============================================================
 * Register-level helpers
 * ============================================================ */

hal_status_t hal_i2c_write_reg(hal_i2c_t *h, uint8_t addr,
                               uint16_t reg, const uint8_t *data, uint32_t len)
{
    return _convert_status(ll_i2c_write_reg(h->instance, addr, reg, data, len));
}

hal_status_t hal_i2c_read_reg(hal_i2c_t *h, uint8_t addr,
                              uint16_t reg, uint8_t *buf, uint32_t len)
{
    return _convert_status(ll_i2c_read_reg(h->instance, addr, reg, buf, len));
}

hal_status_t hal_i2c_write_byte(hal_i2c_t *h, uint8_t addr,
                                uint16_t reg, uint8_t value)
{
    return hal_i2c_write_reg(h, addr, reg, &value, 1);
}

hal_status_t hal_i2c_read_byte(hal_i2c_t *h, uint8_t addr,
                               uint16_t reg, uint8_t *value)
{
    return hal_i2c_read_reg(h, addr, reg, value, 1);
}

/* ============================================================
 * Bus utilities
 * ============================================================ */

hal_status_t hal_i2c_probe(hal_i2c_t *h, uint8_t addr)
{
    return _convert_status(ll_i2c_probe(h->instance, addr));
}

void hal_i2c_scan(hal_i2c_t *h, uint8_t *found, uint8_t *count,
                  uint8_t max_count)
{
    *count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (hal_i2c_probe(h, addr) == HAL_OK) {
            if (*count < max_count) {
                found[*count] = addr;
            }
            (*count)++;
        }
    }
}
