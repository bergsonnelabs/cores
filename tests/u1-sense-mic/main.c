/**
 * Sense.MIC find test with Disp.RGBW visual feedback.
 *
 * Green  = Sense.MIC found on I2C3 (0x36)
 * Red    = Sense.MIC not found
 *
 * Also prints ADC readings over USB CDC when the mic is found.
 */
#include "core.h"
#include "core_tiles.h"
#include "core_usb.h"
#include "tile_display_rgbw.h"
#include "tile_sense_mic.h"

static tile_t led;
static tile_t mic;

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(3000);  /* Wait for USB host to enumerate */

    /* I2C bus scan — report all responding addresses */
    core_usb_printf("Scanning I2C1...\r\n");
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (core_tiles_pal(&core_i2c1)->i2c_is_ready(
                core_tiles_pal(&core_i2c1)->handle, a) == 0)
            core_usb_printf("  I2C1: 0x%02X ACK\r\n", a);
    }
    core_usb_printf("Scanning I2C3...\r\n");
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (core_tiles_pal(&core_i2c3)->i2c_is_ready(
                core_tiles_pal(&core_i2c3)->handle, a) == 0)
            core_usb_printf("  I2C3: 0x%02X ACK\r\n", a);
    }
    core_usb_printf("Scan done.\r\n");

    /* Init the RGBW display on I2C1 */
    tile_display_rgbw_init(core_tiles_pal(&core_i2c1), 0, &led, NULL);

    /* Probe for Sense.MIC on I2C3 — retry a few times with delay,
     * since the MAX11645 may need extra time after power-on */
    uint8_t mic_found = 0;
    for (int attempt = 0; attempt < 5; attempt++) {
        if (tile_sense_mic_find(core_tiles_pal(&core_i2c3), 0)) {
            core_usb_printf("Sense.MIC found on attempt %d\r\n", attempt);
            mic_found = 1;
            break;
        }
        core_usb_printf("Sense.MIC not found (attempt %d), retrying...\r\n", attempt);
        core_delay_ms(200);
    }
    if (mic_found) {
        core_usb_printf("Sense.MIC found at 0x36\r\n");

        /* Full init */
        tile_sense_mic_init(core_tiles_pal(&core_i2c3), 0, &mic, NULL);

        if (tile_is_ready(&mic)) {
            /* Green — found and initialized */
            tile_display_rgbw_set(&led, 0, 24, 0, 0);
            core_usb_printf("Sense.MIC initialized OK\r\n");

            /* Demo: read a few samples */
            for (int i = 0; i < 5; i++) {
                uint16_t raw = tile_sense_mic_get_raw(&mic);
                uint16_t mv  = tile_sense_mic_get_raw_mv(&mic);
                core_usb_printf("  sample %d: raw=%u  mv=%u\r\n", i, raw, mv);
                core_delay_ms(100);
            }

            /* Burst capture for sound level */
            uint16_t samples[128];
            tile_sense_mic_get_samples(&mic, samples, 128);
            uint16_t dc = tile_sense_mic_dc_level(&mic, samples, 128);
            uint16_t pp = tile_sense_mic_peak_to_peak(&mic, samples, 128);
            core_usb_printf("  DC level: %u  peak-to-peak: %u\r\n", dc, pp);
        } else {
            /* Yellow — found but init failed */
            tile_display_rgbw_set(&led, 24, 12, 0, 0);
            core_usb_printf("Sense.MIC found but init failed\r\n");
        }
    } else {
        /* Red — not found */
        tile_display_rgbw_set(&led, 24, 0, 0, 0);
        core_usb_printf("Sense.MIC NOT found on I2C3\r\n");
    }

    /* Main loop — continuously read and print */
    uint32_t tick = 0;
    while (1) {
        if (tile_is_ready(&mic)) {
            uint16_t raw = tile_sense_mic_get_raw(&mic);
            int16_t  ac  = tile_sense_mic_get_audio_sample(&mic);
            core_usb_printf("[%lu] raw=%u  ac=%d\r\n", tick, raw, ac);
        } else {
            if ((tick % 8) == 0)
                core_usb_printf("[%lu] mic not ready\r\n", tick);
        }
        tick++;
        core_delay_ms(250);
    }
}
