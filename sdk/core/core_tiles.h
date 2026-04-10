/**
 * core_tiles.h — Bridge between Cores SDK and Kiln tile drivers.
 *
 * Automatically wires a core_i2c_t bus handle into a tiles_hal_t
 * so tile drivers can be used with zero boilerplate:
 *
 *   tile_t touch;
 *   tile_sense_t_c_init(&core_i2c3, 0, &touch, &cfg);
 *
 * The tile driver receives a tiles_hal_t* that routes all I2C,
 * delay, and GPIO interrupt calls through the Cores SDK.
 *
 * How it works: tile driver init functions accept tiles_hal_t*,
 * which is a different type than core_i2c_t*. The CORE_TILES_HAL()
 * macro creates a static tiles_hal_t from a core_i2c_t*, and the
 * _core_tiles_auto() inline resolves &core_i2cN to a tiles_hal_t*.
 *
 * Every tile_*_init() that includes this header gets a transparent
 * wrapper macro so the user just passes &core_i2cN directly.
 */

#ifndef CORE_TILES_H
#define CORE_TILES_H

#include "core_i2c.h"
#include "core_spi.h"
#include "core_pad.h"
#include "tiles_hal.h"
#include "ll_systick.h"

/* ---- Internal: Cores SDK -> tiles_hal_t function adapters ---- */

static inline int _ct_i2c_read(void *h, uint8_t addr, uint16_t reg,
                               uint8_t *data, uint16_t len)
{
    return core_i2c_read_reg((core_i2c_t *)h, addr, reg, data, len);
}

static inline int _ct_i2c_write(void *h, uint8_t addr, uint16_t reg,
                                const uint8_t *data, uint16_t len)
{
    return core_i2c_write_reg((core_i2c_t *)h, addr, reg, data, len);
}

static inline int _ct_i2c_ready(void *h, uint8_t addr)
{
    return core_i2c_probe((core_i2c_t *)h, addr);
}

static inline int _ct_i2c_write_raw(void *h, uint8_t addr,
                                    const uint8_t *data, uint16_t len)
{
    return core_i2c_write((core_i2c_t *)h, addr, data, len);
}

static inline int _ct_i2c_read_raw(void *h, uint8_t addr,
                                   uint8_t *data, uint16_t len)
{
    return core_i2c_read((core_i2c_t *)h, addr, data, len);
}

static inline int _ct_gpio_irq_enable(void *h, uint8_t pin, uint8_t edge,
                                      void (*cb)(void *), void *ctx)
{
    (void)h;
    uint32_t e = (edge == TILES_GPIO_EDGE_FALLING) ? EDGE_FALLING
               : (edge == TILES_GPIO_EDGE_RISING)  ? EDGE_RISING
               : EDGE_BOTH;
    return core_pad_on_change(pin, e, cb, ctx);
}

/* ---- Internal: Cores SDK -> tiles_hal_t SPI adapters ---- */

static inline int _ct_spi_read(void *h, uint8_t cs, uint8_t reg,
                               uint8_t *data, uint16_t len)
{
    core_spi_t *spi = (core_spi_t *)h;
    (void)cs;  /* CS already configured on the hal_spi_t handle */
    core_spi_select(spi);
    core_spi_transfer(spi, reg | 0x80);  /* Read bit (MSB=1) */
    core_spi_read(spi, data, len);
    core_spi_deselect(spi);
    return 0;
}

static inline int _ct_spi_write(void *h, uint8_t cs, uint8_t reg,
                                const uint8_t *data, uint16_t len)
{
    core_spi_t *spi = (core_spi_t *)h;
    (void)cs;
    core_spi_select(spi);
    core_spi_transfer(spi, reg & 0x7F);  /* Write bit (MSB=0) */
    core_spi_write(spi, data, len);
    core_spi_deselect(spi);
    return 0;
}

/* ---- Public: create a tiles_hal_t* from a bus handle ---- */

/**
 * Create a tiles_hal_t that routes through the Cores SDK.
 * Returns a pointer to a static tiles_hal_t — one per bus.
 *
 * Usage:
 *   tiles_hal_t *hal = core_tiles_hal(&core_i2c3);
 *   tile_sense_t_c_init(hal, 0, &touch, &cfg);
 *
 * Or use the implicit wrappers below for even cleaner code.
 */
/**
 * Get a tiles_hal_t* for a given core_i2c_t bus.
 * Each unique bus pointer gets its own slot (up to 4 buses).
 * Multiple tiles on the same bus share the same HAL — correct and efficient.
 *
 *   tile_disp_rgbw_init(core_tiles_hal(&core_i2c1), 0, &led);
 *   tile_sense_t_c_init(core_tiles_hal(&core_i2c3), 0, &touch, &cfg);
 */
static inline tiles_hal_t *core_tiles_hal(core_i2c_t *bus)
{
    enum { CT_MAX_BUSES = 4 };
    static tiles_hal_t hals[CT_MAX_BUSES];
    static void *keys[CT_MAX_BUSES];
    static uint8_t count = 0;

    /* Return existing HAL if this bus was already registered */
    for (uint8_t i = 0; i < count; i++)
        if (keys[i] == bus)
            return &hals[i];

    /* Allocate a new slot */
    if (count >= CT_MAX_BUSES)
        return &hals[0];  /* Fallback — shouldn't happen */

    uint8_t i = count++;
    keys[i] = bus;
    hals[i].i2c_read        = _ct_i2c_read;
    hals[i].i2c_write       = _ct_i2c_write;
    hals[i].i2c_is_ready    = _ct_i2c_ready;
    hals[i].i2c_write_raw   = _ct_i2c_write_raw;
    hals[i].i2c_read_raw    = _ct_i2c_read_raw;
    hals[i].gpio_irq_enable = _ct_gpio_irq_enable;
    hals[i].delay_ms        = ll_delay_ms;
    hals[i].buses           = TILES_BUS_I2C;
    hals[i].handle          = bus;
    return &hals[i];
}

/**
 * Get a tiles_hal_t* for a given core_spi_t bus.
 * Works the same as core_tiles_hal() but wires SPI function pointers.
 * CS pin must already be configured on the hal_spi_t handle.
 *
 *   tile_drive_a_2_init(core_tiles_hal_spi(&core_spi1), 0, &dac, NULL);
 */
static inline tiles_hal_t *core_tiles_hal_spi(core_spi_t *bus)
{
    enum { CT_MAX_SPI = 4 };
    static tiles_hal_t hals[CT_MAX_SPI];
    static void *keys[CT_MAX_SPI];
    static uint8_t count = 0;

    for (uint8_t i = 0; i < count; i++)
        if (keys[i] == bus)
            return &hals[i];

    if (count >= CT_MAX_SPI)
        return &hals[0];

    uint8_t i = count++;
    keys[i] = bus;
    hals[i].spi_read        = _ct_spi_read;
    hals[i].spi_write       = _ct_spi_write;
    hals[i].gpio_irq_enable = _ct_gpio_irq_enable;
    hals[i].delay_ms        = ll_delay_ms;
    hals[i].buses           = TILES_BUS_SPI;
    hals[i].handle          = bus;
    return &hals[i];
}

#endif /* CORE_TILES_H */
