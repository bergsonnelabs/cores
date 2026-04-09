/**
 * val-sense-mic — Compile-only validation for the Sense.MIC tile driver.
 *
 * Exercises every public API function in tile_sense_mic.h to verify
 * compilation. Does not require hardware — all results are cast to void.
 *
 * Core.U.1 (Core-U-1-a), clock=max, I2C1 at 400 kHz
 */

#include "core.h"
#include "core_tiles.h"
#include "tile_sense_mic.h"

int main(void)
{
    core_init();

    tiles_hal_t *hal = core_tiles_hal(&core_i2c1);
    tile_t mic;

    /* Lifecycle */
    uint8_t found = tile_sense_mic_find(hal, 0);
    (void)found;

    tile_sense_mic_init(hal, 0, &mic, NULL);

    /* Init with config */
    sense_mic_cfg_t cfg = {
        .ref     = SENSE_MIC_REF_INTERNAL,
        .channel = SENSE_MIC_CH_AIN0,
        .scan    = SENSE_MIC_SCAN_SINGLE,
        .vref_mv = 2048,
    };
    tile_sense_mic_init(hal, 0, &mic, &cfg);

    tile_sense_mic_sleep(&mic);
    tile_sense_mic_wake(&mic);
    tile_sense_mic_reset(&mic);

    /* Re-init after reset */
    tile_sense_mic_init(hal, 0, &mic, NULL);

    /* Configuration */
    tile_sense_mic_set_reference(&mic, SENSE_MIC_REF_VDD);
    tile_sense_mic_set_channel(&mic, SENSE_MIC_CH_AIN1);
    tile_sense_mic_set_scan_mode(&mic, SENSE_MIC_SCAN_8X);

    uint16_t vref = tile_sense_mic_get_vref_mv(&mic);
    (void)vref;

    /* Data reads */
    uint16_t raw = tile_sense_mic_get_raw(&mic);
    (void)raw;

    uint16_t mv = tile_sense_mic_get_raw_mv(&mic);
    (void)mv;

    int16_t ac = tile_sense_mic_get_audio_sample(&mic);
    (void)ac;

    uint16_t dc = tile_sense_mic_get_dc_offset(&mic);
    (void)dc;

    tile_sense_mic_calibrate(&mic);

    uint16_t samples[64];
    tile_sense_mic_get_samples(&mic, samples, 64);

    /* Audio analysis utilities */
    uint16_t level = tile_sense_mic_dc_level(samples, 64);
    (void)level;

    uint16_t pp = tile_sense_mic_peak_to_peak(samples, 64);
    (void)pp;

    uint16_t rms = tile_sense_mic_rms(samples, 64, 750);
    (void)rms;

    uint16_t amp_mv = tile_sense_mic_amplitude_mv(&mic, pp);
    (void)amp_mv;

    /* State checks */
    uint8_t ready = tile_is_ready(&mic);
    (void)ready;

    tile_state_t state = tile_state(&mic);
    (void)state;

    while (1) {
        core_delay_ms(1000);
    }
}
