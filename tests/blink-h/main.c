/**
 * Core.H — CSI 4 MHz test (clock = "low")
 */

#include "core.h"

int main(void)
{
    core_init();  /* clock=low → CSI 4 MHz */

    ll_rcc_gpio_clk_enable(GPIOA);
    ll_gpio_config_output(GPIOA, 5);

    while (1) {
        GPIOA->BSRR = (1UL << 5);
        core_delay_ms(50);
        GPIOA->BSRR = (1UL << 21);
        core_delay_ms(50);
    }
}
