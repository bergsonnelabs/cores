/**
 * Sense.T.C touch demo with Disp.RGBW visual feedback.
 *
 * Touch surface -> green LED
 * Proximity     -> red LED
 * Idle          -> LED off
 */
#include "core.h"
#include "core_tiles.h"
#include "core_usb.h"
#include "tile_sense_t_c.h"
#include "tile_disp_rgbw.h"

static tile_t touch;
static tile_t led;

static void on_touch_event(tile_t *tile, uint16_t status, void *ctx)
{
    (void)tile; (void)ctx;

    if (status & SENSE_T_C_SURFACE_TOUCH)
        tile_disp_rgbw_set(&led, 0, 24, 0, 0);
    else if (status & SENSE_T_C_SURFACE_PROX)
        tile_disp_rgbw_set(&led, 24, 0, 0, 0);
    else
        tile_disp_rgbw_off(&led);
}

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(500);

    tile_disp_rgbw_init(core_tiles_pal(&core_i2c1), 0, &led, NULL);

    sense_t_c_cfg_t cfg = {
        .on_event        = on_touch_event,
        .channels        = (1 << SENSE_T_C_CH_SURFACE),
        .prox_threshold  = 4,
        .touch_threshold = 8,
        .rdy_pin         = 3,
    };
    tile_sense_t_c_init(core_tiles_pal(&core_i2c3), 0, &touch, &cfg);

    while (1) {
        tile_sense_t_c_process(&touch);
        core_delay_ms(30);
    }
}
