/**
 * DFU Test — USB CDC serial with "dfu" reboot command
 *
 * Blinks LED and prints a heartbeat over USB CDC.
 * Type "dfu" + Enter to reboot into the DFU bootloader.
 */

#include "core.h"
#include "hal_usb_cdc.h"
#include "hal_dfu.h"

/* Simple command buffer */
static char cmd_buf[16];
static uint8_t cmd_len;

static void check_serial(void)
{
    uint8_t byte;
    while (hal_usb_cdc_rx_try(&byte)) {
        if (byte == '\r' || byte == '\n') {
            if (cmd_len > 0) {
                cmd_buf[cmd_len] = '\0';

                if (cmd_len == 3 &&
                    cmd_buf[0] == 'd' &&
                    cmd_buf[1] == 'f' &&
                    cmd_buf[2] == 'u') {
                    hal_usb_cdc_printf("Rebooting to DFU...\r\n");
                    /* Small delay so the message gets sent */
                    core_delay_ms(50);
                    hal_dfu_reboot();
                } else {
                    hal_usb_cdc_printf("Unknown: %s\r\n", cmd_buf);
                }
                cmd_len = 0;
            }
        } else {
            if (cmd_len < sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_len++] = (char)byte;
            }
        }
    }
}

int main(void)
{
    core_init();
    core_led_init();
    hal_usb_cdc_init();

    uint32_t last_print = 0;
    uint32_t count = 0;

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);

        check_serial();

        if (hal_usb_cdc_connected() && (hal_tick() - last_print >= 2000)) {
            hal_usb_cdc_printf("dfu-test alive [%lu] — type 'dfu' to reboot\r\n", count++);
            last_print = hal_tick();
        }
    }
}
