/**
 * Core.ST.L0.1 starter — blink the status LED.
 *
 * The L0 is the small one: STM32L011, no USB. It is flashed over SWD
 * (`make flash`) or the ROM's USART/SPI bootloader — there is no USB DFU and
 * no 1200-baud touch, so `make flash-dfu` does not apply here.
 *
 * core_init() applies the clock level from config.json ("medium" = 2 MHz MSI).
 * Everything else — pads, buses, tiles — is declared in config.json and
 * generated into coregen/ at build time; see docs/start/project-config.
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
