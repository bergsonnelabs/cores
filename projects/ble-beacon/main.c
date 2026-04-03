/**
 * BLE Beacon — minimal BLE advertising test
 *
 * Phase B1: stub main for build verification.
 * Will be fleshed out in B3/B4.
 */

#include "core.h"

int main(void)
{
    core_init();
    core_led_init();

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);
    }
}
