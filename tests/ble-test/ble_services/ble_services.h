#ifndef BLE_SERVICES_H
#define BLE_SERVICES_H

#include "core_ble.h"

extern core_ble_char_t ble_counter;
extern core_ble_char_t ble_led;

void app_ble_services(void);

#endif /* BLE_SERVICES_H */

/*
 * Write callback stubs — copy these into main.c and fill in your logic.
 * They are declared as extern in ble_services.c and called when a
 * BLE central writes to the corresponding characteristic.
 *
 * void on_aqua_led_write(const uint8_t *data, uint16_t len, void *ctx)
 * {
 *     (void)len; (void)ctx;
 *     // data[0..len-1] contains the value written by the central
 * }
 *
 */
