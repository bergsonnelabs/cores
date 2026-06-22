/* NEGATIVE TEST */
/**
 * val-guard-usb -- USB availability guard
 *
 * Core.ST.W5, clock=default
 *
 * This test MUST FAIL to compile. It verifies that core_usb.h correctly
 * produces a #error when included on a Core that lacks USB hardware.
 *
 * Core.ST.W5 is STM32WBA55 -- no USB peripheral. core_usb.h contains:
 *   #if !defined(STM32L422xx) && !defined(STM32H523xx)
 *   #error "core_usb.h: USB is not available on this Core tile."
 *   #endif
 *
 * Expected result: compilation fails with the #error message above.
 * If this file compiles successfully, the availability guard is broken.
 */

#include "core.h"

/* This include MUST trigger a compile-time #error on Core.ST.W5 */
#include "core_usb.h"

int main(void)
{
    core_init();

    /* This code should never be reached -- the #error above must fire */
    core_usb_init();
    core_usb_printf("should not compile\r\n");

    while (1) {
        core_delay_ms(1000);
    }
}
