/**
 * core_usb.h — USB virtual serial port (CDC)
 *
 * Printf-style output, byte-level TX/RX, and receive callbacks
 * over the USB CDC (virtual COM port) interface.
 *
 * Received bytes are drained from the ring buffer by coregen's
 * `tessera_dispatch_core_usb()` each main-loop iteration (capped to
 * 64 bytes per iteration so burst traffic doesn't starve the DSL
 * loop). The DSL-facing `Core.USB.receive(byte)` event fires once
 * per byte. Gated on `usb.enabled` in config.json — emits a no-op
 * stub when USB isn't enabled.
 *
 * @tessera category usb label=Core.USB icon=❝
 * @tessera event name=receive payload=byte:int
 */

#ifndef CORE_USB_H
#define CORE_USB_H

/* USB CDC requires a USB peripheral: Core.U (STM32L422) or Core.H (STM32H523). */
#if !defined(STM32L422xx) && !defined(STM32H523xx)
#error "core_usb.h: USB is not available on this Core tile. Only Core.U and Core.H have USB hardware."
#endif

#include "hal_usb_cdc.h"
#include <stdio.h>    /* snprintf, printf — commonly used with USB serial output */

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
 * Print a string followed by a newline. Thin wrapper for DSL-style callers.
 *
 * @tessera expose category=usb icon=❝ name=print availability=Core.U,Core.H
 * @param text The message to print.
 */
static inline void core_usb_print(const char *s)
{
    core_usb_printf("%s\n", s ? s : "");
}

/**
 * Print a signed integer followed by a newline.
 *
 * @tessera expose category=usb name=print_int availability=Core.U,Core.H
 * @param value The integer to print (decimal).
 */
static inline void core_usb_print_int(int v)
{
    core_usb_printf("%d\n", v);
}

/**
 * Print a double with a newline. %g trims trailing zeros for readability.
 *
 * @tessera expose category=usb name=print_float availability=Core.U,Core.H
 * @param value The float to print.
 */
static inline void core_usb_print_float(double v)
{
    core_usb_printf("%g\n", v);
}

/**
 * Print "true" / "false" followed by a newline.
 *
 * @tessera expose category=usb name=print_bool availability=Core.U,Core.H
 * @param value {bool} The boolean to print.
 */
static inline void core_usb_print_bool(int v)
{
    core_usb_printf("%s\n", v ? "true" : "false");
}

/**
 * Set a callback for received data.
 * Called from USB ISR. When set, data is NOT buffered for polling reads.
 * @param cb   Callback: void cb(const uint8_t *data, uint16_t len, void *ctx)
 * @param ctx  User context passed to callback; may be NULL
 */
static inline void core_usb_on_receive(hal_usb_cdc_rx_cb_t cb, void *ctx)
{
    hal_usb_cdc_set_rx_callback(cb, ctx);
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
