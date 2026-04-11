/**
 * val-drive-dc-h — Compile-only validation for the Drive.DC.H tile driver.
 *
 * Exercises every public API function in tile_drive_dc_h.h to verify
 * compilation. Does not require hardware — all results are cast to void.
 *
 * Core.U.1 (Core-U-1-a), clock=max, I2C1 at 400 kHz
 */

#include "core.h"
#include "core_tiles.h"
#include "tile_drive_dc_h.h"

int main(void)
{
    core_init();

    tiles_pal_t *hal = core_tiles_pal(&core_i2c1);
    tile_t motor;

    /* ---- Lifecycle ---- */
    uint8_t found = tile_drive_dc_h_find(hal, 0);
    (void)found;

    /* Init with defaults */
    tile_drive_dc_h_init(hal, 0, &motor, NULL);

    /* Init with explicit voltage-mode config */
    drive_dc_h_cfg_t cfg_v = {
        .mode    = DRIVE_DC_H_MODE_VOLTAGE,
        .vm_gain = 1,
        .cs_gain = 0,
        .target  = 180,
    };
    tile_drive_dc_h_init(hal, 0, &motor, &cfg_v);

    /* Init with speed-mode config (full motor params) */
    drive_dc_h_cfg_t cfg_s = {
        .mode            = DRIVE_DC_H_MODE_SPEED,
        .vm_gain         = 0,
        .cs_gain         = 2,
        .target          = 128,
        .motor_mohm      = 5000,
        .ripples_per_rev = 12,
        .kv_uv_per_rpm   = 417,
    };
    tile_drive_dc_h_init(hal, 0, &motor, &cfg_s);

    /* Init with ripple-count mode (position tracking) */
    drive_dc_h_cfg_t cfg_r = {
        .mode            = DRIVE_DC_H_MODE_RIPPLE_COUNT,
        .vm_gain         = 1,
        .cs_gain         = 3,
        .target          = 180,
        .motor_mohm      = 10000,
        .ripples_per_rev = 6,
    };
    tile_drive_dc_h_init(hal, 0, &motor, &cfg_r);

    /* ---- Motor control ---- */
    tile_drive_dc_h_forward(&motor);
    tile_drive_dc_h_reverse(&motor);
    tile_drive_dc_h_brake(&motor);
    tile_drive_dc_h_coast(&motor);

    /* ---- Regulation ---- */
    tile_drive_dc_h_set_target(&motor, 200);
    tile_drive_dc_h_set_target(&motor, 0);

    /* ---- Monitoring ---- */
    uint8_t fault = tile_drive_dc_h_get_fault(&motor);
    (void)fault;

    tile_drive_dc_h_clear_fault(&motor);

    uint8_t stalled = tile_drive_dc_h_is_stalled(&motor);
    (void)stalled;

    uint16_t mv = tile_drive_dc_h_get_voltage_mv(&motor);
    (void)mv;

    uint16_t ma = tile_drive_dc_h_get_current_ma(&motor);
    (void)ma;

    uint8_t speed = tile_drive_dc_h_get_speed(&motor);
    (void)speed;

    uint16_t ripples = tile_drive_dc_h_get_ripple_count(&motor);
    (void)ripples;

    tile_drive_dc_h_clear_ripple_count(&motor);

    /* ---- Power management ---- */
    tile_drive_dc_h_sleep(&motor);
    tile_drive_dc_h_wake(&motor);

    /* ---- State checks ---- */
    uint8_t ready = tile_is_ready(&motor);
    (void)ready;

    tile_state_t state = tile_state(&motor);
    (void)state;

    /* ---- Fault mask compile check ---- */
    (void)DRV8214_FAULT_FAULT;
    (void)DRV8214_FAULT_STALL;
    (void)DRV8214_FAULT_OCP;
    (void)DRV8214_FAULT_OVP;
    (void)DRV8214_FAULT_TSD;
    (void)DRV8214_FAULT_NPOR;
    (void)DRV8214_FAULT_CNT_DONE;

    while (1) {
        core_delay_ms(1000);
    }
}
