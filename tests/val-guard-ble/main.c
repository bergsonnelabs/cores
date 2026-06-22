/* NEGATIVE TEST */
/**
 * val-guard-ble -- BLE availability guard
 *
 * Core.ST.L4.2, clock=default
 *
 * This test MUST FAIL to compile. It verifies that core_ble.h correctly
 * produces a #error when included on a Core that lacks BLE hardware.
 *
 * Core.ST.L4 is STM32L422 -- no BLE radio. core_ble.h contains:
 *   #if !defined(STM32WBA55xx)
 *   #error "core_ble.h: BLE is only available on Core.ST.W5 (STM32WBA55)."
 *   #endif
 *
 * Expected result: compilation fails with the #error message above.
 * If this file compiles successfully, the availability guard is broken.
 */

#include "core.h"

/* This include MUST trigger a compile-time #error on Core.ST.L4 */
#include "core_ble.h"

int main(void)
{
    core_init();

    /* This code should never be reached -- the #error above must fire */
    core_ble_init();
    core_ble_advertise("should-not-compile");

    while (1) {
        core_ble_process();
        core_delay_ms(1000);
    }
}
