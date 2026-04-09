/**
 * core_spi.h -- SPI bus communication
 *
 * Wraps hal_spi with friendlier names.
 */

#ifndef CORE_SPI_H
#define CORE_SPI_H

#include "hal_spi.h"
#include "hal_gpio.h"

/* ---- Init ---- */

/** Initialize SPI in master mode. Same signature as hal_spi_init. */
static inline hal_status_t core_spi_init(hal_spi_t *h,
                                          SPI_TypeDef *instance,
                                          const hal_spi_config_t *cfg)
{
    return hal_spi_init(h, instance, cfg);
}

/* ---- CS management ---- */

/**
 * Assign a CS pin using a tile pad number.
 * Resolves pad to port/pin via hal_pad_lookup, then calls
 * hal_spi_set_cs to configure and deassert the pin.
 */
static inline void core_spi_set_cs(hal_spi_t *h, uint8_t pad)
{
    hal_pad_gpio_t g = hal_pad_lookup(pad);
    if (g.port)
        hal_spi_set_cs(h, g.port, g.pin);
}

/** Assert CS (drive low). */
static inline void core_spi_select(hal_spi_t *h)
{
    hal_spi_select(h);
}

/** Deassert CS (drive high). */
static inline void core_spi_deselect(hal_spi_t *h)
{
    hal_spi_deselect(h);
}

/* ---- Polling transfer ---- */

/** Full-duplex single byte transfer. Returns received byte. */
static inline uint8_t core_spi_transfer(hal_spi_t *h, uint8_t tx)
{
    return hal_spi_transfer(h, tx);
}

/** Write-only (discard received data). */
static inline void core_spi_write(hal_spi_t *h, const uint8_t *data,
                                   uint32_t len)
{
    hal_spi_write(h, data, len);
}

/** Read-only (send zeros). */
static inline void core_spi_read(hal_spi_t *h, uint8_t *buf, uint32_t len)
{
    hal_spi_read(h, buf, len);
}

/**
 * Convenience: select + full-duplex transfer + deselect.
 * tx and rx can be the same buffer. Either can be NULL.
 */
static inline void core_spi_xfer(hal_spi_t *h, const uint8_t *tx,
                                  uint8_t *rx, uint32_t len)
{
    hal_spi_xfer(h, tx, rx, len);
}

/* ---- DMA transfer (non-blocking) ---- */

/**
 * Start a DMA-based SPI transfer (non-blocking).
 *
 * Full-duplex: tx bytes are sent while rx bytes are received
 * simultaneously. Either tx or rx can be NULL for write-only or
 * read-only transfers. The callback fires from DMA ISR context
 * when the transfer completes.
 *
 * Caller must manage CS: assert before calling, deassert in
 * the callback. The command/address byte should be sent via
 * polling (core_spi_transfer) before starting DMA.
 *
 * @param h    SPI handle
 * @param tx   TX buffer (NULL = send zeros)
 * @param rx   RX buffer (NULL = discard received data)
 * @param len  Number of bytes to transfer
 * @param cb   Completion callback (called from DMA ISR)
 * @param ctx  User context for callback
 * @return HAL_OK on success, HAL_BUSY if a DMA transfer is active,
 *         HAL_ERROR if DMA is not available on this platform
 */
static inline hal_status_t core_spi_xfer_dma(hal_spi_t *h,
                                              const uint8_t *tx,
                                              uint8_t *rx,
                                              uint32_t len,
                                              hal_callback_t cb, void *ctx)
{
    return hal_spi_xfer_dma(h, tx, rx, len, cb, ctx);
}

/** Returns 1 if a DMA transfer is in progress. */
static inline int core_spi_busy(hal_spi_t *h)
{
    return hal_spi_busy(h);
}

#endif /* CORE_SPI_H */
