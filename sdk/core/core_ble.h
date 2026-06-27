/**
 * core_ble.h — BLE for Core.ST.W5
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
 * Requires: Core.ST.W5 (STM32WBA55), BLE_ENABLED=1, clock >= "default" (HSE).
 *
 * @studio category ble label=Core.BLE icon=ᛒ
 *
 * @studio coverage
 *   id:    ble
 *   name:  BLE — Bluetooth Low Energy
 *   page:  /docs/sdk/ble
 *   blurb: Core.ST.W5-only API for BLE peripheral mode: advertise, define
 *          GATT services + characteristics, read/write/notify, and
 *          connect/disconnect callbacks. Currently Tier 1 only — there
 *          are no DSL bindings yet, and the API is C-callback heavy
 *          which doesn't translate cleanly to the current Studio
 *          host-call ABI.
 */

#ifndef CORE_BLE_H
#define CORE_BLE_H

#if !defined(STM32WBA55xx)
#error "core_ble.h: BLE is only available on Core.ST.W5 (STM32WBA55). This tile does not have a BLE radio."
#endif

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

/* Write callback type (called when central writes to a characteristic) */
typedef void (*core_ble_write_cb)(const uint8_t *data, uint16_t len, void *ctx);

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
 *
 * The UUID is auto-assigned SEQUENTIALLY by registration order
 * (0000B000-…, 0000B100-…, …). Convenient, but the UUIDs shift if you
 * reorder or insert services — so for a contract shared with client apps,
 * prefer core_ble_add_service_id() to pin a stable, order-independent UUID.
 *
 * @param name  Human-readable service name (for documentation/debugging).
 */
core_ble_svc_t core_ble_add_service(const char *name);

/**
 * Add a GATT service with an EXPLICIT 16-bit ID. The full UUID is
 * 0000<id>-8E22-4541-9D4C-21EDAE82ED19. Pinning the ID makes the GATT
 * contract independent of registration order — the recommended path when the
 * service map is a source of truth shared with phone / desktop client apps.
 *
 * @param name  Human-readable service name (for documentation/debugging).
 * @param id    16-bit identifier placed in the shared base UUID.
 */
core_ble_svc_t core_ble_add_service_id(const char *name, uint16_t id);

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
 * @param ctx       User context passed to on_write callback; may be NULL.
 * @return          Characteristic handle for set_value/notify.
 */
core_ble_char_t core_ble_add_char(core_ble_svc_t svc,
                                   const char *name,
                                   uint8_t access,
                                   uint8_t type,
                                   core_ble_write_cb on_write,
                                   void *ctx);

/**
 * Add a characteristic with an EXPLICIT 16-bit ID (0000<id>-8E22-…), the
 * order-independent counterpart to core_ble_add_char(). Use for the stable
 * contract shared with client apps.
 *
 * @param svc       Service handle.
 * @param name      Human-readable name (for documentation/debugging).
 * @param id        16-bit identifier placed in the shared base UUID.
 * @param access    CORE_BLE_READ / _WRITE / _NOTIFY (or combinations).
 * @param type      CORE_BLE_BOOL / _UINT8/16/32 / CORE_BLE_BYTES(n).
 * @param on_write  Write callback (NULL if not writable).
 * @param ctx       User context for on_write; may be NULL.
 */
core_ble_char_t core_ble_add_char_id(core_ble_svc_t svc,
                                     const char *name,
                                     uint16_t id,
                                     uint8_t access,
                                     uint8_t type,
                                     core_ble_write_cb on_write,
                                     void *ctx);

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

/** Set callback for connection events.
 *  @param cb   Called when a central connects; may be NULL
 *  @param ctx  User context passed to callback; may be NULL
 */
void core_ble_on_connect(void (*cb)(void *ctx), void *ctx);

/** Set callback for disconnection events.
 *  @param cb   Called when a central disconnects; may be NULL
 *  @param ctx  User context passed to callback; may be NULL
 */
void core_ble_on_disconnect(void (*cb)(void *ctx), void *ctx);

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

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @studio unsupported tier=2 value=H title="No DSL / Twin coverage for BLE"
//   The whole subsystem is escape-to-C. Notify, on-write, and
//   on-connect callbacks need a story that maps DSL handlers to the
//   underlying C function pointers — same pattern as Core.Pad rising/
//   falling events but with a payload struct.
//
// @studio unsupported tier=1 value=M title="Central / scanner mode"
//   Peripheral-only today. No scanning, no central-role connections,
//   no GATT-client reads/writes. Tracked as its own initiative — most
//   Bergsonne use-cases are peripheral-role (sensor advertising to a
//   phone). Apps that need central-role drop into the WBA BLE stack.
//
// @studio unsupported tier=1 value=M title="Bonding / persistent pairing"
//   core_ble_enable_pairing() runs Just Works each session — no
//   long-term key storage, so the host re-prompts on every connect.
//   Bonded reconnect needs IRK/LTK persistence in NVM.
//
// @studio unsupported tier=1 value=M title="Custom UUIDs"
//   Service + characteristic UUIDs are auto-generated from the name
//   string. No way to declare a fixed 128-bit UUID for interop with
//   existing apps that expect a specific service identifier.
//
// @studio unsupported tier=1 value=L title="Advanced features (LE Audio, extended adv, multi-link)"
//   The WBA radio supports LE Audio (LC3), extended advertising / 2M
//   PHY / coded PHY, and multi-link (multiple simultaneous connections).
//   None of that is exposed.

#endif /* CORE_BLE_H */
