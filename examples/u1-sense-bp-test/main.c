/**
 * u1-sense-bp-test — Hardware validation for the Sense.BP tile driver.
 *
 * Reads pressure and temperature from the ILPS22QS on I2C3 and prints
 * formatted output over USB CDC every second. A Disp.RGBW on I2C1
 * provides visual status:
 *
 *   Green  = sensor found and reading
 *   Blue   = pressure data ready (flickers each read)
 *   Red    = sensor init failed
 *   Yellow = USB not connected (waiting)
 *
 * Core.U.1, custom bootloader — flash with: make flash-dfu
 */

#include "core.h"
#include "core_i2c.h"
#include "core_usb.h"
#include "core_tiles.h"
#include "tile_disp_rgbw.h"
#include "tile_sense_bp.h"

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(200);

    /* ---- Init Disp.RGBW on I2C1 ---- */

    tile_t led;
    tile_disp_rgbw_init(core_tiles_hal(&core_i2c1), 0, &led);
    tile_disp_rgbw_set(&led, 16, 16, 0, 0);  /* Yellow: waiting */

    /* Wait for USB host to connect */
    core_delay_ms(1500);

    /* ---- Init Sense.BP on I2C3 ---- */

    tiles_hal_t *hal3 = core_tiles_hal(&core_i2c3);
    tile_t baro;

    core_usb_printf("\r\n--- Sense.BP (ILPS22QS) hardware test ---\r\n");

    /* Scan I2C3 to report what's there */
    uint8_t addrs[16];
    uint8_t scan_count = 0;
    core_i2c_scan(&core_i2c3, addrs, &scan_count, sizeof(addrs));
    core_usb_printf("I2C3 scan: %d device(s) found\r\n", scan_count);
    for (uint8_t i = 0; i < scan_count; i++)
        core_usb_printf("  0x%02X\r\n", addrs[i]);

    /* Find */
    uint8_t found = tile_sense_bp_find(hal3, 0);
    core_usb_printf("find(instance 0): %s\r\n", found ? "OK" : "NOT FOUND");

    if (!found) {
        /* Try alternate address */
        found = tile_sense_bp_find(hal3, 1);
        core_usb_printf("find(instance 1): %s\r\n", found ? "OK" : "NOT FOUND");
    }

    /* Init with sensible defaults: 25 Hz, AVG=4, 1260 hPa mode */
    sense_bp_cfg_t cfg = {
        .odr    = SENSE_BP_ODR_25HZ,
        .avg    = SENSE_BP_AVG_4,
        .fs     = SENSE_BP_FS_1260HPA,
        .lpf    = 1,
        .lpf_bw = SENSE_BP_LPF_ODR_4,
        .bdu    = 1,
    };
    tile_sense_bp_init(hal3, 0, &baro, &cfg);

    if (!tile_is_ready(&baro)) {
        /* Try alternate address */
        tile_sense_bp_init(hal3, 1, &baro, &cfg);
    }

    if (tile_is_ready(&baro)) {
        core_usb_printf("init: OK (addr 0x%02X)\r\n", baro.id);
        tile_disp_rgbw_set(&led, 0, 24, 0, 0);  /* Green: ready */
    } else {
        core_usb_printf("init: FAILED\r\n");
        tile_disp_rgbw_set(&led, 24, 0, 0, 0);  /* Red: error */

        while (1) {
            core_delay_ms(1000);
            core_usb_printf("ERROR: Sense.BP not responding\r\n");
        }
    }

    core_usb_printf("\r\nReading pressure and temperature...\r\n\r\n");

    /* ---- Main loop ---- */

    uint32_t sample = 0;
    uint8_t toggle = 0;

    while (1) {
        /* Wait for pressure data */
        uint16_t timeout = 500;
        while (!tile_sense_bp_pressure_ready(&baro) && timeout--)
            core_delay_ms(1);

        int32_t raw_p = tile_sense_bp_get_pressure_raw(&baro);
        int32_t mhpa  = tile_sense_bp_get_pressure_mhpa(&baro);
        int16_t raw_t = tile_sense_bp_get_temp_raw(&baro);
        int32_t cdeg  = tile_sense_bp_get_temp_cdeg(&baro);

        /* Format: integer part and fractional part */
        int32_t p_int  = mhpa / 1000;
        int32_t p_frac = mhpa % 1000;
        if (p_frac < 0) p_frac = -p_frac;

        int32_t t_int  = cdeg / 100;
        int32_t t_frac = cdeg % 100;
        if (t_frac < 0) t_frac = -t_frac;

        core_usb_printf("[%04lu] P: %ld.%03ld hPa (raw %ld)  T: %ld.%02ld C (raw %d)\r\n",
                        (unsigned long)(sample++),
                        (long)p_int, (long)p_frac,
                        (long)raw_p,
                        (long)t_int, (long)t_frac,
                        (int)raw_t);

        /* Alternate green/blue to show activity */
        if (toggle) {
            tile_disp_rgbw_set(&led, 0, 16, 0, 0);
        } else {
            tile_disp_rgbw_set(&led, 0, 0, 16, 0);
        }
        toggle ^= 1;

        core_delay_ms(1000);
    }
}
