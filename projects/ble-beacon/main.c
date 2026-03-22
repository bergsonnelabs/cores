/**
 * ble-beacon — BLE beacon for Core.W
 *
 * Initializes the BLE stack and starts advertising "Core.W" as the
 * device name. LED toggles to show the main loop is running.
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"
#include "hal_ble.h"

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    hal_ble_init();
    hal_ble_advertise("Core.W");

    LED_ON();

    while (1)
    {
        hal_ble_process();
        ll_delay_ms(750);
        LED_TOGGLE();
    }
}
