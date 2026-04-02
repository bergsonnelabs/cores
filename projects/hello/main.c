/**
 * Hello World — blink + USB serial
 *
 * DFU firmware updates work automatically via make flash-dfu.
 */

#include "core.h"

int main(void)
{
    core_init();
    core_led_init();
    core_usb_init();

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);

//        if (core_usb_connected()) {
  //          core_usb_printf("hello!\r\n");
    //    }
    }
}
