/**
 * val-usb-rx-callback -- Validation: USB CDC receive callback + polling reads
 *
 * Core.U (Core-U-2-a), clock=max, USB enabled
 *
 * Exercises: core_init, core_usb_init, core_usb_on_receive (cb, ctx),
 *            core_usb_available, core_usb_connected, core_usb_try_read,
 *            core_usb_printf
 *
 * The rx callback signature is: void(const uint8_t *data, uint16_t len, void *ctx)
 */

#include "core.h"
#include "core_usb.h"

static volatile uint32_t rx_bytes;

/* Receive callback -- signature: void(const uint8_t *data, uint16_t len, void *ctx) */
static void on_rx(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)data;
    (void)ctx;
    rx_bytes += len;
}

int main(void)
{
    core_init();
    core_usb_init();

    /* Register receive callback -- core_usb_on_receive(cb, ctx) */
    core_usb_on_receive(on_rx, NULL);

    while (1) {
        /* Polling API -- available bytes in ring buffer */
        uint16_t avail = core_usb_available();
        (void)avail;

        /* Connection check */
        if (core_usb_connected()) {
            /* Non-blocking single byte read */
            uint8_t byte;
            int got = core_usb_try_read(&byte);

            core_usb_printf("rx_total=%lu got=%d\r\n", rx_bytes, got);
        }

        core_delay_ms(100);
    }
}
