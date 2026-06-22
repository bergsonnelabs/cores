/**
 * USB CDC — Core.ST.H5 (STM32H523)
 *
 * Prints a heartbeat every second and echoes received data.
 */

#include "core.h"
#include "core_usb.h"

int main(void)
{
    core_init();
    core_usb_init();
    core_pad_output(9);

    uint32_t count = 0;
    uint32_t last = 0;

    while (1) {
        uint32_t now = _systick_ticks;

        if (now - last >= 1000) {
            last = now;
            core_pad_toggle(9);

            if (core_usb_connected()) {
                core_usb_printf("Core.ST.H5 USB CDC — tick %lu\r\n", ++count);
            }
        }

        uint8_t byte;
        while (core_usb_try_read(&byte)) {
            core_usb_write(&byte, 1);
        }
    }
}
