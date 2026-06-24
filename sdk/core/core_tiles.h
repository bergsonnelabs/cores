/**
 * core_tiles.h — Bridge between Cores SDK and tile drivers.
 *
 * Provides core_tiles_pal() which accepts either an I2C or SPI bus
 * handle and returns a tiles_pal_t* wired to the Cores SDK:
 *
 *   tiles_pal_t *hal = core_tiles_pal(&core_i2c1);   // I2C bus
 *   tiles_pal_t *hal = core_tiles_pal(&core_spi1);   // SPI bus
 *
 * The correct bus type is resolved at compile time via C11 _Generic.
 * Passing the wrong type is a compile error.
 *
 * @studio coverage
 *   id:    tiles
 *   name:  Tiles — SDK ↔ tile-driver bridge
 *   blurb: Tier 1 only. Header-only adapter that wraps a core_i2c_t* /
 *          core_spi_t* into the tiles_pal_t* shape tile drivers
 *          consume. C11 _Generic resolves the bus type at compile
 *          time; the resulting hal struct is cached per bus pointer
 *          (max 4 per bus type). Used by every tile val / hardware
 *          test in the SDK; not user-facing in the DSL.
 */

#ifndef CORE_TILES_H
#define CORE_TILES_H

#include "core_i2c.h"
/* SPI is unavailable on Core.ST.L0 (STM32L011) — core_spi.h #errors there.
 * Guard all SPI bridging below so I2C-only tile projects still build on L0. */
#if !defined(STM32L011xx)
#define _CORE_TILES_HAS_SPI 1
#include "core_spi.h"
#endif
#include "core_pad.h"
#include "tiles_pal.h"
#include "ll_systick.h"

/* ---- Internal: I2C adapters ---- */

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

/* ---- Internal: SPI adapters ---- */
#ifdef _CORE_TILES_HAS_SPI

static inline int _ct_spi_read(void *h, uint8_t cs, uint8_t reg,
                               uint8_t *data, uint16_t len)
{
    core_spi_t *spi = (core_spi_t *)h;
    (void)cs;
    core_spi_select(spi);
    core_spi_transfer(spi, reg | 0x80);
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
    core_spi_transfer(spi, reg & 0x7F);
    core_spi_write(spi, data, len);
    core_spi_deselect(spi);
    return 0;
}

#endif /* _CORE_TILES_HAS_SPI */

/* ---- Internal: shared adapters ---- */

static inline int _ct_gpio_irq_enable(void *h, uint8_t pin, uint8_t edge,
                                      void (*cb)(void *), void *ctx)
{
    (void)h;
    uint32_t e = (edge == TILES_GPIO_EDGE_FALLING) ? EDGE_FALLING
               : (edge == TILES_GPIO_EDGE_RISING)  ? EDGE_RISING
               : EDGE_BOTH;
    return core_pad_on_change(pin, e, cb, ctx);
}

/* ---- Internal: typed constructors ---- */

static inline tiles_pal_t *_core_tiles_pal_i2c(core_i2c_t *bus)
{
    enum { CT_MAX = 4 };
    static tiles_pal_t hals[CT_MAX];
    static void *keys[CT_MAX];
    static uint8_t count = 0;

    for (uint8_t i = 0; i < count; i++)
        if (keys[i] == bus)
            return &hals[i];

    if (count >= CT_MAX)
        return &hals[0];

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

#ifdef _CORE_TILES_HAS_SPI
static inline tiles_pal_t *_core_tiles_pal_spi(core_spi_t *bus)
{
    enum { CT_MAX = 4 };
    static tiles_pal_t hals[CT_MAX];
    static void *keys[CT_MAX];
    static uint8_t count = 0;

    for (uint8_t i = 0; i < count; i++)
        if (keys[i] == bus)
            return &hals[i];

    if (count >= CT_MAX)
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
#endif /* _CORE_TILES_HAS_SPI */

/* ---- Public API ---- */

/**
 * Get a tiles_pal_t* for any Cores SDK bus handle.
 *
 * Accepts either a core_i2c_t* or core_spi_t* — the correct bus type
 * is resolved at compile time. Each unique bus pointer gets its own
 * cached slot (up to 4 per bus type).
 *
 * @code
 *   tiles_pal_t *hal = core_tiles_pal(&core_i2c1);   // I2C
 *   tiles_pal_t *hal = core_tiles_pal(&core_spi1);   // SPI
 * @endcode
 */
#ifdef _CORE_TILES_HAS_SPI
#define core_tiles_pal(bus) _Generic((bus), \
    core_i2c_t*: _core_tiles_pal_i2c,      \
    core_spi_t*: _core_tiles_pal_spi       \
)(bus)
#else
/* L0 (no SPI): only the I2C bridge exists. */
#define core_tiles_pal(bus) _Generic((bus), \
    core_i2c_t*: _core_tiles_pal_i2c       \
)(bus)
#endif

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @studio unsupported tier=1 value=M title="Hard cap of 4 cached PALs per bus type"
//   The internal slot table is sized to 4. Designs with >4 simultaneous
//   I2C buses (or >4 SPI buses) will silently route extras to slot 0,
//   which corrupts the cached function pointers if the bus types
//   differ. No current Core has that many buses but the cap is
//   undocumented for future-proofing.
//
// @studio unsupported tier=1 value=L title="No bus / address discovery"
//   Tile drivers receive a bare tiles_pal_t* and a tile JSON; there's
//   no wrapper here that walks coregen's tile_handles.h to enumerate
//   "what tiles are on what bus." Each test builds the wiring by hand.

#endif /* CORE_TILES_H */
