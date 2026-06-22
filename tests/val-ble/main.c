/**
 * val-ble -- Minimal BLE advertising validation (green-field)
 *
 * Core.ST.W5, BLE enabled, no pads, no pairing, no services.
 *
 * The absolute basics: bring up the stack and advertise a name.
 * If a scanner sees "val-ble", the radio + stack + advertising path
 * are all healthy. Nothing else is exercised on purpose.
 *
 * Exercises only: core_init, core_ble_init, core_ble_process,
 *                 core_ble_advertise.
 */

#include "core.h"
#include "core_ble.h"

int main(void)
{
    core_init();

    core_ble_init();

    /* Header contract: advertise after init + at least one process() tick. */
    core_ble_process();
    core_ble_advertise("val-ble");

    while (1) {
        core_ble_process();
    }
}
