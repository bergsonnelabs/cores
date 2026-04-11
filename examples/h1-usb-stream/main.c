/**
 * USB Raw Streaming — binary data output with start/stop control
 *
 * Streams raw binary packets over USB CDC. The host controls
 * streaming by sending 's' to start and 'x' to stop.
 *
 * Each packet is a small struct:
 *   [0:3]  uint32_t  sequence number (little-endian)
 *   [4:5]  uint16_t  sample value (simulated ramp)
 *
 * On the host, use stream_read.py (in this directory).
 *
 * Expected throughput: ~800 KB/s (Full-Speed USB bulk limit).
 * For higher throughput, increase PACKET_SIZE up to 512 bytes
 * (must be a multiple of 64 for best USB efficiency).
 */

#include "core.h"
#include "core_usb.h"

/* Packet layout — keep small for this demo.
 * For maximum throughput, pack more samples per packet
 * (up to 512 bytes, must be a multiple of 64 for best efficiency). */
typedef struct __attribute__((packed)) {
    uint32_t seq;
    uint16_t sample;
} stream_packet_t;

static stream_packet_t packet;
static volatile uint8_t streaming = 0;

/* RX callback — runs in USB ISR, checks for start/stop commands */
static void on_rx(const uint8_t *data, uint16_t len, void *ctx)
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
    core_pad_output(9);

    core_usb_on_receive(on_rx, NULL);

    uint32_t seq = 0;
    uint16_t sample = 0;

    while (1) {
        if (!core_usb_connected()) {
            streaming = 0;
            core_pad_toggle(9);
            core_delay_ms(500);
            continue;
        }

        if (!streaming) {
            core_pad_write(9, 1);   /* LED on = idle, waiting for 's' */
            core_delay_ms(10);
            continue;
        }

        /* Build packet */
        packet.seq = seq++;
        packet.sample = sample++;   /* Simulated ramp — replace with ADC reads */

        core_usb_write((const uint8_t *)&packet, sizeof(packet));

        if ((seq & 0xFFF) == 0)
            core_pad_toggle(9);
    }
}
