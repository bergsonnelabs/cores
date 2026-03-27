/**
 * Ring Test — Sense.I.9 driver validation
 *
 * LED: 1 blink = startup, 3 blinks = driver ready,
 *      heartbeat = live data (fast = motion, slow = still),
 *      SOS = error
 */

#include "core.h"

/* Debug variables (readable via SWD) */
volatile int16_t dbg_accel[3];
volatile int16_t dbg_gyro[3];

int main(void)
{
    /* Clock + LED first so we can see driver init status */
    tile_clock_init();
    ll_systick_init(SYSCLK_HZ);
    led_init();
    led_blink(1, 200, 300);

    /* Pins + I2C + tile drivers */
    tile_pin_init();
    tile_drivers_init();
    led_blink(1, 200, 300);

    if (!tile_is_ready(&tile_sense_i_9_0))
        led_sos();

    led_blink(3, 200, 300);

    /* Live data loop */
    while (1) {
        int16_t accel[3], gyro[3];
        tile_sense_i_9_get_raw_accels(&tile_sense_i_9_0, accel);
        tile_sense_i_9_get_raw_gyros(&tile_sense_i_9_0, gyro);

        dbg_accel[0] = accel[0]; dbg_accel[1] = accel[1]; dbg_accel[2] = accel[2];
        dbg_gyro[0] = gyro[0]; dbg_gyro[1] = gyro[1]; dbg_gyro[2] = gyro[2];

        int32_t mag = (int32_t)accel[0]*accel[0]
                    + (int32_t)accel[1]*accel[1]
                    + (int32_t)accel[2]*accel[2];

        led_heartbeat(mag > 300000000 ? 50 : 500);
    }
}
