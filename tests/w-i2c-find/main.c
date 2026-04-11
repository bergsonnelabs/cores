/**
 * Probe 0x41 (Sense.TOF) on I2C1 — ONLY this address
 */
#include "core.h"
#include "core_i2c.h"

int main(void)
{
    core_init();
    core_led_init();
    core_delay_ms(500);

    int found = (core_i2c_probe(&core_i2c1, 0x69) == 0) ? 1 : 0;

    while (1) {
        LED_TOGGLE();
        core_delay_ms(found ? 100 : 1000);
    }
}
