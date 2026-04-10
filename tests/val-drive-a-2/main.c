/**
 * val-drive-a-2 — Compile-only validation for the Drive.A.2 tile driver.
 *
 * Exercises every public API function in tile_drive_a_2.h to verify
 * compilation. Does not require hardware — all results are cast to void.
 *
 * Core.U.1 (Core-U-1-a), clock=max, I2C1 at 400 kHz
 */

#include "core.h"
#include "core_tiles.h"
#include "tile_drive_a_2.h"

int main(void)
{
    core_init();

    tiles_hal_t *hal = core_tiles_hal(&core_i2c1);
    tile_t dac;

    /* ---- Lifecycle ---- */
    uint8_t found = tile_drive_a_2_find(hal, 0);
    (void)found;

    /* Init with defaults */
    tile_drive_a_2_init(hal, 0, &dac, NULL);

    /* Init with explicit config */
    drive_a_2_cfg_t cfg = {
        .gain        = DRIVE_A_2_GAIN_1X_VDD,
        .amp_gain_db = 6,
    };
    tile_drive_a_2_init(hal, 0, &dac, &cfg);

    tile_drive_a_2_sleep(&dac);
    tile_drive_a_2_wake(&dac);
    tile_drive_a_2_reset(&dac);

    /* Re-init after reset */
    tile_drive_a_2_init(hal, 0, &dac, NULL);

    /* ---- DAC output ---- */
    tile_drive_a_2_set(&dac, 0, 2048);
    tile_drive_a_2_set(&dac, 1, 4095);

    tile_drive_a_2_set_mv(&dac, 0, 1650);
    tile_drive_a_2_set_mv(&dac, 1, 3000);

    uint16_t code = tile_drive_a_2_get(&dac, 0);
    (void)code;

    /* ---- DAC configuration ---- */
    tile_drive_a_2_set_gain(&dac, 0, DRIVE_A_2_GAIN_1X_EXT);
    tile_drive_a_2_set_gain(&dac, 0, DRIVE_A_2_GAIN_1X_VDD);
    tile_drive_a_2_set_gain(&dac, 0, DRIVE_A_2_GAIN_1P5X_INT);
    tile_drive_a_2_set_gain(&dac, 0, DRIVE_A_2_GAIN_2X_INT);
    tile_drive_a_2_set_gain(&dac, 1, DRIVE_A_2_GAIN_3X_INT);
    tile_drive_a_2_set_gain(&dac, 1, DRIVE_A_2_GAIN_4X_INT);

    /* ---- Waveform generation ---- */
    tile_drive_a_2_set_waveform(&dac, 0, DRIVE_A_2_WAVE_TRIANGLE);
    tile_drive_a_2_set_waveform(&dac, 0, DRIVE_A_2_WAVE_SAWTOOTH);
    tile_drive_a_2_set_waveform(&dac, 0, DRIVE_A_2_WAVE_INV_SAW);
    tile_drive_a_2_set_waveform(&dac, 1, DRIVE_A_2_WAVE_SINE);
    tile_drive_a_2_start_waveform(&dac, 0);
    tile_drive_a_2_start_waveform(&dac, 1);
    tile_drive_a_2_stop_waveform(&dac, 0);
    tile_drive_a_2_stop_waveform(&dac, 1);

    /* ---- Amplifier control ---- */
    tile_drive_a_2_amp_set_gain(&dac, 12);
    tile_drive_a_2_amp_set_gain(&dac, -10);
    tile_drive_a_2_amp_set_gain(&dac, 0);

    int8_t amp_gain = tile_drive_a_2_amp_get_gain(&dac);
    (void)amp_gain;

    tile_drive_a_2_amp_enable(&dac);
    tile_drive_a_2_amp_disable(&dac);

    drive_a_2_agc_cfg_t agc = {
        .compression   = DRIVE_A_2_COMP_4_1,
        .fixed_gain_db = 6,
        .max_gain_db   = 30,
        .limiter_level = 26,
        .attack        = 5,
        .release       = 11,
        .hold          = 0,
        .noise_gate    = 1,
    };
    tile_drive_a_2_amp_set_agc(&dac, &agc);

    uint8_t amp_status = tile_drive_a_2_amp_read_status(&dac);
    (void)amp_status;

    /* ---- Status ---- */
    uint16_t dac_status = tile_drive_a_2_read_status(&dac);
    (void)dac_status;

    /* ---- State checks ---- */
    uint8_t ready = tile_is_ready(&dac);
    (void)ready;

    tile_state_t state = tile_state(&dac);
    (void)state;

    while (1) {
        core_delay_ms(1000);
    }
}
