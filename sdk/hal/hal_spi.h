/**
 * hal_spi.h — SPI HAL driver
 *
 * Master-mode SPI with CS pin management, polling and DMA transfers.
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "hal_common.h"
#include "ll_spi.h"
#include "ll_gpio.h"

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    uint32_t prescaler;     /* LL_SPI_PRESCALER_* */
    uint8_t  cpol;          /* LL_SPI_CPOL_LOW or _HIGH */
    uint8_t  cpha;          /* LL_SPI_CPHA_1EDGE or _2EDGE */
} hal_spi_config_t;

typedef struct {
    SPI_TypeDef  *instance;

    /* CS pin (optional — NULL means no CS management) */
    GPIO_TypeDef *cs_port;
    uint32_t      cs_pin;
    uint8_t       cs_active_low;  /* 1 = CS active low (default) */

    /* DMA state */
    volatile uint8_t busy;
    hal_callback_t   complete_cb;
    void            *complete_ctx;
} hal_spi_t;

/* ============================================================
 * API declarations (implemented in hal_spi.c)
 * ============================================================ */

/**
 * Initialize SPI in master mode, 8-bit, MSB-first.
 * The peripheral clock is auto-enabled. SCK/MOSI/MISO pins
 * must be configured for AF (via tile_init or manually).
 */
hal_status_t hal_spi_init(hal_spi_t *h, SPI_TypeDef *instance,
                          const hal_spi_config_t *cfg);

void hal_spi_deinit(hal_spi_t *h);

/**
 * Assign a CS GPIO pin for automatic chip-select management.
 * The pin is configured as push-pull output and deasserted (high).
 */
void hal_spi_set_cs(hal_spi_t *h, GPIO_TypeDef *port, uint32_t pin);

/* ---- CS control ---- */

/** Assert CS (drive low) */
void hal_spi_select(hal_spi_t *h);

/** Deassert CS (drive high) */
void hal_spi_deselect(hal_spi_t *h);

/* ---- Polling transfer ---- */

/** Full-duplex single byte transfer */
uint8_t hal_spi_transfer(hal_spi_t *h, uint8_t tx);

/** Full-duplex buffer transfer (in-place: rx overwrites buf) */
void hal_spi_transfer_buf(hal_spi_t *h, uint8_t *buf, uint32_t len);

/** Write-only (discard received data) */
void hal_spi_write(hal_spi_t *h, const uint8_t *data, uint32_t len);

/** Read-only (send zeros) */
void hal_spi_read(hal_spi_t *h, uint8_t *buf, uint32_t len);

/**
 * Convenience: select + full-duplex transfer + deselect.
 * tx and rx can be the same buffer. Either can be NULL.
 */
void hal_spi_xfer(hal_spi_t *h, const uint8_t *tx, uint8_t *rx, uint32_t len);

/* ---- DMA transfer (non-blocking) ---- */

hal_status_t hal_spi_xfer_dma(hal_spi_t *h, const uint8_t *tx, uint8_t *rx,
                               uint32_t len, hal_callback_t cb, void *ctx);

int hal_spi_busy(hal_spi_t *h);

#endif /* HAL_SPI_H */
