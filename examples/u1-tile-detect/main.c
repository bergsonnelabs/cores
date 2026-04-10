/**
 * u1-tile-detect — I2C3 tile detection tester.
 *
 * Scans the I2C3 bus continuously:
 *   Green = at least one device found
 *   Red   = no device found
 *
 * Core.U.1 with Disp.RGBW on I2C1, test socket on I2C3.
 * Custom bootloader — flash with: make flash-dfu
 */
#include "core.h"
#include "core_i2c.h"
#include "core_tiles.h"
#include "tile_disp_rgbw.h"

int main(void)
{
    core_init();
    core_delay_ms(100);

    /* Init RGBW indicator on I2C1 */
    tile_t led;
    tile_disp_rgbw_init(core_tiles_pal(&core_i2c1), 0, &led);

    uint8_t addrs[16];
    uint8_t count;

    while (1) {
        count = 0;
        core_i2c_scan(&core_i2c3, addrs, &count, sizeof(addrs));

        if (count > 0) {
            tile_disp_rgbw_set(&led, 0, 24, 0, 0);   /* Green */
        } else {
            tile_disp_rgbw_set(&led, 24, 0, 0, 0);    /* Red */
        }

        core_delay_ms(500);
    }
}
