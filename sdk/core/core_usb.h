/**
 * core_usb.h — USB virtual serial port (CDC)
 *
 * Printf-style output, byte-level TX/RX, and receive callbacks
 * over the USB CDC (virtual COM port) interface.
 */

#ifndef CORE_USB_H
#define CORE_USB_H

#include "hal_usb_cdc.h"

/** Initialize USB CDC. Device appears as /dev/tty.usbmodem* on the host. */
static inline void core_usb_init(void)
{
    hal_usb_cdc_init();
}

/** Returns 1 if a host terminal is connected (DTR set). */
static inline int core_usb_connected(void)
{
    return hal_usb_cdc_connected();
}

/** Transmit data (blocking). Returns bytes sent, or -1 if not configured. */
static inline int core_usb_write(const uint8_t *buf, uint16_t len)
{
    return hal_usb_cdc_write(buf, len);
}

/** Printf over USB CDC (blocking). */
#define core_usb_printf  hal_usb_cdc_printf

/**
 * Set a callback for received data.
 * Called from USB ISR. When set, data is NOT buffered for polling reads.
 */
static inline void core_usb_on_receive(hal_usb_cdc_rx_cb_t cb)
{
    hal_usb_cdc_set_rx_callback(cb);
}

/** Returns the number of bytes available to read (ring buffer mode). */
static inline uint16_t core_usb_available(void)
{
    return hal_usb_cdc_available();
}

/** Read a single byte (blocking — waits for data). */
static inline uint8_t core_usb_getc(void)
{
    return hal_usb_cdc_getc();
}

/** Read available bytes into buf (non-blocking). Returns bytes read. */
static inline uint16_t core_usb_read(uint8_t *buf, uint16_t max)
{
    return hal_usb_cdc_read(buf, max);
}

/** Non-blocking single byte read. Returns 1 if a byte was read, 0 if empty. */
static inline int core_usb_try_read(uint8_t *byte)
{
    return hal_usb_cdc_rx_try(byte);
}

#endif /* CORE_USB_H */
