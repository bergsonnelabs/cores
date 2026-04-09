/**
 * val-spi-u2 -- Sense.I.6P6 via Kiln driver over SPI
 *
 * Core.U.2 (STM32L422) SPI1 → Sense.I.6P6 (ICM-42686P)
 *
 * Wiring:
 *   Core.U.2 Pad 2  (PA7)  SPI1.MOSI → Sense.I.6P6 Pad 5
 *   Core.U.2 Pad 10 (PA5)  SPI1.CLK  → Sense.I.6P6 Pad 4
 *   Core.U.2 Pad 18 (PB4)  SPI1.MISO → Sense.I.6P6 Pad 2
 *   Core.U.2 Pad 19 (PA4)  CS (GPIO) → Sense.I.6P6 Pad 3
 *
 * SPI Mode 0, 5 MHz (80/16). Debug output via USB CDC.
 *
 * This is the first Kiln tile driver test over SPI — exercises the
 * full core_ + tile_ protocol: tiles_hal_core → tiles_hal → driver.
 */

#include "core.h"
#include "core_usb.h"
#include "tile_handles.h"

/* Coregen handles (declared in tile_handles.h / core_init.c) */
extern hal_spi_t core_spi1;

#define dbg(...)  core_usb_printf(__VA_ARGS__)
#define imu       tile_sense_i_6p6_spi1_0

int main(void)
{
    core_init();
    core_led_init();

    /* Wait for USB host */
    LED_ON();
    while (!core_usb_connected())
        core_delay_ms(10);
    LED_OFF();
    core_delay_ms(500);

    dbg("\r\n=== val-spi-u2: Kiln driver over SPI ===\r\n");
    dbg("Core.U.2 SPI1 -> Sense.I.6P6 (ICM-42686P)\r\n");
    dbg("Mode 0, 5 MHz | MOSI=pad2 CLK=pad10 MISO=pad18 CS=pad19\r\n\r\n");

    /* ---- Bridge SPI1 to tile driver layer (from coregen recipe) ---- */
    tiles_hal_core_cfg_t hal_cfg = {
        .spi   = &core_spi1,
        .buses = TILES_BUS_SPI,
        .cs    = {
            [0] = { .port = (tiles_gpio_t *)GPIOA, .pin = 4 },  /* pad 19 = PA4 */
        },
    };
    tiles_hal_core_init(&core_hal_spi1, &hal_cfg);
    dbg("tiles_hal_core initialized (SPI, CS[0]=PA4)\r\n");

    /* ---- Initialize Sense.I.6P6 through the driver ---- */
    dbg("\r\n--- tile_sense_i_6p6_init ---\r\n");
    sense_i_6p6_cfg_t cfg = {0};  /* defaults: +/-8G, +/-1000dps, 100Hz, polled */
    tile_sense_i_6p6_init(&core_hal_spi1, 0, &imu, &cfg);

    dbg("  state = %d (2=READY, 4=ERROR)\r\n", imu.state);
    dbg("  flags = 0x%02X (WHO_AM_I)\r\n", imu.flags);

    if (!tile_is_ready(&imu)) {
        dbg("*** INIT FAILED ***\r\n");
        while (1) { LED_ON(); core_delay_ms(50); LED_OFF(); core_delay_ms(50); }
    }
    dbg("  READY\r\n");

    /* ---- Quick functional tests ---- */
    dbg("\r\n--- Functional tests ---\r\n");

    /* data_ready */
    core_delay_ms(20);
    uint8_t drdy = tile_sense_i_6p6_data_ready(&imu);
    dbg("  data_ready: %d\r\n", drdy);

    /* Single-axis reads */
    int16_t accel[3], gyro[3];
    tile_sense_i_6p6_get_raw_accels(&imu, accel);
    tile_sense_i_6p6_get_raw_gyros(&imu, gyro);
    dbg("  accel: (%d, %d, %d)\r\n", accel[0], accel[1], accel[2]);
    dbg("  gyro:  (%d, %d, %d)\r\n", gyro[0], gyro[1], gyro[2]);

    /* Temperature */
    int16_t temp_raw = tile_sense_i_6p6_get_temperature(&imu);
    int temp_c10 = (temp_raw * 100) / 1325 + 250;
    dbg("  temp:  %d.%dC (raw=%d)\r\n", temp_c10 / 10, temp_c10 % 10, temp_raw);

    /* 7-channel burst */
    int16_t all[7];
    tile_sense_i_6p6_get_raw_all(&imu, all);
    dbg("  all:   T=%d A=(%d,%d,%d) G=(%d,%d,%d)\r\n",
        all[0], all[1], all[2], all[3], all[4], all[5], all[6]);

    /* Sleep/wake cycle */
    dbg("\r\n--- Sleep/wake ---\r\n");
    tile_sense_i_6p6_sleep(&imu);
    dbg("  sleep: state=%d\r\n", imu.state);
    core_delay_ms(50);
    tile_sense_i_6p6_wake(&imu);
    dbg("  wake:  state=%d\r\n", imu.state);
    core_delay_ms(50);
    tile_sense_i_6p6_get_raw_accels(&imu, accel);
    dbg("  accel after wake: (%d, %d, %d)\r\n", accel[0], accel[1], accel[2]);

    /* ---- Continuous streaming ---- */
    dbg("\r\n--- Streaming (get_raw_all) ---\r\n\r\n");
    for (int sample = 0; ; sample++) {
        if (tile_sense_i_6p6_data_ready(&imu)) {
            tile_sense_i_6p6_get_raw_all(&imu, all);

            int tc = (all[0] * 100) / 1325 + 250;

            if (sample % 10 == 0) {
                dbg("[%4d] T=%d.%dC  A=(%6d,%6d,%6d)  G=(%6d,%6d,%6d)\r\n",
                    sample, tc / 10, tc % 10,
                    all[1], all[2], all[3],
                    all[4], all[5], all[6]);
            }
            LED_TOGGLE();
        }
        core_delay_ms(10);
    }
}
