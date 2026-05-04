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
 *
 * @studio coverage
 *   id:    usb_hid
 *   name:  USB HID — vendor reports
 *   blurb: Tier 1 only. Single-function header for sending raw 64-byte
 *          HID reports over the composite CDC+HID device on Core.U /
 *          Core.H. Useful for high-throughput driverless host I/O
 *          (hidapi / pyusb / Web HID). No DSL surface — pointer-buffer
 *          ABI doesn't fit the current host-call shape.
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

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @studio unsupported tier=2 value=M title="No DSL surface for HID"
//   Sending a HID report needs a buffer pointer + length. The DSL's
//   array-host ABI (used for tile reads) could carry it, but no
//   default-instance wrapper is wired up.
//
// @studio unsupported tier=1 value=M title="TX only — no HID receive"
//   Wrapper sends reports; receiving host-to-device reports (Set Report
//   / output reports) is not exposed. Unidirectional comms only.
//
// @studio unsupported tier=1 value=L title="Vendor descriptor only"
//   The HID descriptor is hard-coded as a 64-byte vendor report with
//   usage page 0xFF00. No way to register custom reports (keyboard,
//   mouse, gamepad) — that needs the full HID descriptor tooling.

#endif /* CORE_USB_HID_H */
