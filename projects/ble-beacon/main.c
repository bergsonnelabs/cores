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

volatile uint8_t dbg_bd_addr[8] = {0};  /* 6 bytes BD addr + padding */
volatile uint32_t dbg_hci_bd_ret = 0xFF;
volatile uint32_t dbg_adv_en_ret = 0xFF;

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
    hal_status_t adv_ret = hal_ble_advertise("TILETOWN");
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

    /* Test HCI transport: read BD address */
    extern uint8_t hci_read_bd_addr(uint8_t *addr);
    dbg_hci_bd_ret = hci_read_bd_addr((uint8_t *)dbg_bd_addr);

    /* Test: explicit advertising enable via raw HCI */
    extern uint8_t hci_le_set_advertising_enable(uint8_t enable);
    dbg_adv_en_ret = hci_le_set_advertising_enable(1);

    LED_ON();  /* LED on = init complete */

    /* Main loop */
    while (1) {
        extern void UTIL_SEQ_Run(uint32_t mask);
        extern void bleplat_timer_check(void);
        UTIL_SEQ_Run(~0UL);
        bleplat_timer_check();
    }
}
