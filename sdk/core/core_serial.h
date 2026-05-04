/**
 * core_serial.h -- UART serial communication
 *
 * Wraps hal_uart with friendlier names. Instance and clock
 * resolution still requires explicit init (auto-resolve from
 * config.json is planned).
 *
 * @tessera coverage
 *   id:    serial
 *   name:  Serial — UART
 *   page:  /docs/sdk/uart
 *   blurb: Polled UART send / receive (write, print, putc, getc, read,
 *          available) with friendly names over hal_uart. Tier 1 only —
 *          there's no default-instance Tier 2 helper for UART yet, and
 *          coregen doesn't auto-init a serial port from config.json.
 *          Apps that want printf-style output to a host should prefer
 *          Core.USB; this header is for hardware UART pins (RS-485,
 *          GPS, side-channel debug).
 */

#ifndef CORE_SERIAL_H
#define CORE_SERIAL_H

#include "hal_uart.h"

/* ---- Init ---- */

/** Initialize a UART instance.
 *  Clock is auto-resolved from PCLK1_HZ (core_config.h).
 */
static inline hal_status_t core_serial_init(hal_uart_t *h,
                                             USART_TypeDef *instance,
                                             const hal_uart_config_t *cfg)
{
    return hal_uart_init(h, instance, PCLK1_HZ, cfg);
}

/** @deprecated Use core_serial_init(h, instance, cfg) — clock auto-resolved. */
static inline hal_status_t core_serial_init_clk(hal_uart_t *h,
                                                 USART_TypeDef *instance,
                                                 uint32_t pclk_hz,
                                                 const hal_uart_config_t *cfg)
{
    return hal_uart_init(h, instance, pclk_hz, cfg);
}

/* ---- TX ---- */

/** Transmit a buffer (blocking). */
static inline void core_serial_write(hal_uart_t *h, const uint8_t *data,
                                      uint32_t len)
{
    hal_uart_tx(h, data, len);
}

/** Transmit a null-terminated string (blocking). */
static inline void core_serial_print(hal_uart_t *h, const char *str)
{
    hal_uart_tx_str(h, str);
}

/** Printf over UART (blocking). */
#define core_serial_printf  hal_uart_printf

/** Transmit a single byte (blocking). */
static inline void core_serial_putc(hal_uart_t *h, uint8_t byte)
{
    hal_uart_putc(h, byte);
}

/* ---- RX ---- */

/** Number of bytes available in the RX buffer. */
static inline uint16_t core_serial_available(hal_uart_t *h)
{
    return hal_uart_available(h);
}

/** Receive a single byte (blocking -- waits for data). */
static inline uint8_t core_serial_getc(hal_uart_t *h)
{
    return hal_uart_rx(h);
}

/**
 * Read available bytes from the RX ring buffer (non-blocking).
 * Returns number of bytes read.
 */
static inline uint16_t core_serial_read(hal_uart_t *h, uint8_t *buf,
                                         uint16_t max_len)
{
    return hal_uart_read(h, buf, max_len);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=H title="No Tier 2 (default-instance) helpers"
//   Same gap pattern as I2C: callers have to declare and init a
//   hal_uart_t themselves, then thread it through every call. A
//   coregen-resolved core_serial_print(text), core_serial_write_bus(...)
//   would let the DSL ship a Core.Serial category.
//
// @tessera unsupported tier=1 value=H title="No interrupt / DMA driven path"
//   All calls block. SDK roadmap Tier 1 item: "UART IRQ + coregen"
//   for non-blocking serial (GPS, RS-485). Long reads currently stall
//   the main loop until the buffer fills or times out.
//
// @tessera unsupported tier=1 value=M title="No flow control / parity / 9-bit modes"
//   The thin wrappers expose only the basics — RTS/CTS, parity, stop
//   bits, 9-bit data, and inversion are reachable only via the
//   hal_uart_config_t struct passed to init.
//
// @tessera unsupported tier=1 value=M title="No LPUART (low-power UART)"
//   SDK roadmap Tier 2: LPUART works in Stop mode for wake-on-serial.
//   Compile-only on every Core today; not surfaced through this header.

#endif /* CORE_SERIAL_H */
