/**
 * ble-beacon — BLE beacon for Core.W
 *
 * Uses hal_ble.c init (proven to not hang) + real ST platform files.
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

volatile uint32_t dbg_nvic[6];

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    /* Quick LED flash = alive */
    LED_ON();
    for (volatile int d = 0; d < 1000000; d++);
    LED_OFF();

    hal_ble_init();

    /* BLE init changes AHB prescaler — re-init SysTick */
    ll_systick_init(SYSCLK_HZ / 2);

    /* Let the sequencer run for 2 seconds before starting advertising.
       This ensures all BLE background processes are active. */
    uint8_t adv_started = 0;

    /* Main loop with heartbeat */
    uint32_t last_toggle = 0;
    while (1) {
        extern void UTIL_SEQ_Run(uint32_t mask);
        extern void ble_timer_server_check(void);
        UTIL_SEQ_Run(~0UL);
        ble_timer_server_check();

        extern uint32_t hal_tick(void);
        uint32_t now = hal_tick();

        /* Start advertising after 2 seconds of sequencer running */
        if (!adv_started && now > 2000) {
            hal_ble_advertise("TILETOWN");
            adv_started = 1;
        }

        if ((now - last_toggle) >= 500) {
            last_toggle = now;
            LED_TOGGLE();
        }
    }
}
