/**
 * u1-sense-tof-test -- Hardware validation for the Sense.TOF tile driver.
 *
 * Reads distance from the TMF8806 on I2C3 and prints formatted output
 * over USB CDC. A Disp.RGBW on I2C1 provides visual status:
 *
 *   Green  = sensor found and reading
 *   Blue   = distance data ready (flickers each read)
 *   Red    = sensor init failed
 *   Yellow = starting up
 *
 * Core.U.1, custom bootloader -- flash with: make flash-dfu
 */

#include "core.h"
#include "core_i2c.h"
#include "core_usb.h"
#include "core_tiles.h"
#include "tile_disp_rgbw.h"
#include "tile_sense_tof.h"

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(200);

    /* ---- Init Disp.RGBW on I2C1 ---- */

    tile_t led;
    tile_disp_rgbw_init(core_tiles_hal(&core_i2c1), 0, &led);
    tile_disp_rgbw_set(&led, 16, 16, 0, 0);  /* Yellow: starting */

    /* Wait for USB host to connect */
    core_delay_ms(1500);

    /* ---- Init Sense.TOF on I2C3 ---- */

    tiles_hal_t *hal3 = core_tiles_hal(&core_i2c3);
    tile_t tof;

    core_usb_printf("\r\n--- Sense.TOF (TMF8806) hardware test ---\r\n");

    /* Scan I2C3 to report what's there */
    uint8_t addrs[16];
    uint8_t scan_count = 0;
    core_i2c_scan(&core_i2c3, addrs, &scan_count, sizeof(addrs));
    core_usb_printf("I2C3 scan: %d device(s) found\r\n", scan_count);
    for (uint8_t i = 0; i < scan_count; i++)
        core_usb_printf("  0x%02X\r\n", addrs[i]);

    /* Find */
    uint8_t found = tile_sense_tof_find(hal3, 0);
    core_usb_printf("find(instance 0): %s\r\n", found ? "OK" : "NOT FOUND");

    if (!found) {
        core_usb_printf("ERROR: Sense.TOF not detected at 0x41\r\n");
        tile_disp_rgbw_set(&led, 24, 0, 0, 0);  /* Red: error */

        while (1) {
            core_delay_ms(1000);
            core_usb_printf("ERROR: Sense.TOF not responding\r\n");
        }
    }

    /* Init with defaults: 2.5 m mode, 30 ms period, 900k iterations */
    sense_tof_cfg_t cfg = {
        .mode       = SENSE_TOF_RANGE_2500MM,
        .period_ms  = 30,
        .kilo_iters = 900,
        .threshold  = 6,
    };
    tile_sense_tof_init(hal3, 0, &tof, &cfg);

    if (tile_is_ready(&tof)) {
        /* Print app version */
        sense_tof_version_t ver;
        tile_sense_tof_get_app_version(&tof, &ver);
        core_usb_printf("init: OK (addr 0x%02X)\r\n", tof.id);
        core_usb_printf("app version: %d.%d.%d\r\n", ver.major, ver.minor, ver.patch);
        tile_disp_rgbw_set(&led, 0, 24, 0, 0);  /* Green: ready */
    } else {
        core_usb_printf("init: FAILED\r\n");
        tile_disp_rgbw_set(&led, 24, 0, 0, 0);  /* Red: error */

        while (1) {
            core_delay_ms(1000);
            core_usb_printf("ERROR: Sense.TOF init failed\r\n");
        }
    }

    /* Start continuous measurement */
    tile_sense_tof_start(&tof);
    core_usb_printf("\r\nReading distance (continuous, 2.5 m mode)...\r\n\r\n");

    /* ---- Main loop ---- */

    uint32_t sample = 0;
    uint8_t toggle = 0;

    while (1) {
        /* Wait for result */
        uint16_t timeout = 500;
        while (!tile_sense_tof_result_ready(&tof) && timeout--)
            core_delay_ms(1);

        sense_tof_result_t res;
        tile_sense_tof_get_result(&tof, &res);

        core_usb_printf("[%04lu] dist: %5u mm  rel: %2u  temp: %d C\r\n",
                        (unsigned long)(sample++),
                        (unsigned)res.distance_mm,
                        (unsigned)res.reliability,
                        (int)res.temperature);

        /* Alternate green/blue to show activity */
        if (toggle) {
            tile_disp_rgbw_set(&led, 0, 16, 0, 0);
        } else {
            tile_disp_rgbw_set(&led, 0, 0, 16, 0);
        }
        toggle ^= 1;

        core_delay_ms(200);
    }
}
