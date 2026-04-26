/**
 * Blink — LED blink demo
 *
 * Simplest possible project. Blinks the onboard LED at 5 Hz.
 */

#include "core.h"
#include "core_usb.h"

int main(void)
{
    core_init();
    core_usb_init();
    core_led_init();

    while (1) {
        LED_TOGGLE();
        core_delay_ms(100);
    }
}
