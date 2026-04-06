/**
 * core_serial.h -- UART serial communication
 *
 * Wraps hal_uart with friendlier names. Instance and clock
 * resolution still requires explicit init (auto-resolve from
 * project.json is planned).
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

#endif /* CORE_SERIAL_H */
