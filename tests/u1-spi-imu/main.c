/**
 * WOM via INT1 callback test.
 *
 * Blue  = idle, waiting for motion
 * Green = motion detected!
 * Fades back to blue after 1 second
 */
#include "core.h"
#include "core_tiles.h"
#include "core_usb.h"
#include "tile_sense_i_6p6.h"
#include "tile_display_rgbw.h"

static tile_t imu;
static tile_t led;
static volatile uint32_t last_wom = 0;
static volatile uint32_t tick = 0;

static void on_imu_event(tile_t *tile, uint32_t events, void *ctx)
{
    (void)tile; (void)ctx;

    if (events & SENSE_I_6P6_EV_WOM_ANY) {
        last_wom = tick;
        tile_display_rgbw_set(&led, 0, 24, 0, 0);

        if (core_usb_connected()) {
            core_usb_printf("WOM! axes=%s%s%s\r\n",
                (events & SENSE_I_6P6_EV_WOM_X) ? "X" : "",
                (events & SENSE_I_6P6_EV_WOM_Y) ? "Y" : "",
                (events & SENSE_I_6P6_EV_WOM_Z) ? "Z" : "");
        }
    }
}

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(500);

    tile_display_rgbw_init(core_tiles_pal(&core_i2c1), 0, &led, NULL);

    sense_i_6p6_cfg_t cfg = {
        .on_event = on_imu_event,
        .int1_pin = 9,
    };
    tile_sense_i_6p6_init(core_tiles_pal(&core_i2c3), 0, &imu, &cfg);

    if (imu.state != TILE_STATE_READY) {
        tile_display_rgbw_set(&led, 16, 0, 0, 0);
        while (1) core_delay_ms(1000);
    }

    /* Configure WOM: 200mg threshold, compare to previous sample */
    tile_sense_i_6p6_wom_config(&imu, 200, 200, 200, SENSE_I_6P6_WOM_PREVIOUS);
    tile_sense_i_6p6_wom_enable(&imu);
    tile_sense_i_6p6_int1_wom(&imu, 1);

    tile_display_rgbw_set(&led, 0, 0, 8, 0);  /* blue = waiting */

    while (1) {
        tile_sense_i_6p6_process(&imu);

        /* Fade back to blue 1 second after last WOM */
        if (tick - last_wom > 100 && last_wom > 0)
            tile_display_rgbw_set(&led, 0, 0, 8, 0);

        tick++;
        core_delay_ms(10);
    }
}
