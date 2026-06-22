/**
 * core_ble.c — BLE API for Core.ST.W5
 *
 * Wraps the low-level ble_app.c and ble_svc.c with a clean core_ API.
 */

#include "core_ble.h"
#include <stdint.h>

/* Low-level BLE functions (sdk/ble/) */
extern void ble_app_init(void);
extern int  ble_app_advertise(const char *name);
extern int  ble_app_stop_advertise(void);
extern void ble_app_set_services_cb(void (*cb)(void));

extern void     ble_svc_init(void);
extern uint16_t ble_svc_add_service(const char *name, uint8_t num_chars);
extern uint16_t ble_svc_add_char(uint16_t svc_handle, const char *name,
                                  uint8_t access, uint8_t value_len,
                                  void (*on_write)(const uint8_t *data, uint16_t len, void *ctx),
                                  void *ctx);
extern int      ble_svc_set_value(uint16_t char_handle, const void *data, uint16_t len);
extern int      ble_svc_notify(uint16_t char_handle);

/* Configurable parameters in ble_app.c */
extern uint8_t  ble_app_tx_power_code;
extern uint16_t ble_app_adv_interval_min;
extern uint16_t ble_app_adv_interval_max;

/* Sequencer */
extern void UTIL_SEQ_Run(uint32_t mask);

/* Connection state (ble_app_glue.c) */
extern volatile uint8_t ble_connected;
extern void (*ble_on_connect_cb)(void *ctx);
extern void *ble_on_connect_ctx;
extern void (*ble_on_disconnect_cb)(void *ctx);
extern void *ble_on_disconnect_ctx;

/* ---- State ---- */

static uint8_t _ble_initialized;
static uint8_t _ble_seq_warmup_done;
static uint32_t _ble_seq_count;

/* User's service builder function */
static void (*_services_builder)(void);

/* TX power codes: 0=low, 1=medium, 2=high */
static const uint8_t _tx_power_codes[] = { 0x00, 0x19, 0x1F };

/* Service builder callback — called from ble_app_init after SVCCTL_Init */
static void _register_services(void)
{
    ble_svc_init();
    if (_services_builder) _services_builder();
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

void core_ble_set_services(void (*builder)(void))
{
    _services_builder = builder;
}

void core_ble_init(void)
{
    ble_app_set_services_cb(_register_services);
    ble_app_init();
    _ble_initialized = 1;
    _ble_seq_warmup_done = 0;
    _ble_seq_count = 0;
}

/* Saved name for re-advertising after disconnect */
static const char *_adv_name;

int core_ble_advertise(const char *name)
{
    if (!_ble_initialized) return -1;
    _adv_name = name;
    while (!_ble_seq_warmup_done) {
        UTIL_SEQ_Run(~0UL);
        _ble_seq_count++;
        if (_ble_seq_count > 100) _ble_seq_warmup_done = 1;
    }
    return ble_app_advertise(name);
}

int core_ble_stop_advertise(void)
{
    if (!_ble_initialized) return -1;
    return ble_app_stop_advertise();
}

/* Re-advertise flag (ble_app_glue.c) */
extern volatile uint8_t ble_need_readvertise;

void core_ble_process(void)
{
    UTIL_SEQ_Run(~0UL);
    if (!_ble_seq_warmup_done) {
        _ble_seq_count++;
        if (_ble_seq_count > 100) _ble_seq_warmup_done = 1;
    }

    /* Auto re-advertise after disconnect */
    if (ble_need_readvertise && _adv_name) {
        ble_need_readvertise = 0;
        ble_app_advertise(_adv_name);
    }
}

/* ============================================================
 * Service builder
 * ============================================================ */

core_ble_svc_t core_ble_add_service(const char *name)
{
    /* We pass 8 as max chars per service — generous default.
     * The BLE stack allocates attribute records from the pool
     * configured in BleStack_Init. */
    return ble_svc_add_service(name, 8);
}

core_ble_char_t core_ble_add_char(core_ble_svc_t svc,
                                   const char *name,
                                   uint8_t access,
                                   uint8_t type,
                                   core_ble_write_cb on_write,
                                   void *ctx)
{
    return ble_svc_add_char(svc, name, access, type, on_write, ctx);
}

/* ============================================================
 * Runtime
 * ============================================================ */

int core_ble_set_value(core_ble_char_t ch, const void *data, uint16_t len)
{
    return ble_svc_set_value(ch, data, len);
}

int core_ble_notify(core_ble_char_t ch)
{
    return ble_svc_notify(ch);
}

/* ============================================================
 * Connection
 * ============================================================ */

int core_ble_connected(void)      { return ble_connected; }
void core_ble_on_connect(void (*cb)(void *ctx), void *ctx)    { ble_on_connect_cb = cb; ble_on_connect_ctx = ctx; }
void core_ble_on_disconnect(void (*cb)(void *ctx), void *ctx) { ble_on_disconnect_cb = cb; ble_on_disconnect_ctx = ctx; }

/* ============================================================
 * Configuration
 * ============================================================ */

void core_ble_set_tx_power(uint8_t level)
{
    if (level > 2) level = 2;
    ble_app_tx_power_code = _tx_power_codes[level];
}

extern uint8_t ble_app_pairing_enabled;

void core_ble_enable_pairing(void)
{
    ble_app_pairing_enabled = 1;
}

void core_ble_set_adv_interval(uint16_t min_ms, uint16_t max_ms)
{
    if (min_ms < 20) min_ms = 20;
    if (max_ms < min_ms) max_ms = min_ms;
    if (max_ms > 10240) max_ms = 10240;
    ble_app_adv_interval_min = (uint16_t)((uint32_t)min_ms * 8 / 5);
    ble_app_adv_interval_max = (uint16_t)((uint32_t)max_ms * 8 / 5);
}
