/**
 * hal_i2c.c — I2C HAL driver implementation
 */

#include "hal_i2c.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
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
    h->timing = cfg->timing;
    h->fmp = cfg->fmp;

    _i2c_clk_enable(instance);
    if (cfg->fmp) {
        ll_i2c_init_fmp(instance, cfg->timing);
    } else {
        ll_i2c_init(instance, cfg->timing);
    }
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
 * Auto-recovery wrapper — retry once after bus recovery
 * ============================================================ */

static hal_status_t _with_recovery(hal_i2c_t *h, int ll_result)
{
    hal_status_t s = _convert_status(ll_result);
    if (s == HAL_TIMEOUT || s == HAL_ERROR) {
        hal_i2c_recover(h);
    }
    return s;
}

/* ============================================================
 * Raw master operations
 * ============================================================ */

hal_status_t hal_i2c_write(hal_i2c_t *h, uint8_t addr,
                           const uint8_t *data, uint32_t len)
{
    return _with_recovery(h, ll_i2c_write(h->instance, addr, data, len));
}

hal_status_t hal_i2c_read(hal_i2c_t *h, uint8_t addr,
                          uint8_t *buf, uint32_t len)
{
    return _with_recovery(h, ll_i2c_read(h->instance, addr, buf, len));
}

/* ============================================================
 * Register-level helpers
 * ============================================================ */

hal_status_t hal_i2c_write_reg(hal_i2c_t *h, uint8_t addr,
                               uint16_t reg, const uint8_t *data, uint32_t len)
{
    return _with_recovery(h, ll_i2c_write_reg(h->instance, addr, reg, data, len));
}

hal_status_t hal_i2c_read_reg(hal_i2c_t *h, uint8_t addr,
                              uint16_t reg, uint8_t *buf, uint32_t len)
{
    return _with_recovery(h, ll_i2c_read_reg(h->instance, addr, reg, buf, len));
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
 * Bus recovery — clock out a stuck slave
 * ============================================================ */

/* Map I2C instance → SCL/SDA GPIO port+pin.
 * These are fixed per tile family (set by coregen core_pads_init). */
typedef struct {
    GPIO_TypeDef *scl_port; uint8_t scl_pin; uint8_t scl_af;
    GPIO_TypeDef *sda_port; uint8_t sda_pin; uint8_t sda_af;
} _i2c_pins_t;

static _i2c_pins_t _i2c_pins(I2C_TypeDef *instance)
{
    _i2c_pins_t p = {0};
#if defined(STM32L422xx)
    if (instance == I2C1) {
        p = (const _i2c_pins_t){ GPIOB, 6, 4, GPIOB, 7, 4 };
    } else if (instance == I2C3) {
        p = (const _i2c_pins_t){ GPIOA, 7, 4, GPIOB, 4, 4 };
    }
#elif defined(STM32WBA55xx)
    if (instance == I2C1) {
        p = (const _i2c_pins_t){ GPIOB, 2, 4, GPIOB, 1, 4 };
    } else if (instance == I2C3) {
        p = (const _i2c_pins_t){ GPIOA, 6, 4, GPIOA, 7, 4 };
    }
#elif defined(STM32H523xx)
    if (instance == I2C1) {
        p = (const _i2c_pins_t){ GPIOB, 6, 4, GPIOB, 7, 4 };
    } else if (instance == I2C2) {
        p = (const _i2c_pins_t){ GPIOB, 10, 4, GPIOB, 11, 4 };
    }
#elif defined(STM32L011xx)
    if (instance == I2C1) {
        p = (const _i2c_pins_t){ GPIOA, 9, 1, GPIOA, 10, 1 };
    }
#endif
    return p;
}

static void _i2c_delay(void)
{
    for (volatile int i = 0; i < 50; i++) ;
}

hal_status_t hal_i2c_recover(hal_i2c_t *h)
{
    _i2c_pins_t pins = _i2c_pins(h->instance);
    if (!pins.scl_port) return HAL_ERROR;  /* unknown instance */

    /* 1. Disable I2C peripheral */
    h->instance->CR1 &= ~LL_I2C_CR1_PE;

    /* 2. Switch SCL to push-pull output (high), SDA to input */
    ll_gpio_config_output(pins.scl_port, pins.scl_pin);
    pins.scl_port->BSRR = (1UL << pins.scl_pin);  /* SCL high */

    ll_gpio_config_input(pins.sda_port, pins.sda_pin, LL_GPIO_PULL_UP);

    _i2c_delay();

    /* 3. Clock SCL up to 16 times until SDA goes high */
    uint8_t recovered = 0;
    for (int i = 0; i < 16; i++) {
        if (pins.sda_port->IDR & (1UL << pins.sda_pin)) {
            recovered = 1;
            break;
        }
        pins.scl_port->BSRR = (1UL << (pins.scl_pin + 16));  /* SCL low */
        _i2c_delay();
        pins.scl_port->BSRR = (1UL << pins.scl_pin);          /* SCL high */
        _i2c_delay();
    }

    /* 4. Generate STOP: SDA low, then SDA high while SCL is high */
    /* Switch SDA to output temporarily */
    ll_gpio_config_output(pins.sda_port, pins.sda_pin);
    ll_gpio_set_output_type(pins.sda_port, pins.sda_pin, LL_GPIO_OTYPE_OD);

    pins.sda_port->BSRR = (1UL << (pins.sda_pin + 16));  /* SDA low */
    _i2c_delay();
    pins.sda_port->BSRR = (1UL << pins.sda_pin);          /* SDA high */
    _i2c_delay();

    /* 5. Restore pins to AF open-drain and re-init I2C */
    ll_gpio_config_af(pins.scl_port, pins.scl_pin, pins.scl_af,
                      LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_UP);
    ll_gpio_config_af(pins.sda_port, pins.sda_pin, pins.sda_af,
                      LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_UP);

    if (h->fmp)
        ll_i2c_init_fmp(h->instance, h->timing);
    else
        ll_i2c_init(h->instance, h->timing);

    return recovered ? HAL_OK : HAL_ERROR;
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
