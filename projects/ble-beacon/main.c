/**
 * ble-beacon — BLE beacon for Core.W
 *
 * Advertises "Core.W" over BLE. LED toggles as heartbeat.
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"
#include "hal_ble.h"

void HardFault_Handler(void)
{
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);
    while (1) {
        for (int i = 0; i < 3; i++) {
            LED_ON();  for (volatile int d = 0; d < 100000; d++);
            LED_OFF(); for (volatile int d = 0; d < 100000; d++);
        }
        for (int i = 0; i < 3; i++) {
            LED_ON();  for (volatile int d = 0; d < 500000; d++);
            LED_OFF(); for (volatile int d = 0; d < 500000; d++);
        }
        for (int i = 0; i < 3; i++) {
            LED_ON();  for (volatile int d = 0; d < 100000; d++);
            LED_OFF(); for (volatile int d = 0; d < 100000; d++);
        }
        for (volatile int d = 0; d < 2000000; d++);
    }
}

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    LED_OFF();

    hal_ble_init();

    /* BLE init changes AHB prescaler — re-init SysTick at actual HCLK */
    ll_systick_init(SYSCLK_HZ / 2);  /* Test: if LED correct, AHB is /2 */

    /* Blink error code if advertise fails */
    hal_status_t adv_ret = hal_ble_advertise("Core.W");
    if (adv_ret != HAL_OK) {
        /* Blink the error count: 1=ERROR, 2=BUSY, 3=TIMEOUT, 4=NACK */
        int errc = -(int)adv_ret;
        while (1) {
            for (int i = 0; i < errc; i++) {
                LED_ON();  ll_delay_ms(200);
                LED_OFF(); ll_delay_ms(200);
            }
            ll_delay_ms(2000);
        }
    }

    LED_ON();

    /* Main loop: call BLE stack continuously, toggle LED periodically */
    uint32_t last_toggle = 0;
    while (1) {
        hal_ble_process();

        /* Link layer background processing (deferred radio operations) */
        extern void ll_sys_bg_process(void);
        ll_sys_bg_process();

        uint32_t now = hal_tick();
        if ((now - last_toggle) >= 750) {
            last_toggle = now;
            LED_TOGGLE();
        }
    }
}
