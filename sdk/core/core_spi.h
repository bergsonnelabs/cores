/**
 * core_spi.h — SPI master bus communication
 *
 * Master-mode SPI with software CS management, polling and DMA
 * transfers. Wraps hal_spi with pad-based convenience.
 *
 * Quick start (polling):
 * @code
 *   hal_spi_t spi;
 *   hal_spi_config_t cfg = { .prescaler = LL_SPI_PRESCALER_8 };
 *   core_spi_init(&spi, SPI1, &cfg);
 *   core_spi_set_cs(&spi, 11);         // Pad 11 as CS
 *
 *   core_spi_select(&spi);
 *   uint8_t who = core_spi_transfer(&spi, 0xF5);  // Read reg 0x75
 *   core_spi_deselect(&spi);
 * @endcode
 *
 * Available on: Core.U, Core.W, Core.H (not Core.L — no SPI peripheral).
 *
 * @tessera coverage
 *   id:    spi
 *   name:  SPI — bus communication
 *   page:  /docs/sdk/spi
 *   blurb: Master-mode SPI: polled byte / buffer transfer, software CS
 *          via tile pads, and DMA non-blocking transfers (Core.U
 *          verified; Core.W/H DMA is HAL-side WIP). Tier 1 only —
 *          tile drivers consume SPI through core_tiles_pal, so the
 *          DSL doesn't need a direct SPI surface yet.
 */

#ifndef CORE_SPI_H
#define CORE_SPI_H

/* SPI is not available on Core.L (STM32L011) */
#if defined(STM32L011xx)
#error "core_spi.h: SPI is not available on Core.L. Only Core.U, Core.W, and Core.H have SPI."
#endif

#include "hal_spi.h"
#include "hal_gpio.h"

/** SPI handle type — same as hal_spi_t under the hood. */
typedef hal_spi_t core_spi_t;

/** SPI configuration — prescaler, clock polarity, clock phase. */
typedef hal_spi_config_t core_spi_config_t;

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

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=H title="No Tier 2 (default-instance) helpers"
//   Pattern identical to I2C and Serial: callers thread their own
//   hal_spi_t through every call. A coregen-resolved
//   core_spi_xfer_bus(bus_id, ...) would mean tile drivers (and any
//   future DSL surface) wouldn't have to plumb the handle by hand.
//
// @tessera unsupported tier=1 value=H title="SPI master broken on Core.W; compile-only on Core.H"
//   SDK roadmap Tier 1 item: Core.W has an SPI v2 CSTART bug; Core.H
//   builds but isn't hardware-verified. Only Core.U is end-to-end
//   verified for polled + FIFO + Kiln-driver use. Tile drivers that
//   need SPI on W/H should expect rough edges.
//
// @tessera unsupported tier=2 value=M title="DMA verified only on Core.U"
//   SDK roadmap Tier 2: Core.W / Core.H GPDMA is deferred (SPI v2
//   TSIZE constraints). Calls fall back to polled or hit
//   HAL_ERROR. Long buffer transfers (display refresh, audio) are
//   significantly slower on W/H than on U.
//
// @tessera unsupported tier=1 value=M title="Slave mode missing"
//   Master-only. No path for a Core to act as a SPI peripheral on
//   another host's bus.
//
// @tessera unsupported tier=1 value=L title="Quad / Octo SPI / OctoSPI"
//   SDK roadmap Tier 2: QUADSPI / OctoSPI (memory-mapped external
//   flash for NOR / PSRAM tiles) is wired only on Core.U + Core.H
//   silicon and isn't wrapped here.

#endif /* CORE_SPI_H */
