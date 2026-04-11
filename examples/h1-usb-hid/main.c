/**
 * USB Composite (CDC + HID) — demo
 *
 * CDC: prints heartbeat with HID report count every second
 * HID: sends a vendor-defined report every 100ms
 *
 * On the host:
 *   CDC: screen /dev/tty.usbmodem* 115200
 *   HID: python3 hid_read.py
 *
 * Both interfaces work simultaneously.
 */

#include "core.h"
#include "core_usb.h"
#include "core_usb_hid.h"

typedef struct __attribute__((packed)) {
    uint32_t counter;
    uint16_t sensor;
} hid_report_t;

int main(void)
{
    core_init();
    core_usb_init();
    core_pad_output(9);

    hid_report_t report = { 0, 0 };
    uint32_t last_cdc = 0;
    uint16_t sample = 0;

    while (1) {
        uint32_t now = _systick_ticks;

        /* CDC heartbeat every second */
        if (now - last_cdc >= 1000) {
            last_cdc = now;
            core_pad_toggle(9);
            if (core_usb_connected()) {
                core_usb_printf("HID reports sent: %lu\r\n", report.counter);
            }
        }

        /* HID report every 100ms.
         * Reports are zero-padded to 64 bytes by the driver. */
        report.counter++;
        report.sensor = sample++;   /* Simulated ramp — replace with real sensor */

        core_usb_hid_send((const uint8_t *)&report, sizeof(report));

        core_delay_ms(100);
    }
}
