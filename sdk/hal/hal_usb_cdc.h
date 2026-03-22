/**
 * hal_usb_cdc.h — USB CDC (Virtual Serial Port) driver
 *
 * Implements USB Device with CDC-ACM class on STM32L422.
 * When initialized, the device appears as /dev/tty.usbmodem*
 * on the host. Provides printf-style output and byte-level RX.
 *
 * Crystal-less operation via HSI48 + CRS (synced to USB SOF).
 */

#ifndef HAL_USB_CDC_H
#define HAL_USB_CDC_H

#include "hal_common.h"
#include <stdint.h>
#include <stdarg.h>

#if defined(STM32L422xx)

/* ============================================================
 * Configuration
 * ============================================================ */

#ifndef HAL_USB_CDC_RX_BUF_SIZE
  #define HAL_USB_CDC_RX_BUF_SIZE   256   /* Must be power of 2 */
#endif

#ifndef HAL_USB_CDC_PRINTF_BUF_SIZE
  #define HAL_USB_CDC_PRINTF_BUF_SIZE 256
#endif

/* ============================================================
 * RX callback type
 * ============================================================ */

/** Called from USB ISR when data is received on EP1 OUT. */
typedef void (*hal_usb_cdc_rx_cb_t)(const uint8_t *data, uint16_t len);

/* ============================================================
 * API
 * ============================================================ */

/**
 * Initialize USB CDC.
 *
 * This function:
 *   - Enables HSI48 + CRS (crystal-less 48MHz for USB)
 *   - Selects HSI48 as USB clock source
 *   - Enables VDDUSB power supply
 *   - Configures PA11/PA12 as AF10
 *   - Enables USB peripheral clock
 *   - Configures USB device and endpoints
 *   - Enables USB interrupt
 *   - Connects DP pull-up (host sees device)
 */
void hal_usb_cdc_init(void);

/**
 * Check if the host has opened the virtual COM port.
 * Returns 1 if configured and DTR is set (terminal connected).
 */
int hal_usb_cdc_connected(void);

/* ---- TX (device -> host) ---- */

/**
 * Transmit data over USB CDC (blocking).
 * Waits for the host to consume data if the EP is busy.
 * Returns number of bytes sent, or -1 if not configured.
 */
int hal_usb_cdc_write(const uint8_t *buf, uint16_t len);

/**
 * Printf over USB CDC (blocking).
 * Returns number of characters written (from vsnprintf).
 */
int hal_usb_cdc_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/* ---- RX ---- */

/**
 * Set a callback for received data.
 * Called from the USB ISR with a pointer to data copied from PMA.
 * If set, data is NOT stored in the ring buffer.
 * If not set, data goes into the ring buffer for polling reads.
 */
void hal_usb_cdc_set_rx_callback(hal_usb_cdc_rx_cb_t cb);

/**
 * Check if data is available to read (ring buffer mode).
 */
int hal_usb_cdc_rx_ready(void);

/**
 * Read a single byte (blocking — waits for data).
 */
uint8_t hal_usb_cdc_getc(void);

/**
 * Try to read a byte without blocking.
 * Returns 1 if a byte was read, 0 if no data available.
 */
int hal_usb_cdc_rx_try(uint8_t *byte);

/**
 * Read available bytes into buffer (non-blocking).
 * Returns number of bytes actually read.
 */
uint16_t hal_usb_cdc_read(uint8_t *buf, uint16_t max_len);

/**
 * Number of bytes available in the RX buffer.
 */
uint16_t hal_usb_cdc_available(void);

#endif /* STM32L422xx */

#endif /* HAL_USB_CDC_H */
