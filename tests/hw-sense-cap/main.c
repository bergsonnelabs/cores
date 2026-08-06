/**
 * hw-sense-cap — touch-event demo on the high-level API (driver v0.3).
 *
 * Board: Core.ST.L4.1 + Sense.CAP r0 (IQS7211A) + 2x3 surface.
 * I2C1 on Core pads 4/5, polled mode (pad 3 is MCLR on the A chip).
 *
 * Streams machine-parseable lines for visualizer_web.py:
 *   D,<t_ms>,<nf>,<x>,<y>,<strength>,<zone>,<touchbits>,<d0..d5>  ~25 Hz
 *   E,<name>,<a>,<b>,<c>                                          events
 * Serial commands: 'r' = re-ATI (re-baseline in the current mechanical
 * configuration), 's1'..'s5' = sensitivity level.
 *
 * Run `python3 visualizer_web.py` and open http://localhost:8765 for
 * the live surface view (heatmap, finger + trail, delta bars, events).
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
        core_usb_printf("E,DOWN,%u,%u,%u\r\n", ev->finger, ev->x, ev->y);
    else if (ev->phase == SENSE_CAP_TOUCH_UP)
        core_usb_printf("E,UP,%u,%u,%u\r\n", ev->finger, ev->x, ev->y);
}

static void on_tap_ev(tile_t *t, uint8_t taps, uint16_t x, uint16_t y, void *ctx)
{
    (void)t; (void)ctx;
    core_usb_printf("E,%s,%d,%u,%u\r\n", taps == 2 ? "DTAP" : "TAP",
                    tile_sense_cap_zone_at(&pad, x, y), x, y);
}

static void on_hold_ev(tile_t *t, uint16_t x, uint16_t y, void *ctx)
{
    (void)t; (void)ctx;
    core_usb_printf("E,HOLD,0,%u,%u\r\n", x, y);
}

static void on_swipe_ev(tile_t *t, uint8_t dir, int16_t vx, int16_t vy, void *ctx)
{
    (void)t; (void)ctx;
    static const char *names[] = { "LEFT", "RIGHT", "UP", "DOWN" };
    core_usb_printf("E,SWIPE_%s,%d,%d,0\r\n", names[dir & 3], vx, vy);
}

static void on_drag_ev(tile_t *t, int16_t dx, int16_t dy, uint16_t x, uint16_t y,
                       void *ctx)
{
    (void)t; (void)ctx; (void)dx; (void)dy; (void)x; (void)y;
}

static void on_pinch_ev(tile_t *t, int16_t delta, void *ctx)
{
    (void)t; (void)ctx;
    core_usb_printf("E,PINCH,%d,0,0\r\n", delta);
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

    /* The preset carries the hardware-characterised recipe (div31 base,
     * target 900, thresholds 8/5). Re-baseline via the 'r' serial
     * command after any mechanical change. */
    uint8_t ok = 0;
    for (uint8_t attempt = 1; attempt <= 3 && !ok; attempt++) {
        ok = tile_sense_cap_setup_2x3(&pad);
        core_usb_printf("recipe 'r0 preset': %s\r\n", ok ? "TUNED" : "no");
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

        /* Serial commands from the visualizer: 'r' = re-ATI (re-baseline
         * in the current mechanical configuration), 's1'..'s5' =
         * sensitivity level. */
        while (core_usb_available()) {
            uint8_t ch = core_usb_getc();
            if (ch == 'r') {
                core_usb_printf("E,REATI_START,0,0,0\r\n");
                uint8_t rok = tile_sense_cap_re_ati(&pad);
                core_usb_printf("E,REATI_%s,0,0,0\r\n", rok ? "OK" : "FAIL");
            } else if (ch == 's') {
                while (!core_usb_available()) { core_delay_ms(1); }
                uint8_t lvl = (uint8_t)(core_usb_getc() - '0');
                tile_sense_cap_set_sensitivity(&pad, lvl);
                core_usb_printf("E,SENS,%u,0,0\r\n", lvl);
            }
        }

        uint32_t now = core_millis();
        if (now - last_status >= 40) {
            last_status = now;
            uint8_t nf = tile_sense_cap_get_num_fingers(&pad);
            core_usb_printf("D,%lu,%u,%u,%u,%u,%d,%02X",
                            (unsigned long)now, nf,
                            tile_sense_cap_get_finger_x(&pad, 0),
                            tile_sense_cap_get_finger_y(&pad, 0),
                            tile_sense_cap_get_finger_strength(&pad, 0),
                            tile_sense_cap_get_zone(&pad),
                            (unsigned)(tile_sense_cap_get_touch_status(&pad) & 0x3F));
            for (uint8_t c = 0; c < tile_sense_cap_get_num_channels(&pad); c++)
                core_usb_printf(",%d",
                                (int16_t)tile_sense_cap_get_channel_delta(&pad, c));
            core_usb_printf("\r\n");
        }
        core_delay_ms(10);
    }
}
