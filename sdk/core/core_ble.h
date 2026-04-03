/**
 * core_ble.h — BLE advertising for Core.W
 *
 * Simple BLE API for advertising a device name. The BLE stack,
 * sequencer, and radio are managed internally — no ST HAL or
 * RTOS required.
 *
 * Usage:
 *   core_init();
 *   core_ble_init();
 *   core_ble_advertise("MY-DEVICE");
 *   while (1) { core_ble_process(); }
 *
 * Requires: Core.W (STM32WBA55), BLE_ENABLED=1, clock >= "default" (HSE).
 */

#ifndef CORE_BLE_H
#define CORE_BLE_H

#include <stdint.h>

/**
 * Initialize the BLE stack.
 * Call once after core_init(). Handles HSE tuning, radio clocks,
 * link layer, GAP, GATT, and sequencer setup internally.
 * BD address is derived automatically from the device's unique ID.
 */
void core_ble_init(void);

/**
 * Start BLE advertising with the given device name.
 * Name appears in BLE scanner apps (nRF Connect, LightBlue, etc.).
 *
 * @param name  Device name, up to 20 characters.
 * @return      0 on success, -1 on failure.
 *
 * Must be called after core_ble_init() and at least one core_ble_process().
 */
int core_ble_advertise(const char *name);

/**
 * Stop BLE advertising.
 * @return  0 on success, -1 on failure.
 */
int core_ble_stop_advertise(void);

/**
 * Process BLE events.
 * Call this regularly from your main loop. All BLE stack processing
 * (radio events, sequencer tasks, HCI commands) happens here.
 */
void core_ble_process(void);

/**
 * Set TX power level before advertising.
 *
 * @param level  0 = low (~-20 dBm), 1 = medium (~0 dBm), 2 = high (~+6 dBm)
 *               Default: 1.
 *
 * Call before core_ble_advertise(). Takes effect on next advertise start.
 */
void core_ble_set_tx_power(uint8_t level);

/**
 * Set advertising interval before advertising.
 *
 * @param min_ms  Minimum interval in milliseconds (20–10240). Default: 100.
 * @param max_ms  Maximum interval in milliseconds (20–10240). Default: 150.
 *
 * Shorter intervals = faster discovery, more power consumption.
 * Call before core_ble_advertise(). Takes effect on next advertise start.
 */
void core_ble_set_adv_interval(uint16_t min_ms, uint16_t max_ms);

/**
 * Check if a BLE central is connected.
 * @return  1 if connected, 0 if not.
 */
int core_ble_connected(void);

/**
 * Set callback for connection events.
 * Called from BLE stack context when a central connects.
 */
void core_ble_on_connect(void (*cb)(void));

/**
 * Set callback for disconnection events.
 * Called from BLE stack context when the central disconnects.
 */
void core_ble_on_disconnect(void (*cb)(void));

#endif /* CORE_BLE_H */
