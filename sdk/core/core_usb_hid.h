/**
 * core_usb_hid.h — USB HID (generic vendor reports)
 *
 * Send raw binary reports to the host over USB HID. No drivers
 * needed on the host — works with hidapi, pyusb, or any HID reader.
 *
 * Reports are automatically zero-padded to 64 bytes. You can send
 * any struct up to 64 bytes — just cast to uint8_t* and pass its size.
 *
 * The HID interface is part of the composite CDC+HID device —
 * call core_usb_init() first, then use core_usb_hid_send().
 *
 * Only available on Core.U (STM32L422) and Core.H (STM32H523).
 *
 * VID:1209 PID:0001, Interface 2 = HID (vendor-defined, usage page 0xFF00).
 */

#ifndef CORE_USB_HID_H
#define CORE_USB_HID_H

#if !defined(STM32L422xx) && !defined(STM32H523xx)
#error "core_usb_hid.h: USB HID is not available on this Core tile. Only Core.U and Core.H have USB hardware."
#endif

#include "hal_usb_cdc.h"

/**
 * Send a HID report (up to 64 bytes, zero-padded automatically).
 * Blocking — waits for the previous report to be read by the host.
 * Returns bytes of user data sent, or -1 if USB not configured.
 */
static inline int core_usb_hid_send(const uint8_t *buf, uint16_t len)
{
    return hal_usb_hid_send_report(buf, len);
}

#endif /* CORE_USB_HID_H */
