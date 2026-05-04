/**
 * @file   tiles_pal_esp_idf.c
 * @brief  ESP-IDF HAL implementation for tile drivers.
 *
 * Bridges tiles_pal_t to the ESP-IDF I2C master and SPI master APIs.
 * Uses the esp_idf >= v5.x i2c_master driver.
 */

#include "tiles_pal_esp_idf.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define ESP_HAL_TIMEOUT_MS  100

/* Store the cfg pointer so SPI callbacks can access spi_host/clock */
static const tiles_pal_esp_idf_cfg_t* s_cfg;

/* -------------------------------------------------------------- */
/* I2C callbacks                                                   */
/* -------------------------------------------------------------- */

static int esp_i2c_read(void* handle, uint8_t addr, uint8_t reg,
                         uint8_t* data, uint16_t len)
{
    i2c_master_bus_handle_t bus = (i2c_master_bus_handle_t)handle;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) != ESP_OK) return -1;

    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, data, len,
                                                 ESP_HAL_TIMEOUT_MS);
    i2c_master_bus_rm_device(dev);
    return (err == ESP_OK) ? 0 : -1;
}

static int esp_i2c_write(void* handle, uint8_t addr, uint8_t reg,
                          const uint8_t* data, uint16_t len)
{
    i2c_master_bus_handle_t bus = (i2c_master_bus_handle_t)handle;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) != ESP_OK) return -1;

    uint8_t buf[len + 1];
    buf[0] = reg;
    memcpy(buf + 1, data, len);

    esp_err_t err = i2c_master_transmit(dev, buf, len + 1, ESP_HAL_TIMEOUT_MS);
    i2c_master_bus_rm_device(dev);
    return (err == ESP_OK) ? 0 : -1;
}

static int esp_i2c_is_ready(void* handle, uint8_t addr)
{
    i2c_master_bus_handle_t bus = (i2c_master_bus_handle_t)handle;
    return (i2c_master_probe(bus, addr, ESP_HAL_TIMEOUT_MS) == ESP_OK) ? 0 : -1;
}

/* -------------------------------------------------------------- */
/* SPI callbacks                                                   */
/* -------------------------------------------------------------- */

static int esp_spi_read(void* handle, uint8_t cs, uint8_t reg,
                         uint8_t* data, uint16_t len)
{
    (void)handle;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = s_cfg->spi_clock_hz,
        .mode = 0,
        .spics_io_num = s_cfg->spi_cs_pins[cs],
        .queue_size = 1,
    };
    spi_device_handle_t dev;
    if (spi_bus_add_device(s_cfg->spi_host, &devcfg, &dev) != ESP_OK) return -1;

    uint8_t cmd = reg | 0x80;
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(dev, &t);

    spi_transaction_t r = {0};
    r.length = len * 8;
    r.rx_buffer = data;
    esp_err_t err = spi_device_transmit(dev, &r);

    spi_bus_remove_device(dev);
    return (err == ESP_OK) ? 0 : -1;
}

static int esp_spi_write(void* handle, uint8_t cs, uint8_t reg,
                          const uint8_t* data, uint16_t len)
{
    (void)handle;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = s_cfg->spi_clock_hz,
        .mode = 0,
        .spics_io_num = s_cfg->spi_cs_pins[cs],
        .queue_size = 1,
    };
    spi_device_handle_t dev;
    if (spi_bus_add_device(s_cfg->spi_host, &devcfg, &dev) != ESP_OK) return -1;

    uint8_t buf[len + 1];
    buf[0] = reg & 0x7F;
    memcpy(buf + 1, data, len);

    spi_transaction_t t = {0};
    t.length = (len + 1) * 8;
    t.tx_buffer = buf;
    esp_err_t err = spi_device_transmit(dev, &t);

    spi_bus_remove_device(dev);
    return (err == ESP_OK) ? 0 : -1;
}

/* -------------------------------------------------------------- */
/* Delay                                                           */
/* -------------------------------------------------------------- */

static void esp_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* -------------------------------------------------------------- */
/* Public init                                                      */
/* -------------------------------------------------------------- */

void tiles_pal_esp_idf_init(tiles_pal_t* hal, const tiles_pal_esp_idf_cfg_t* cfg)
{
    s_cfg = cfg;

    /* I2C */
    if (cfg->buses & TILES_BUS_I2C) {
        hal->i2c_read     = esp_i2c_read;
        hal->i2c_write    = esp_i2c_write;
        hal->i2c_is_ready = esp_i2c_is_ready;
        hal->handle       = (void*)cfg->i2c_bus;
    } else {
        hal->i2c_read     = NULL;
        hal->i2c_write    = NULL;
        hal->i2c_is_ready = NULL;
        hal->handle       = NULL;
    }

    /* SPI */
    if (cfg->buses & TILES_BUS_SPI) {
        hal->spi_read  = esp_spi_read;
        hal->spi_write = esp_spi_write;
    } else {
        hal->spi_read  = NULL;
        hal->spi_write = NULL;
    }

    /* Shared */
    hal->delay_ms = esp_delay_ms;
    hal->on_error = NULL;
    hal->buses    = cfg->buses;
}
