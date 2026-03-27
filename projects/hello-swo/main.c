/**
 * Hello SWO — Debug printf via SWD trace
 *
 * Prints messages through ITM/SWO using the existing ST-Link
 * debug cable. No UART adapter needed.
 *
 * Build:  make PROJECT=hello-swo
 * Run:    make PROJECT=hello-swo swo
 *
 * You should see "Hello from Core.U.2!" and a counting message
 * in the terminal where you ran `make swo`.
 */

#include "hal_debug.h"
#include "hal_gpio.h"

int main(void)
{
    core_init();

    /* Initialize SWO debug output */
    hal_debug_init(SYSCLK_HZ);

    /* LED heartbeat */
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    hal_debug_printf("Hello from Core.U.2!\r\n");
    hal_debug_printf("SYSCLK = %lu MHz\r\n", SYSCLK_HZ / 1000000UL);

    uint32_t count = 0;
    while (1) {
        hal_debug_printf("tick %lu\r\n", count++);
        LED_TOGGLE();
        ll_delay_ms(500);
    }
}
