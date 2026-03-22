/**
 * LSI test — verify LSI1 oscillator starts on Core.W using LL functions
 *
 * LED pattern:
 *   1 blink  = tile init OK
 *   2 blinks = PWR + backup domain access enabled
 *   3 blinks = LSI1 running
 *   4 blinks = radio sleep timer clock set
 *   continuous = done, success
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"

static void blink_n(int n, int ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  ll_delay_ms(ms);
        LED_OFF(); ll_delay_ms(ms);
    }
    ll_delay_ms(1000);
}

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    blink_n(1, 200);  /* tile init OK */

    /* Enable PWR clock + backup domain access */
    ll_pwr_enable_backup_access();
    blink_n(2, 200);  /* PWR + DBP OK */

    /* Start LSI1 */
    ll_rcc_lsi1_enable();
    while (!ll_rcc_lsi1_ready()) ;
    blink_n(3, 200);  /* LSI1 running */

    /* Set radio sleep timer to LSI */
    ll_rcc_set_radio_sleep_clk(LL_RCC_RADIOSLEEPSOURCE_LSI);
    blink_n(4, 200);  /* Radio sleep clock set */

    /* Success — continuous fast blink */
    while (1) {
        LED_TOGGLE();
        ll_delay_ms(100);
    }
}
