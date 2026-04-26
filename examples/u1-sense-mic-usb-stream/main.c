/**
 * Sense.MIC streaming demo — high-speed ADC streaming over USB CDC.
 *
 * Protocol:
 *   Host sends 's' → firmware starts streaming binary packets
 *   Host sends 'x' → firmware stops streaming
 *
 * Binary packet format (64 bytes = one USB bulk packet):
 *   [0xAA] [0x55] [30 × 16-bit little-endian samples]
 *
 * Rate messages (text, prefixed with '#'):
 *   # rate=18234 sps
 *
 * RGBW indicator:
 *   Green  = streaming
 *   Blue   = idle (mic ready, waiting for 's')
 *   Red    = mic not found
 */
#include "core.h"
#include "core_tiles.h"
#include "core_usb.h"
#include "tile_display_rgbw.h"
#include "tile_sense_mic.h"

/* Packet layout */
#define SYNC_0            0xAA
#define SYNC_1            0x55
#define SAMPLES_PER_PKT   30
#define PKT_SIZE          (2 + SAMPLES_PER_PKT * 2)  /* 62 bytes */

static tile_t led;
static tile_t mic;
static volatile uint8_t streaming = 0;

/* USB RX callback — runs from ISR, just sets a flag */
static void on_usb_rx(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] == 's') streaming = 1;
        if (data[i] == 'x') streaming = 0;
    }
}

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(500);

    /* Init RGBW on I2C1 */
    tile_display_rgbw_init(core_tiles_pal(&core_i2c1), 0, &led, NULL);

    /* Find and init Sense.MIC on I2C3 with retries */
    uint8_t mic_found = 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        if (tile_sense_mic_find(core_tiles_pal(&core_i2c3), 0)) {
            mic_found = 1;
            break;
        }
        core_delay_ms(100);
    }

    if (!mic_found) {
        tile_display_rgbw_set(&led, 24, 0, 0, 0);  /* Red */
        core_usb_printf("# error: Sense.MIC not found\r\n");
        while (1) core_delay_ms(1000);
    }

    tile_sense_mic_init(core_tiles_pal(&core_i2c3), 0, &mic, NULL);
    if (!tile_is_ready(&mic)) {
        tile_display_rgbw_set(&led, 24, 12, 0, 0);  /* Yellow */
        core_usb_printf("# error: Sense.MIC init failed\r\n");
        while (1) core_delay_ms(1000);
    }

    /* Report DC offset from calibration */
    core_usb_printf("# dc_offset=%u\r\n", tile_sense_mic_get_dc_offset(&mic));

    /* Set up USB RX callback for start/stop commands */
    core_usb_on_receive(on_usb_rx, NULL);

    /* Blue = idle, waiting for 's' */
    tile_display_rgbw_set(&led, 0, 0, 24, 0);
    core_usb_printf("# ready (send 's' to stream, 'x' to stop)\r\n");

    /* Streaming state */
    uint8_t pkt[PKT_SIZE];
    pkt[0] = SYNC_0;
    pkt[1] = SYNC_1;

    uint32_t sample_count = 0;
    uint32_t last_rate_tick = 0;
    uint32_t last_rate_count = 0;

    uint8_t was_streaming = 0;

    while (1) {
        if (!streaming) {
            if (was_streaming) {
                tile_display_rgbw_set(&led, 0, 0, 24, 0);  /* Blue = idle */
                core_usb_printf("# stopped\r\n");
                was_streaming = 0;
            }
            core_delay_ms(50);
            continue;
        }

        if (!was_streaming) {
            tile_display_rgbw_set(&led, 0, 24, 0, 0);  /* Green = streaming */
            was_streaming = 1;
        }

        /* Fill packet with samples */
        for (int i = 0; i < SAMPLES_PER_PKT; i++) {
            uint16_t raw = tile_sense_mic_get_raw(&mic);
            pkt[2 + i * 2]     = (uint8_t)(raw & 0xFF);        /* low byte */
            pkt[2 + i * 2 + 1] = (uint8_t)((raw >> 8) & 0xFF); /* high byte */
            sample_count++;
        }

        /* Send packet */
        core_usb_write(pkt, PKT_SIZE);

        /* Report sample rate every ~1 second */
        uint32_t now = core_millis();
        uint32_t elapsed = now - last_rate_tick;
        if (elapsed >= 1000) {
            uint32_t delta_samples = sample_count - last_rate_count;
            uint32_t rate = (delta_samples * 1000) / elapsed;
            core_usb_printf("# rate=%lu sps\r\n", rate);
            last_rate_tick = now;
            last_rate_count = sample_count;
        }

    }
}
