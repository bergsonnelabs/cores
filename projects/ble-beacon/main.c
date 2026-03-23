/**
 * ble-beacon — BLE beacon for Core.W
 *
 * Uses real ST middleware (HAL, SCM, LPM, BPKA, etc.) via app_entry.c
 * and our hal_ble.c wrapper for the BLE stack.
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
    /* HSE tuning from OTP — must happen before clock init */
    extern void MX_APPE_Config(void);
    MX_APPE_Config();

    /* Standard tile init (clocks, GPIO, etc.) */
    tile_init();
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    /* Quick LED flash = alive */
    LED_ON();
    for (volatile int d = 0; d < 1000000; d++);
    LED_OFF();

    /* Initialize ST middleware (AMM, RNG, Flash, BPKA, SNVMA, SCM) */
    extern uint32_t MX_APPE_Init(void *p_param);
    MX_APPE_Init((void *)0);

    /* Initialize BLE stack, GAP, GATT */
    hal_ble_init();

    /* Let the sequencer run for 2 seconds before starting advertising.
       This ensures all BLE background processes are active. */
    uint8_t adv_started = 0;

    /* Main loop with heartbeat */
    uint32_t last_toggle = 0;
    while (1) {
        extern void UTIL_SEQ_Run(uint32_t mask);
        UTIL_SEQ_Run(~0UL);

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
