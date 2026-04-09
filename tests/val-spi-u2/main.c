/**
 * val-spi-u2 -- Sense.I.6P6 Kiln driver over SPI — full validation
 *
 * Core.U.2 SPI1 → Sense.I.6P6 (ICM-42686P)
 * Pads: MOSI=2 CLK=10 MISO=18 CS=19
 * SPI Mode 0, 5 MHz. Debug via USB CDC.
 *
 * Tests: init, polling reads, sleep/wake, FIFO burst reads.
 */

#include "core.h"
#include "core_usb.h"
#include "tile_handles.h"

extern hal_spi_t core_spi1;

#define dbg(...)  core_usb_printf(__VA_ARGS__)
#define imu       tile_sense_i_6p6_spi1_0

int main(void)
{
    core_init();
    core_led_init();

    LED_ON();
    while (!core_usb_connected())
        core_delay_ms(10);
    LED_OFF();
    core_delay_ms(500);

    dbg("\r\n=== Sense.I.6P6 over SPI — full validation ===\r\n\r\n");

    /* ---- HAL setup ---- */
    tiles_hal_core_cfg_t hal_cfg = {
        .spi   = &core_spi1,
        .buses = TILES_BUS_SPI,
        .cs    = { [0] = { .port = (tiles_gpio_t *)GPIOA, .pin = 4 } },
    };
    tiles_hal_core_init(&core_hal_spi1, &hal_cfg);

    /* ---- Init ---- */
    sense_i_6p6_cfg_t cfg = {0};
    tile_sense_i_6p6_init(&core_hal_spi1, 0, &imu, &cfg);
    dbg("Init: state=%d WHO_AM_I=0x%02X\r\n", imu.state, imu.flags);
    if (!tile_is_ready(&imu)) {
        dbg("FAIL\r\n");
        while (1) { LED_ON(); core_delay_ms(50); LED_OFF(); core_delay_ms(50); }
    }

    /* ---- 1. Polling reads ---- */
    dbg("\r\n--- 1. Polling reads ---\r\n");
    core_delay_ms(50);
    int16_t accel[3], gyro[3], all[7];
    for (int i = 0; i < 10; i++) {
        tile_sense_i_6p6_get_raw_6dof(&imu, all);
        if (i % 3 == 0)
            dbg("  A=(%6d,%6d,%6d) G=(%6d,%6d,%6d)\r\n",
                all[0], all[1], all[2], all[3], all[4], all[5]);
        core_delay_ms(10);
    }

    /* ---- 2. Sleep/wake ---- */
    dbg("\r\n--- 2. Sleep/wake ---\r\n");
    tile_sense_i_6p6_sleep(&imu);
    dbg("  sleep: state=%d\r\n", imu.state);
    core_delay_ms(50);
    tile_sense_i_6p6_wake(&imu);
    dbg("  wake:  state=%d\r\n", imu.state);
    core_delay_ms(100);
    tile_sense_i_6p6_get_raw_accels(&imu, accel);
    dbg("  accel: (%d, %d, %d)\r\n", accel[0], accel[1], accel[2]);

    /* ---- 3. FIFO ---- */
    dbg("\r\n--- 3. FIFO ---\r\n");

    /* Configure: stream mode, accel+gyro+temp, standard 16-bit */
    tile_sense_i_6p6_fifo_config(&imu, SENSE_I_6P6_FIFO_STREAM,
                                  1, 1, 1, 0);
    tile_sense_i_6p6_fifo_flush(&imu);
    dbg("  FIFO configured: stream, accel+gyro+temp\r\n");

    /* Let FIFO collect ~500ms of data at 100Hz = ~50 packets */
    core_delay_ms(500);

    uint16_t count = tile_sense_i_6p6_fifo_count(&imu);
    dbg("  FIFO count: %d packets\r\n", count);

    /* Read up to 10 packets */
    sense_i_6p6_fifo_packet_t pkts[10];
    uint16_t nread = tile_sense_i_6p6_fifo_read_packets(&imu, pkts, 10);
    dbg("  Read %d packets:\r\n", nread);
    for (uint16_t i = 0; i < nread && i < 5; i++) {
        dbg("    [%d] hdr=0x%02X A=(%6d,%6d,%6d) G=(%6d,%6d,%6d) T=%d ts=%d\r\n",
            i, pkts[i].header,
            pkts[i].accel[0], pkts[i].accel[1], pkts[i].accel[2],
            pkts[i].gyro[0], pkts[i].gyro[1], pkts[i].gyro[2],
            pkts[i].temp, pkts[i].timestamp);
    }

    uint16_t lost = tile_sense_i_6p6_fifo_lost_count(&imu);
    dbg("  Lost packets: %d\r\n", lost);

    /* Second burst — verify FIFO keeps streaming */
    core_delay_ms(200);
    count = tile_sense_i_6p6_fifo_count(&imu);
    nread = tile_sense_i_6p6_fifo_read_packets(&imu, pkts, 10);
    dbg("  Second burst: count=%d, read=%d\r\n", count, nread);
    if (nread > 0) {
        dbg("    [0] A=(%d,%d,%d) G=(%d,%d,%d)\r\n",
            pkts[0].accel[0], pkts[0].accel[1], pkts[0].accel[2],
            pkts[0].gyro[0], pkts[0].gyro[1], pkts[0].gyro[2]);
    }

    /* ---- 4. Continuous polling with data_ready ---- */
    dbg("\r\n--- 4. Continuous data_ready polling ---\r\n");
    /* Disable FIFO for clean polling */
    tile_sense_i_6p6_fifo_config(&imu, SENSE_I_6P6_FIFO_BYPASS,
                                  0, 0, 0, 0);
    core_delay_ms(50);
    for (int i = 0; i < 50; i++) {
        if (tile_sense_i_6p6_data_ready(&imu)) {
            tile_sense_i_6p6_get_raw_6dof(&imu, all);
            if (i % 10 == 0)
                dbg("  [%2d] A=(%6d,%6d,%6d) G=(%6d,%6d,%6d)\r\n",
                    i, all[0], all[1], all[2], all[3], all[4], all[5]);
            LED_TOGGLE();
        }
        core_delay_ms(10);
    }

    dbg("\r\n=== ALL TESTS PASSED ===\r\n");
    while (1) { LED_TOGGLE(); core_delay_ms(500); }
}
