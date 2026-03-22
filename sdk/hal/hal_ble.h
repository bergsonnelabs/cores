/**
 * hal_ble.h — BLE HAL wrapper for STM32WBA55
 *
 * Simple API that wraps the ST BLE stack binary for basic
 * advertising, connection, and notification support.
 */

#ifndef HAL_BLE_H
#define HAL_BLE_H

#include <stdint.h>
#include "hal_common.h"

void hal_ble_init(void);
void hal_ble_process(void);
hal_status_t hal_ble_advertise(const char *name);
hal_status_t hal_ble_stop_advertise(void);
int hal_ble_connected(void);

typedef void (*hal_ble_connect_cb_t)(uint16_t conn_handle);
typedef void (*hal_ble_disconnect_cb_t)(uint16_t conn_handle, uint8_t reason);
typedef void (*hal_ble_rx_cb_t)(uint16_t conn_handle, const uint8_t *data, uint16_t len);

void hal_ble_on_connect(hal_ble_connect_cb_t cb);
void hal_ble_on_disconnect(hal_ble_disconnect_cb_t cb);
hal_status_t hal_ble_notify(uint16_t conn_handle, uint16_t char_handle,
                            const uint8_t *data, uint16_t len);

#endif /* HAL_BLE_H */
