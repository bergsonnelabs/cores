/**
 * Blink — LED blink using the LL layer and tilegen headers
 *
 * Portable across all Core tiles — uses generated tile_board.h
 * for the LED pin and LL functions for GPIO and timing.
 */

#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"

int main(void)
{
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    while (1) {
        LED_ON();
        ll_delay_ms(250);
        LED_OFF();
        ll_delay_ms(250);
    }
}
