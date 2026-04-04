/**
 * core_ble.h — BLE for Core.W
 *
 * Simple BLE API: advertise, connect, define GATT services.
 * The BLE stack, sequencer, and radio are managed internally.
 *
 * Usage:
 *   core_ble_set_services(my_services);     // register service builder
 *   core_ble_init();
 *   core_ble_advertise("MY-DEVICE");
 *   while (1) { core_ble_process(); }
 *
 * Requires: Core.W (STM32WBA55), BLE_ENABLED=1, clock >= "default" (HSE).
 */

#ifndef CORE_BLE_H
#define CORE_BLE_H

#include <stdint.h>

/* ============================================================
 * Value types for core_ble_add_char
 * ============================================================ */

#define CORE_BLE_BOOL      1
#define CORE_BLE_UINT8     1
#define CORE_BLE_INT8      1
#define CORE_BLE_UINT16    2
#define CORE_BLE_INT16     2
#define CORE_BLE_UINT32    4
#define CORE_BLE_INT32     4
#define CORE_BLE_BYTES(n)  (n)

/* ============================================================
 * Access modes for core_ble_add_char
 * ============================================================ */

#define CORE_BLE_READ    0x02
#define CORE_BLE_WRITE   0x08
#define CORE_BLE_NOTIFY  0x10
#define CORE_BLE_RW      (CORE_BLE_READ | CORE_BLE_WRITE)

/* ============================================================
 * Handles
 * ============================================================ */

typedef uint16_t core_ble_svc_t;
typedef uint16_t core_ble_char_t;

/* Write callback type */
typedef void (*core_ble_write_cb)(const uint8_t *data, uint16_t len);

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * Register a service builder function.
 * Called before core_ble_init(). The builder is invoked during init
 * after the GATT server is ready.
 */
void core_ble_set_services(void (*builder)(void));

/** Initialize the BLE stack. Call once after core_init(). */
void core_ble_init(void);

/** Start advertising. Call after init + at least one core_ble_process(). */
int core_ble_advertise(const char *name);

/** Stop advertising. */
int core_ble_stop_advertise(void);

/** Process BLE events. Call continuously from main loop. */
void core_ble_process(void);

/* ============================================================
 * Service builder — call from your services function
 * ============================================================ */

/**
 * Add a GATT service. Returns a handle for adding characteristics.
 * UUID is auto-generated from the service name.
 *
 * @param name  Human-readable service name (for documentation/debugging).
 */
core_ble_svc_t core_ble_add_service(const char *name);

/**
 * Add a characteristic to a service.
 *
 * @param svc       Service handle from core_ble_add_service.
 * @param name      Human-readable name (for documentation/debugging).
 * @param access    Access mode: CORE_BLE_READ, CORE_BLE_WRITE, CORE_BLE_NOTIFY,
 *                  or combinations (CORE_BLE_RW, CORE_BLE_READ | CORE_BLE_NOTIFY).
 * @param type      Value type: CORE_BLE_BOOL, CORE_BLE_UINT8, CORE_BLE_UINT16,
 *                  CORE_BLE_UINT32, or CORE_BLE_BYTES(n) for raw buffers.
 * @param on_write  Callback when central writes. NULL if read-only.
 * @return          Characteristic handle for set_value/notify.
 */
core_ble_char_t core_ble_add_char(core_ble_svc_t svc,
                                   const char *name,
                                   uint8_t access,
                                   uint8_t type,
                                   core_ble_write_cb on_write);

/* ============================================================
 * Runtime — read/write/notify
 * ============================================================ */

/**
 * Update a characteristic's value. For readable characteristics,
 * this is what the central will read. For notify characteristics,
 * call core_ble_notify() after to push the update.
 */
int core_ble_set_value(core_ble_char_t ch, const void *data, uint16_t len);

/**
 * Send a notification to the connected central.
 * The central must have enabled notifications (CCCD) for this to work.
 * Sends the current value set by core_ble_set_value().
 */
int core_ble_notify(core_ble_char_t ch);

/* ============================================================
 * Connection
 * ============================================================ */

/** Returns 1 if a central is connected. */
int core_ble_connected(void);

/** Set callback for connection events. */
void core_ble_on_connect(void (*cb)(void));

/** Set callback for disconnection events. */
void core_ble_on_disconnect(void (*cb)(void));

/* ============================================================
 * Configuration (call before core_ble_init)
 * ============================================================ */

/** TX power: 0=low(-20dBm), 1=medium(0dBm), 2=high(+10dBm). Default: 1. */
void core_ble_set_tx_power(uint8_t level);

/** Advertising interval in ms (20-10240). Default: 100/150. */
void core_ble_set_adv_interval(uint16_t min_ms, uint16_t max_ms);

/**
 * Enable OS-level pairing (Just Works).
 * When enabled, the phone/computer will show a "Pair?" dialog.
 * Once paired, the device auto-reconnects and appears in OS
 * Bluetooth settings. Call before core_ble_init(). Default: disabled.
 */
void core_ble_enable_pairing(void);

#endif /* CORE_BLE_H */
