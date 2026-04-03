/**
 * BLE Beacon — minimal BLE advertising on Core.W
 *
 * Advertises as "TILETOWN" and blinks LED as heartbeat.
 */

#include "core.h"
#include "stm32_seq.h"

extern void ble_app_init(void);
extern int  ble_app_advertise(const char *name);

int main(void)
{
    core_init();
    core_led_init();

    /* Initialize BLE stack + GAP + GATT */
    ble_app_init();

    /* Start advertising from main loop (ACI commands need sequencer) */
    uint8_t adv_started = 0;
    uint32_t loops = 0;

    while (1) {
        UTIL_SEQ_Run(~0UL);
        loops++;

        /* Start advertising after sequencer warm-up */
        if (!adv_started && loops > 100) {
            ble_app_advertise("TILETOWN");
            adv_started = 1;
        }

        /* Heartbeat LED — brief flash every 2s */
        {
            static uint32_t last_on = 0;
            uint32_t now = core_millis();
            if (now - last_on >= 2000) {
                last_on = now;
                LED_ON();
            } else if (now - last_on >= 20) {
                LED_OFF();
            }
        }
    }
}
