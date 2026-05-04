/**
 * @file   tiles_pal_arduino.cpp
 * @brief  Arduino HAL implementation for tile drivers.
 *
 * Bridges tiles_pal_t to the Arduino Wire (I2C) and SPI libraries.
 * Uses blocking I/O — suitable for single-threaded sketches.
 */

#include "tiles_pal_arduino.h"
#include <SPI.h>

/* -------------------------------------------------------------- */
/* I2C callbacks                                                   */
/* -------------------------------------------------------------- */

static int arduino_i2c_read(void* handle, uint8_t addr, uint8_t reg,
                             uint8_t* data, uint16_t len)
{
    TwoWire* wire = (TwoWire*)handle;
    wire->beginTransmission(addr);
    wire->write(reg);
    if (wire->endTransmission(false) != 0) return -1;

    wire->requestFrom(addr, (uint8_t)len);
    for (uint16_t i = 0; i < len; i++) {
        if (!wire->available()) return -1;
        data[i] = wire->read();
    }
    return 0;
}

static int arduino_i2c_write(void* handle, uint8_t addr, uint8_t reg,
                              const uint8_t* data, uint16_t len)
{
    TwoWire* wire = (TwoWire*)handle;
    wire->beginTransmission(addr);
    wire->write(reg);
    wire->write(data, len);
    return (wire->endTransmission() == 0) ? 0 : -1;
}

static int arduino_i2c_is_ready(void* handle, uint8_t addr)
{
    TwoWire* wire = (TwoWire*)handle;
    wire->beginTransmission(addr);
    return (wire->endTransmission() == 0) ? 0 : -1;
}

/* -------------------------------------------------------------- */
/* SPI callbacks                                                   */
/* -------------------------------------------------------------- */

static int arduino_spi_read(void* handle, uint8_t cs, uint8_t reg,
                             uint8_t* data, uint16_t len)
{
    (void)handle;
    digitalWrite(cs, LOW);
    SPI.transfer(reg | 0x80);   /* read bit — common convention */
    for (uint16_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }
    digitalWrite(cs, HIGH);
    return 0;
}

static int arduino_spi_write(void* handle, uint8_t cs, uint8_t reg,
                              const uint8_t* data, uint16_t len)
{
    (void)handle;
    digitalWrite(cs, LOW);
    SPI.transfer(reg & 0x7F);   /* write bit — clear MSB */
    for (uint16_t i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    digitalWrite(cs, HIGH);
    return 0;
}

/* -------------------------------------------------------------- */
/* Delay                                                           */
/* -------------------------------------------------------------- */

static void arduino_delay_ms(uint32_t ms)
{
    delay(ms);
}

/* -------------------------------------------------------------- */
/* Public init                                                      */
/* -------------------------------------------------------------- */

void tiles_pal_arduino_init(tiles_pal_t* hal, const tiles_pal_arduino_cfg_t* cfg)
{
    /* I2C */
    if (cfg->buses & TILES_BUS_I2C) {
        hal->i2c_read     = arduino_i2c_read;
        hal->i2c_write    = arduino_i2c_write;
        hal->i2c_is_ready = arduino_i2c_is_ready;
        hal->handle       = (void*)(cfg->wire ? cfg->wire : &Wire);
    } else {
        hal->i2c_read     = NULL;
        hal->i2c_write    = NULL;
        hal->i2c_is_ready = NULL;
        hal->handle       = NULL;
    }

    /* SPI */
    if (cfg->buses & TILES_BUS_SPI) {
        hal->spi_read  = arduino_spi_read;
        hal->spi_write = arduino_spi_write;
    } else {
        hal->spi_read  = NULL;
        hal->spi_write = NULL;
    }

    /* Shared */
    hal->delay_ms = arduino_delay_ms;
    hal->on_error = NULL;
    hal->buses    = cfg->buses;
}
