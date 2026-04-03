/**
 * BLE Beacon — minimal BLE advertising on Core.W
 *
 * Advertises as "TILETOWN" using the core_ble API.
 */

#include "core.h"
#include "core_ble.h"

int main(void)
{
    core_init();
    core_led_init();

    core_ble_init();
    core_ble_advertise("TILETOWN");

    while (1) {
        core_ble_process();

        /* Heartbeat LED — brief flash every 2s */
        static uint32_t last_on = 0;
        uint32_t now = core_millis();
        if (now - last_on >= 2000) {
            last_on = now;
            LED_ON();
        } else if (now - last_on >= 20) {
            LED_OFF();
        }
    }
}
