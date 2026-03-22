/**
 * USB Serial — CDC virtual serial port demo
 *
 * Echoes received data back to the host and periodically sends
 * a status message with uptime. Uses the hal_usb_cdc driver for
 * all USB enumeration and data transfer.
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"
#include "hal_usb_cdc.h"

static uint32_t last_print_ms = 0;

void on_rx(const uint8_t *data, uint16_t len)
{
    hal_usb_cdc_write(data, len);  /* Echo back */
}

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    hal_usb_cdc_init();
    hal_usb_cdc_set_rx_callback(on_rx);

    while (1) {
        if (hal_usb_cdc_connected()) {
            uint32_t now = hal_tick();
            if ((now - last_print_ms) >= 5000) {
                last_print_ms = now;
                hal_usb_cdc_printf("Hello from Core.U.2! Uptime: %lu ms\r\n",
                                   (unsigned long)now);
                LED_TOGGLE();
            }
        }
    }
}
