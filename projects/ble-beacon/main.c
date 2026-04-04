/**
 * BLE LED Demo — control LED via BLE on Core.W
 *
 * Demonstrates the core_ble service builder API.
 * Connect with nRF Connect and write to the LED characteristic.
 */

#include "core.h"
#include "core_ble.h"

/* ---- BLE Services ---- */

static core_ble_char_t led_char;

static void on_led_write(const uint8_t *data, uint16_t len)
{
    (void)len;
    if (data[0]) LED_ON();
    else         LED_OFF();
}

void app_ble_services(void)
{
    core_ble_svc_t svc;

    svc = core_ble_add_service("LED Control");
    led_char = core_ble_add_char(svc, "LED State",
                                  CORE_BLE_RW, CORE_BLE_BOOL,
                                  on_led_write);
}

/* ---- Main ---- */

int main(void)
{
    core_init();
    core_led_init();

    core_ble_set_services(app_ble_services);
    core_ble_init();
    core_ble_advertise("Core.W");

    while (1) {
        core_ble_process();

        /* Heartbeat when not connected */
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
