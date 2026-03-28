/**
 * Blink — LED blink at full speed
 *
 * Uses core_init() to configure the clock tree (HSI16 → PLL → 80MHz)
 * and the LL layer for GPIO and timing. Portable across all Core tiles.
 */


int main(void)
{
    core_init();
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    while (1) {
        LED_ON();
        ll_delay_ms(250);
        LED_OFF();
        ll_delay_ms(250);
    }
}
