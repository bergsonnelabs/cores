/**
 * BLE LED + Counter Demo — Core.W
 *
 * Two services:
 * 1. LED Control: write 0x01/0x00 to toggle LED
 * 2. Counter: notifies a 1-byte counter every second
 */

#include "core.h"
#include "core_ble.h"

/* ---- BLE Services ---- */

static core_ble_char_t led_char;
static core_ble_char_t counter_char;

static void on_led_write(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)len; (void)ctx;
    if (data[0]) LED_ON();
    else         LED_OFF();
}

void app_ble_services(void)
{
    core_ble_svc_t svc;

    svc = core_ble_add_service("LED Control");
    led_char = core_ble_add_char(svc, "LED State",
                                  CORE_BLE_RW, CORE_BLE_BOOL,
                                  on_led_write, NULL);

    svc = core_ble_add_service("Counter");
    counter_char = core_ble_add_char(svc, "Count",
                                      CORE_BLE_READ | CORE_BLE_NOTIFY,
                                      CORE_BLE_UINT8, NULL, NULL);
}

/* ---- Main ---- */

int main(void)
{
    core_init();
    core_led_init();

    core_ble_set_services(app_ble_services);
    core_ble_enable_pairing();
    core_ble_init();
    core_ble_advertise("Core.W");

    uint8_t counter = 0;
    uint32_t last_notify = 0;

    while (1) {
        core_ble_process();

        /* Send counter notification every second while connected */
        if (core_ble_connected()) {
            uint32_t now = core_millis();
            if (now - last_notify >= 1000) {
                last_notify = now;
                counter++;
                core_ble_set_value(counter_char, &counter, 1);
            }
        }

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
