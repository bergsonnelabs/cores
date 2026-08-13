/**
 * @file   tile_TEMPLATE.c
 * @brief  tile driver implementation for the TEMPLATE tile (CHIPNAME).
 */

#include "tile_TEMPLATE.h"

/* ------------------------------------------------------------------ */
/*  Instance mapping                                                  */
/* ------------------------------------------------------------------ */

/** Maps instance index to device ID (I2C address, SPI CS, etc.). */
static const uint8_t id_table[] = {
    CHIPNAME_I2C_ADDR_DEFAULT,   /* instance 0 — pin floating  */
    CHIPNAME_I2C_ADDR_ALT,       /* instance 1 — pin to GND    */
};

#define ID_TABLE_LEN  (sizeof(id_table) / sizeof(id_table[0]))

/* ------------------------------------------------------------------ */
/*  Private helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief  Resolve instance index to device ID.
 * @return Device ID, or 0x00 if instance is out of range.
 */
static uint8_t resolve_id(uint8_t instance)
{
    if (instance >= ID_TABLE_LEN) return 0x00;
    return id_table[instance];
}

/** Write a single register. */
static void chip_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &value, 1);
}

/** Read a single register. */
static uint8_t chip_read(tile_t* tile, uint8_t reg)
{
    uint8_t value = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &value, 1);
    return value;
}

/** Read multiple bytes starting at a register. */
static void chip_read_buf(tile_t* tile, uint8_t reg,
                           uint8_t* buf, uint16_t len)
{
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, buf, len);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

uint8_t tile_TEMPLATE_find(tiles_pal_t* hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (addr == 0x00) return 0;

    return (hal->i2c_is_ready(hal->handle, addr) == 0) ? 1 : 0;
}

void tile_TEMPLATE_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile)
{
    tile->hal   = NULL;
    tile->id    = 0;
    tile->state = TILE_STATE_NONE;
    tile->flags = 0;
    tile->callback = NULL;
    tile->cb_ctx   = NULL;

    /* Resolve device ID */
    uint8_t addr = resolve_id(instance);
    if (addr == 0x00) {
        TILE_ON_ERROR(tile, "init: invalid instance");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    tile->hal = hal;
    tile->id  = addr;

    /* Verify device is on bus */
    if (hal->i2c_is_ready(hal->handle, addr) != 0) {
        TILE_ON_ERROR(tile, "init: device not found on bus");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Verify chip ID (if the chip has a WHO_AM_I register) */
    uint8_t whoami = chip_read(tile, CHIPNAME_WHOAMI_REG);
    if (whoami != CHIPNAME_WHOAMI_DEFAULT) {
        TILE_ON_ERROR(tile, "init: unexpected chip ID");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* --- Chip-specific initialization sequence --- */

    /* Example: soft reset
     * chip_write(tile, CHIPNAME_REG_RESET, 0x01);
     * hal->delay_ms(50);
     */

    /* Example: configure default operating mode
     * chip_write(tile, CHIPNAME_REG_CONFIG, DEFAULT_CONFIG_VALUE);
     */

    tile->state = TILE_STATE_READY;
}

uint8_t tile_TEMPLATE_data_ready(tile_t* tile)
{
    /* Example: read status register and check data-ready bit
     * uint8_t status = chip_read(tile, CHIPNAME_REG_STATUS);
     * return (status & CHIPNAME_STATUS_DRDY) ? 1 : 0;
     */
    return 0;
}

void tile_TEMPLATE_reset(tile_t* tile)
{
    /* Example: soft reset sequence
     * chip_write(tile, CHIPNAME_REG_RESET, 0x01);
     * tile->hal->delay_ms(50);
     */
}

/* --- Add tile-specific function implementations below --- */
