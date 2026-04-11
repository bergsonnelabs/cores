/**
 * Fault Handler Test — Core.W
 *
 * Blinks LED 5 times rapidly, then triggers a HardFault.
 * Expected result: LED switches to SOS pattern (3 short, 3 long, 3 short).
 *
 * To use: rename this to main.c (backup the test main.c first).
 */

#include "core.h"
#include "hal_fault.h"

int main(void)
{
    core_init();
    core_led_init();

    /* 5 rapid blinks = "about to fault" */
    for (int i = 0; i < 5; i++) {
        LED_ON();  core_delay_ms(100);
        LED_OFF(); core_delay_ms(100);
    }

    core_delay_ms(1000);

    /* Trigger HardFault: call invalid function pointer */
    void (*bad)(void) = (void (*)(void))0xDEADDEAD;
    bad();

    /* Should never reach here */
    while (1) ;
}
