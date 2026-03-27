/**
 * Blink — LED blink demo
 *
 * Simplest possible project. Uses core_init() for clock + SysTick,
 * core_led for the onboard LED.
 */

#include "core.h"

int main(void)
{
    core_init();
    core_led_init();

    while (1) {
        LED_ON();
        ll_delay_ms(250);
        LED_OFF();
        ll_delay_ms(250);
    }
}
