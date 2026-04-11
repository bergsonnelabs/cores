/**
 * Core.H — Watchdog test (self-verifying)
 *
 * Boot 1: 3 scope pulses, start IWDG (2s), feed 4×, stop feeding → reset
 * Boot 2: detects IWDG flag → 10 rapid pulses = SUCCESS, then heartbeat
 *
 * Uses PA5 (pad 9) for scope output.
 */

#include "core.h"
#include "core_power.h"

static void pulse(int n, int ms) {
    for (int i = 0; i < n; i++) {
        GPIOA->BSRR = (1UL << 5);
        core_delay_ms(ms);
        GPIOA->BSRR = (1UL << 21);
        core_delay_ms(ms);
    }
}

int main(void)
{
    core_init();
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_gpio_config_output(GPIOA, 5);

    if (core_watchdog_caused_reset()) {
        /* Boot 2: watchdog reset detected — SUCCESS */
        ll_rcc_clear_reset_flags();
        pulse(10, 25);  /* 10 rapid pulses */
        /* Heartbeat — keep feeding */
        while (1) {
            GPIOA->BSRR = (1UL << 5); core_delay_ms(200);
            GPIOA->BSRR = (1UL << 21); core_delay_ms(200);
            core_watchdog_feed();
        }
    }

    /* Boot 1: normal — 3 slow pulses */
    pulse(3, 100);
    core_delay_ms(500);

    /* Start IWDG with 2-second timeout */
    core_watchdog_start(2);

    /* Feed 4 times (4 pulses) */
    for (int i = 0; i < 4; i++) {
        pulse(1, 50);
        core_delay_ms(200);
        core_watchdog_feed();
    }

    /* Stop feeding — wait for reset */
    while (1) ;
}
