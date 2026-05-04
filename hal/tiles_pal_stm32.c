/**
 * @file   tiles_pal_stm32.c
 * @brief  STM32 HAL implementation for tile drivers.
 *
 * Bridges tiles_pal_t to the STM32 HAL I2C and SPI APIs.
 * Uses blocking transfers. For DMA-based transfers, copy this file
 * and swap in the _DMA variants of the HAL calls.
 */

#include "tiles_pal_stm32.h"

#define STM32_HAL_TIMEOUT_MS  50

/* Store cfg pointer so SPI callbacks can access CS port/pin mappings */
static const tiles_pal_stm32_cfg_t* s_cfg;

/* -------------------------------------------------------------- */
/* I2C callbacks                                                   */
/* -------------------------------------------------------------- */

static int stm32_i2c_read(void* handle, uint8_t addr, uint16_t reg,
                           uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    uint16_t mem_size = (reg > 0xFF) ? 2 : 1;
    return (int)HAL_I2C_Mem_Read(hi2c, (uint16_t)(addr << 1), reg, mem_size,
                                  data, len, STM32_HAL_TIMEOUT_MS);
}

static int stm32_i2c_write(void* handle, uint8_t addr, uint16_t reg,
                            const uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    uint16_t mem_size = (reg > 0xFF) ? 2 : 1;
    return (int)HAL_I2C_Mem_Write(hi2c, (uint16_t)(addr << 1), reg, mem_size,
                                   (uint8_t*)data, len, STM32_HAL_TIMEOUT_MS);
}

static int stm32_i2c_is_ready(void* handle, uint8_t addr)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 3,
                                       STM32_HAL_TIMEOUT_MS);
}

static int stm32_i2c_write_raw(void* handle, uint8_t addr,
                                const uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_Master_Transmit(hi2c, (uint16_t)(addr << 1),
                                         (uint8_t*)data, len, STM32_HAL_TIMEOUT_MS);
}

static int stm32_i2c_read_raw(void* handle, uint8_t addr,
                               uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_Master_Receive(hi2c, (uint16_t)(addr << 1),
                                        data, len, STM32_HAL_TIMEOUT_MS);
}

/* -------------------------------------------------------------- */
/* SPI callbacks                                                   */
/* -------------------------------------------------------------- */

static int stm32_spi_read(void* handle, uint8_t cs, uint8_t reg,
                           uint8_t* data, uint16_t len)
{
    (void)handle;
    SPI_HandleTypeDef* hspi = s_cfg->spi;

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_RESET);

    uint8_t cmd = reg | 0x80;
    HAL_SPI_Transmit(hspi, &cmd, 1, STM32_HAL_TIMEOUT_MS);
    HAL_StatusTypeDef status = HAL_SPI_Receive(hspi, data, len,
                                                STM32_HAL_TIMEOUT_MS);

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_SET);

    return (int)status;
}

static int stm32_spi_write(void* handle, uint8_t cs, uint8_t reg,
                            const uint8_t* data, uint16_t len)
{
    (void)handle;
    SPI_HandleTypeDef* hspi = s_cfg->spi;

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_RESET);

    uint8_t cmd = reg & 0x7F;
    HAL_SPI_Transmit(hspi, &cmd, 1, STM32_HAL_TIMEOUT_MS);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hspi, (uint8_t*)data, len,
                                                 STM32_HAL_TIMEOUT_MS);

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_SET);

    return (int)status;
}

/* -------------------------------------------------------------- */
/* Delay                                                           */
/* -------------------------------------------------------------- */

static void stm32_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* -------------------------------------------------------------- */
/* Public init                                                      */
/* -------------------------------------------------------------- */

void tiles_pal_stm32_init(tiles_pal_t* hal, const tiles_pal_stm32_cfg_t* cfg)
{
    s_cfg = cfg;

    /* I2C */
    if (cfg->buses & TILES_BUS_I2C) {
        hal->i2c_read      = stm32_i2c_read;
        hal->i2c_write     = stm32_i2c_write;
        hal->i2c_is_ready  = stm32_i2c_is_ready;
        hal->i2c_write_raw = stm32_i2c_write_raw;
        hal->i2c_read_raw  = stm32_i2c_read_raw;
        hal->handle        = (void*)cfg->i2c;
    } else {
        hal->i2c_read      = NULL;
        hal->i2c_write     = NULL;
        hal->i2c_is_ready  = NULL;
        hal->i2c_write_raw = NULL;
        hal->i2c_read_raw  = NULL;
        hal->handle        = NULL;
    }

    /* SPI */
    if (cfg->buses & TILES_BUS_SPI) {
        hal->spi_read  = stm32_spi_read;
        hal->spi_write = stm32_spi_write;
    } else {
        hal->spi_read  = NULL;
        hal->spi_write = NULL;
    }

    /* Shared */
    hal->delay_ms = stm32_delay_ms;
    hal->on_error = NULL;
    hal->buses    = cfg->buses;
}
