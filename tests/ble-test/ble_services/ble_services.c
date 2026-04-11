#include <stddef.h>
#include "core_ble.h"
#include "ble_services.h"

core_ble_char_t ble_counter;
core_ble_char_t ble_led;

extern void on_aqua_led_write(const uint8_t *data, uint16_t len, void *ctx);

void app_ble_services(void)
{
    core_ble_svc_t svc;

    svc = core_ble_add_service("aqua");
    ble_counter = core_ble_add_char(svc, "counter",
        CORE_BLE_READ | CORE_BLE_NOTIFY, CORE_BLE_UINT8, NULL, NULL);
    ble_led = core_ble_add_char(svc, "led",
        CORE_BLE_WRITE, CORE_BLE_UINT8, on_aqua_led_write, NULL);
}
