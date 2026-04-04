/**
 * BLE Beacon — advertising + connection on Core.W
 *
 * Advertises as "TILETOWN". LED stays on while connected.
 * Heartbeat blink when not connected.
 */

#include "core.h"
#include "core_ble.h"

static void on_connect(void)  { LED_ON(); }
static void on_disconnect(void) { LED_OFF(); }

int main(void)
{
    core_init();
    core_led_init();

    core_ble_init();
    core_ble_on_connect(on_connect);
    core_ble_on_disconnect(on_disconnect);
    core_ble_advertise("TILETOWN");

    while (1) {
        core_ble_process();

        /* Heartbeat when not connected — 10ms flash every 3s */
        if (!core_ble_connected()) {
            static uint32_t last_on = 0;
            uint32_t now = core_millis();
            if (now - last_on >= 3000) {
                last_on = now;
                LED_ON();
            } else if (now - last_on >= 10) {
                LED_OFF();
            }
        }
    }
}
