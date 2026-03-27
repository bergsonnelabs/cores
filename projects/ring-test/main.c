/**
 * Ring Test — Sense.I.9 driver validation
 *
 * Uses auto-generated tile driver init from coregen.
 * The Kiln HAL setup, driver find/init are all handled by tile_init().
 *
 * LED:
 *   1 blink  = startup OK
 *   3 blinks = Kiln driver READY
 *   Heartbeat = reading live data (fast = motion, slow = still)
 *   SOS = error
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"
#include "tile_sense_i_9.h"

/* ---- Debug variables (readable via SWD) ---- */

volatile int16_t dbg_accel[3];
volatile int16_t dbg_gyro[3];

/* ---- LED helpers ---- */

static void blink_n(int n, int on_ms, int off_ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  ll_delay_ms(on_ms);
        LED_OFF(); ll_delay_ms(off_ms);
    }
}

static void sos(void)
{
    while (1) {
        blink_n(3, 100, 100);
        blink_n(3, 400, 400);
        blink_n(3, 100, 100);
        ll_delay_ms(2000);
    }
}

/* ---- Main ---- */

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);

    /* LED */
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    blink_n(1, 200, 300);  /* startup */

    if (!tile_is_ready(&tile_sense_i_9_0)) {
        sos();
    }

    blink_n(3, 200, 300);  /* driver ready */

    while (1) {
        int16_t accel[3], gyro[3];
        tile_sense_i_9_get_raw_accels(&tile_sense_i_9_0, accel);
        tile_sense_i_9_get_raw_gyros(&tile_sense_i_9_0, gyro);

        dbg_accel[0] = accel[0]; dbg_accel[1] = accel[1]; dbg_accel[2] = accel[2];
        dbg_gyro[0] = gyro[0]; dbg_gyro[1] = gyro[1]; dbg_gyro[2] = gyro[2];

        int32_t mag = (int32_t)accel[0]*accel[0] + (int32_t)accel[1]*accel[1] + (int32_t)accel[2]*accel[2];
        LED_TOGGLE();
        ll_delay_ms(mag > 300000000 ? 50 : 500);
    }
}
