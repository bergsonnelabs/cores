/**
 * core_ble.c — BLE advertising API for Core.W
 *
 * Wraps the low-level ble_app.c with a clean core_ API.
 * Handles sequencer pump, TX power mapping, and interval conversion.
 */

#include "core_ble.h"
#include <stdint.h>

/* Low-level BLE functions (sdk/ble/ble_app.c) */
extern void ble_app_init(void);
extern int  ble_app_advertise(const char *name);

/* Configurable parameters in ble_app.c */
extern uint8_t  ble_app_tx_power_code;
extern uint16_t ble_app_adv_interval_min;
extern uint16_t ble_app_adv_interval_max;

/* Sequencer (sdk/ble/stm32_seq.c) */
extern void UTIL_SEQ_Run(uint32_t mask);

/* Stop advertising (sdk/ble/ble_app.c — uses aci_gap_set_non_discoverable) */
extern int ble_app_stop_advertise(void);

/* ---- State ---- */

static uint8_t _ble_initialized;
static uint8_t _ble_seq_warmup_done;
static uint32_t _ble_seq_count;

/* ---- TX power mapping ---- */
/* BLE TX power codes for aci_hal_set_tx_power_level (En_High_Power=1):
 *   0x00 = -20 dBm,  0x08 = -14 dBm,  0x12 = -6 dBm,
 *   0x19 = -0.3 dBm, 0x1F = +10 dBm
 * We map 3 user-friendly levels to these. */
static const uint8_t _tx_power_codes[] = {
    0x00,   /* level 0: low   (~-20 dBm) */
    0x19,   /* level 1: medium (~0 dBm)  — default */
    0x1F,   /* level 2: high  (~+10 dBm) */
};

/* ---- API implementation ---- */

void core_ble_init(void)
{
    ble_app_init();
    _ble_initialized = 1;
    _ble_seq_warmup_done = 0;
    _ble_seq_count = 0;
}

int core_ble_advertise(const char *name)
{
    if (!_ble_initialized) return -1;

    /* Ensure sequencer has warmed up */
    while (!_ble_seq_warmup_done) {
        UTIL_SEQ_Run(~0UL);
        _ble_seq_count++;
        if (_ble_seq_count > 100) {
            _ble_seq_warmup_done = 1;
        }
    }

    return ble_app_advertise(name);
}

int core_ble_stop_advertise(void)
{
    if (!_ble_initialized) return -1;
    return ble_app_stop_advertise();
}

void core_ble_process(void)
{
    UTIL_SEQ_Run(~0UL);

    if (!_ble_seq_warmup_done) {
        _ble_seq_count++;
        if (_ble_seq_count > 100) {
            _ble_seq_warmup_done = 1;
        }
    }
}

void core_ble_set_tx_power(uint8_t level)
{
    if (level > 2) level = 2;
    ble_app_tx_power_code = _tx_power_codes[level];
}

void core_ble_set_adv_interval(uint16_t min_ms, uint16_t max_ms)
{
    /* BLE advertising interval units are 0.625 ms.
     * Convert ms to BLE units: units = ms / 0.625 = ms * 8 / 5 */
    if (min_ms < 20) min_ms = 20;
    if (max_ms < min_ms) max_ms = min_ms;
    if (max_ms > 10240) max_ms = 10240;

    ble_app_adv_interval_min = (uint16_t)((uint32_t)min_ms * 8 / 5);
    ble_app_adv_interval_max = (uint16_t)((uint32_t)max_ms * 8 / 5);
}

/* ---- Connection API ---- */

/* State and callbacks defined in ble_app_glue.c */
extern volatile uint8_t ble_connected;
extern void (*ble_on_connect_cb)(void);
extern void (*ble_on_disconnect_cb)(void);

int core_ble_connected(void)
{
    return ble_connected;
}

void core_ble_on_connect(void (*cb)(void))
{
    ble_on_connect_cb = cb;
}

void core_ble_on_disconnect(void (*cb)(void))
{
    ble_on_disconnect_cb = cb;
}
