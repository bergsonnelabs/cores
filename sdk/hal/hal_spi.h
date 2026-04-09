/**
 * hal_spi.h — SPI HAL driver
 *
 * Master-mode SPI with software CS pin management, polling and DMA
 * transfers. Supports the old SPI IP (L0/L4) and SPI v2 (WBA/H5).
 *
 * CS is managed via GPIO (not hardware NSS) for predictable
 * per-byte control. Assign a CS pin with hal_spi_set_cs() or
 * manage CS externally via the tiles_hal_core CS array.
 *
 * DMA: hal_spi_xfer_dma() provides non-blocking full-duplex
 * transfers with completion callback. Currently implemented for
 * STM32L422 (classic DMA); WBA/H5 GPDMA deferred.
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

typedef struct hal_spi {
    SPI_TypeDef  *instance;

    /* CS pin (optional — NULL means no CS management) */
    GPIO_TypeDef *cs_port;
    uint32_t      cs_pin;
    uint8_t       cs_active_low;  /* 1 = CS active low (default) */

    /* DMA state */
    volatile uint8_t busy;
    hal_callback_t   complete_cb;
    void            *complete_ctx;
    uint8_t         *dma_rx_buf;   /* DMA destination buffer */
    uint32_t         dma_len;      /* DMA transfer length */
} hal_spi_t;

/* ============================================================
 * API declarations (implemented in hal_spi.c)
 * ============================================================ */

/**
 * Initialize SPI in master mode, 8-bit, MSB-first.
 * The peripheral clock is auto-enabled. SCK/MOSI/MISO pins
 * must be configured for AF (via coregen or manually).
 *
 * @param h         Handle (zero-initialized by this function)
 * @param instance  SPI peripheral (SPI1, SPI2, SPI3)
 * @param cfg       Clock prescaler, polarity, and phase
 * @return HAL_OK on success
 */
hal_status_t hal_spi_init(hal_spi_t *h, SPI_TypeDef *instance,
                          const hal_spi_config_t *cfg);

/** Disable the SPI peripheral and release the handle. */
void hal_spi_deinit(hal_spi_t *h);

/**
 * Assign a CS GPIO pin for automatic chip-select management.
 * The pin is configured as push-pull output and deasserted (high).
 *
 * @param h     SPI handle
 * @param port  GPIO port (e.g. GPIOA)
 * @param pin   Pin number (0–15)
 */
void hal_spi_set_cs(hal_spi_t *h, GPIO_TypeDef *port, uint32_t pin);

/* ---- CS control ---- */

/** Assert CS (drive active — low by default). */
void hal_spi_select(hal_spi_t *h);

/** Deassert CS (drive inactive — high by default). */
void hal_spi_deselect(hal_spi_t *h);

/* ---- Polling transfer (blocking) ---- */

/** Full-duplex single byte: send tx, return received byte. */
uint8_t hal_spi_transfer(hal_spi_t *h, uint8_t tx);

/** Full-duplex buffer transfer (in-place: rx overwrites buf). */
void hal_spi_transfer_buf(hal_spi_t *h, uint8_t *buf, uint32_t len);

/** Write-only: send data, discard received bytes. */
void hal_spi_write(hal_spi_t *h, const uint8_t *data, uint32_t len);

/** Read-only: send zeros, capture received bytes. */
void hal_spi_read(hal_spi_t *h, uint8_t *buf, uint32_t len);

/**
 * Convenience: select → full-duplex transfer → deselect.
 * tx and rx can be the same buffer. Either can be NULL.
 */
void hal_spi_xfer(hal_spi_t *h, const uint8_t *tx, uint8_t *rx, uint32_t len);

/* ---- DMA transfer (non-blocking) ---- */

/**
 * Start a full-duplex DMA transfer.
 *
 * @param h    SPI handle
 * @param tx   TX buffer (NULL = send zeros)
 * @param rx   RX buffer (NULL = discard received data)
 * @param len  Number of bytes
 * @param cb   Completion callback (from DMA ISR context)
 * @param ctx  User context for callback
 * @return HAL_OK, HAL_BUSY, or HAL_ERROR (DMA not available)
 */
hal_status_t hal_spi_xfer_dma(hal_spi_t *h, const uint8_t *tx, uint8_t *rx,
                               uint32_t len, hal_callback_t cb, void *ctx);

/** Returns 1 if a DMA transfer is in progress. */
int hal_spi_busy(hal_spi_t *h);

#endif /* HAL_SPI_H */
