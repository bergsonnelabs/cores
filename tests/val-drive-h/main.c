/**
 * val-drive-h — Compile-only validation for the Drive.H tile driver.
 *
 * Exercises every public API function in tile_drive_h.h to verify
 * compilation. Does not require hardware — all results are cast to void.
 *
 * Core.U.1 (Core-U-1-a), clock=max, I2C1 at 400 kHz
 */

#include "core.h"
#include "core_tiles.h"
#include "tile_drive_h.h"

int main(void)
{
    core_init();

    tiles_pal_t *hal = core_tiles_pal(&core_i2c1);
    tile_t haptic;

    /* ---- Lifecycle ---- */
    uint8_t found = tile_drive_h_find(hal, 0);
    (void)found;

    /* Init with defaults */
    tile_drive_h_init(hal, 0, &haptic, NULL);

    /* Init with explicit config */
    drive_h_cfg_t cfg = {
        .library     = 6,
        .closed_loop = 0,
    };
    tile_drive_h_init(hal, 0, &haptic, &cfg);

    /* ERM config variant */
    drive_h_cfg_t cfg_erm = {
        .library     = 2,
        .closed_loop = 1,
    };
    tile_drive_h_init(hal, 0, &haptic, &cfg_erm);

    /* ---- Single effect playback ---- */
    tile_drive_h_play(&haptic, 1, 1);
    tile_drive_h_play(&haptic, 47, 3);

    /* ---- Sequence playback ---- */
    const uint8_t seq[] = { 1, 12, 47 };
    tile_drive_h_play_sequence(&haptic, seq, 3);

    const uint8_t seq_full[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    tile_drive_h_play_sequence(&haptic, seq_full, 8);

    /* ---- Load sequence (no trigger) ---- */
    const uint8_t preload[] = { 10, 20 };
    tile_drive_h_load_sequence(&haptic, preload, 2);

    /* ---- Trigger modes ---- */
    tile_drive_h_set_trigger(&haptic, DRIVE_H_TRIG_EDGE);
    tile_drive_h_set_trigger(&haptic, DRIVE_H_TRIG_LEVEL);
    tile_drive_h_set_trigger(&haptic, DRIVE_H_TRIG_INTERNAL);

    /* ---- Playback status ---- */
    uint8_t playing = tile_drive_h_is_playing(&haptic);
    (void)playing;

    /* ---- Stop ---- */
    tile_drive_h_stop(&haptic);

    /* ---- RTP mode ---- */
    tile_drive_h_rtp_start(&haptic);
    tile_drive_h_rtp_write(&haptic, 0);
    tile_drive_h_rtp_write(&haptic, 64);
    tile_drive_h_rtp_write(&haptic, 127);
    tile_drive_h_rtp_stop(&haptic);

    /* ---- Status readback ---- */
    uint8_t status = tile_drive_h_get_status(&haptic);
    (void)status;

    /* ---- Diagnostics ---- */
    uint8_t diag = tile_drive_h_diagnose(&haptic);
    (void)diag;

    /* ---- Calibration ---- */
    uint8_t cal = tile_drive_h_calibrate(&haptic);
    (void)cal;

    /* ---- Battery voltage ---- */
    uint16_t vbat = tile_drive_h_get_vbat_mv(&haptic);
    (void)vbat;

    /* ---- Resonance frequency ---- */
    uint16_t res_hz = tile_drive_h_get_resonance_hz(&haptic);
    (void)res_hz;

    /* ---- Standby / wake ---- */
    tile_drive_h_standby(&haptic);
    tile_drive_h_wake(&haptic);

    /* ---- State checks ---- */
    uint8_t ready = tile_is_ready(&haptic);
    (void)ready;

    tile_state_t state = tile_state(&haptic);
    (void)state;

    while (1) {
        core_delay_ms(1000);
    }
}
