/**
 * hw-sense-cap — touch-event demo on the high-level API (driver v0.3).
 *
 * Board: Core.ST.L4.1 + Sense.CAP r0 (IQS7211A) + 2x3 surface on the
 * bench harness (breadboard + jumpers). I2C1 on Core pads 4/5, polled
 * mode (pad 3 is MCLR on the A-variant chip).
 *
 * Prints every recognizer event as it fires (tap, double-tap, long
 * press, swipe, drag, pinch, down/up) plus a 2 Hz status line with
 * zone, position and channel deltas.
 */

#include "core.h"
#include "core_led.h"
#include "core_usb.h"
#include "core_watchdog.h"
#include "core_timing.h"
#include "core_tiles.h"
#include "tile_sense_cap.h"

static tile_t pad;

static uint32_t clk(void *ctx) { (void)ctx; return core_millis(); }

static void on_touch_ev(tile_t *t, const sense_cap_touch_t *ev, void *ctx)
{
    (void)t; (void)ctx;
    if (ev->phase == SENSE_CAP_TOUCH_DOWN)
        core_usb_printf("[DOWN]  f%u at (%u,%u)\r\n", ev->finger, ev->x, ev->y);
    else if (ev->phase == SENSE_CAP_TOUCH_UP)
        core_usb_printf("[UP]    f%u at (%u,%u)\r\n", ev->finger, ev->x, ev->y);
}

static void on_tap_ev(tile_t *t, uint8_t taps, uint16_t x, uint16_t y, void *ctx)
{
    (void)t; (void)ctx;
    core_usb_printf("[%s] at (%u,%u) zone %d\r\n",
                    taps == 2 ? "DOUBLE-TAP" : "TAP", x, y,
                    tile_sense_cap_zone_at(&pad, x, y));
}

static void on_hold_ev(tile_t *t, uint16_t x, uint16_t y, void *ctx)
{
    (void)t; (void)ctx;
    core_usb_printf("[LONG-PRESS] at (%u,%u)\r\n", x, y);
}

static void on_swipe_ev(tile_t *t, uint8_t dir, int16_t vx, int16_t vy, void *ctx)
{
    (void)t; (void)ctx;
    static const char *names[] = { "LEFT", "RIGHT", "UP", "DOWN" };
    core_usb_printf("[SWIPE %s] v=(%d,%d)\r\n", names[dir & 3], vx, vy);
}

static void on_drag_ev(tile_t *t, int16_t dx, int16_t dy, uint16_t x, uint16_t y,
                       void *ctx)
{
    (void)t; (void)ctx; (void)dx; (void)dy;
    core_usb_printf("[DRAG]  to (%u,%u)\r\n", x, y);
}

static void on_pinch_ev(tile_t *t, int16_t delta, void *ctx)
{
    (void)t; (void)ctx;
    core_usb_printf("[PINCH] %+d\r\n", delta);
}

int main(void)
{
    core_init();
    core_led_heartbeat(1000, 100);
    core_usb_init();
    core_delay_ms(4000);

    core_usb_printf("\r\n=== hw-sense-cap: touch-event demo (v0.3) ===\r\n");

    sense_cap_cfg_t cfg = {
        .rdy_pin  = 0,               /* polled: pad 3 is MCLR on the A chip */
        .gestures = SENSE_CAP_GESTURE_ALL,
        .millis   = clk,
    };
    tile_sense_cap_init(core_tiles_pal(&core_i2c1), 0, &pad, &cfg);

    if (!tile_is_ready(&pad)) {
        core_usb_printf("IQS7211A: NOT FOUND on I2C1 (0x56)\r\n");
        while (1) { core_watchdog_feed(); core_delay_ms(500); }
    }

    /* Bench harness recipe: reduced ATI base for the big attachment
     * capacitance, tripled target for usable deltas, thresholds above
     * the harness noise floor. */
    sense_cap_surface_t surf = sense_cap_surface_2x3;
    surf.ati_base         = 0x023F;
    surf.ati_target       = 900;
    surf.touch_set_mult   = 12;   /* above the harness phantom floor */
    surf.touch_clear_mult = 7;

    uint8_t ok = 0;
    for (uint8_t attempt = 1; attempt <= 3 && !ok; attempt++) {
        ok = tile_sense_cap_configure_surface(&pad, &surf);
        core_usb_printf("surface: attempt %u %s\r\n", attempt,
                        ok ? "configured, ATI OK" : "failed");
    }

    tile_sense_cap_on_touch(&pad, on_touch_ev, NULL);
    tile_sense_cap_on_tap(&pad, on_tap_ev, NULL);
    tile_sense_cap_on_long_press(&pad, on_hold_ev, NULL);
    tile_sense_cap_on_swipe(&pad, on_swipe_ev, NULL);
    tile_sense_cap_on_drag(&pad, on_drag_ev, NULL);
    tile_sense_cap_on_pinch(&pad, on_pinch_ev, NULL);

    uint32_t last_status = 0;
    while (1) {
        core_watchdog_feed();
        tile_sense_cap_process(&pad);

        uint32_t now = core_millis();
        if (now - last_status >= 500) {
            last_status = now;
            int32_t xp, yp;
            tile_sense_cap_get_position_pct(&pad, &xp, &yp);
            core_usb_printf("zone:%2d pos:%3ld,%3ld%% d:",
                            tile_sense_cap_get_zone(&pad),
                            (long)xp, (long)yp);
            for (uint8_t c = 0; c < tile_sense_cap_get_num_channels(&pad); c++)
                core_usb_printf(" %5d",
                                (int16_t)tile_sense_cap_get_channel_delta(&pad, c));
            core_usb_printf("\r\n");
        }
        core_delay_ms(10);
    }
}
