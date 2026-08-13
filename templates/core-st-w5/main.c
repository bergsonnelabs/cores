/**
 * Core.ST.W5 starter — blink the status LED.
 *
 * The W5 is the radio one: STM32WBA55, Cortex-M33 with Bluetooth LE. Two
 * things differ from the L4/H5 templates:
 *
 *   - No USB. The WBA55 has no USB peripheral, so there is no CDC, no
 *     1200-baud touch, and no `make flash-dfu`. Flash over SWD with
 *     `make flash`, which routes through STM32CubeProgrammer (OpenOCD has no
 *     WBA55 support).
 *   - No watchdog in config.json. The `iwdg` + strike-counter brick recovery
 *     escapes into ROM DFU, which needs USB — without it an un-fed watchdog
 *     just reset-loops, so this template leaves it off.
 *
 * For BLE, build with BLE_ENABLED=1 and see the ble-* projects under tests/.
 *
 * "clock": "medium" is 32 MHz from the HSE. Raise it to "high" (64 MHz) or
 * "max" (100 MHz) in config.json when you need the headroom.
 */

#include "core.h"

int main(void)
{
    core_init();
    core_led_init();

    while (1) {
        LED_TOGGLE();
        core_delay_ms(250);
    }
}
