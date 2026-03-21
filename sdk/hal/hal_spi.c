/**
 * hal_spi.c — SPI HAL driver implementation
 */

#include "hal_spi.h"
#include "ll_rcc.h"
#include <string.h>

/* ============================================================
 * Init / Deinit
 * ============================================================ */

hal_status_t hal_spi_init(hal_spi_t *h, SPI_TypeDef *instance,
                          const hal_spi_config_t *cfg)
{
    memset(h, 0, sizeof(*h));
    h->instance = instance;
    h->cs_active_low = 1;  /* Default: CS active low */

    ll_spi_init(instance, cfg->prescaler, cfg->cpol, cfg->cpha);
    return HAL_OK;
}

void hal_spi_deinit(hal_spi_t *h)
{
    if (h && h->instance) {
        ll_spi_disable(h->instance);
        h->instance = NULL;
    }
}

void hal_spi_set_cs(hal_spi_t *h, GPIO_TypeDef *port, uint32_t pin)
{
    h->cs_port = port;
    h->cs_pin = pin;

    /* Configure CS as push-pull output and deassert */
    ll_rcc_gpio_clk_enable(port);
    ll_gpio_config_output(port, pin);
    ll_gpio_set(port, 1UL << pin);  /* Deassert (high) */
}

/* ============================================================
 * CS control
 * ============================================================ */

void hal_spi_select(hal_spi_t *h)
{
    if (h->cs_port) {
        if (h->cs_active_low)
            ll_gpio_clear(h->cs_port, 1UL << h->cs_pin);
        else
            ll_gpio_set(h->cs_port, 1UL << h->cs_pin);
    }
}

void hal_spi_deselect(hal_spi_t *h)
{
    if (h->cs_port) {
        if (h->cs_active_low)
            ll_gpio_set(h->cs_port, 1UL << h->cs_pin);
        else
            ll_gpio_clear(h->cs_port, 1UL << h->cs_pin);
    }
}

/* ============================================================
 * Polling transfer
 * ============================================================ */

uint8_t hal_spi_transfer(hal_spi_t *h, uint8_t tx)
{
    return ll_spi_transfer(h->instance, tx);
}

void hal_spi_transfer_buf(hal_spi_t *h, uint8_t *buf, uint32_t len)
{
    ll_spi_transfer_buf(h->instance, buf, len);
}

void hal_spi_write(hal_spi_t *h, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        (void)ll_spi_transfer(h->instance, data[i]);
    }
}

void hal_spi_read(hal_spi_t *h, uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = ll_spi_transfer(h->instance, 0x00);
    }
}

void hal_spi_xfer(hal_spi_t *h, const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    hal_spi_select(h);
    for (uint32_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx ? tx[i] : 0x00;
        uint8_t rx_byte = ll_spi_transfer(h->instance, tx_byte);
        if (rx) rx[i] = rx_byte;
    }
    hal_spi_deselect(h);
}

/* ============================================================
 * DMA transfer (stub)
 * ============================================================ */

hal_status_t hal_spi_xfer_dma(hal_spi_t *h, const uint8_t *tx, uint8_t *rx,
                               uint32_t len, hal_callback_t cb, void *ctx)
{
    (void)h; (void)tx; (void)rx; (void)len; (void)cb; (void)ctx;
    return HAL_ERROR;
}

int hal_spi_busy(hal_spi_t *h)
{
    return h->busy;
}
