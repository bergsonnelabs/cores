/**
 * Blink — LED blink at full speed
 *
 * Uses tile_init() to configure the clock tree (HSI16 → PLL → 80MHz)
 * and the LL layer for GPIO and timing. Portable across all Core tiles.
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"

int main(void)
{
    tile_init();
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
