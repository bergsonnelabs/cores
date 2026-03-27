/**
 * Tick Toggle — HAL timer interrupt test
 *
 * Uses hal_timer_tick_init to fire a periodic ISR callback
 * that toggles Pad 7 as plain GPIO. No PWM hardware — just
 * a software toggle from the timer interrupt.
 *
 * Target: 1.234kHz output → period = 810.37µs
 * The toggle fires at 2× the output frequency (each toggle
 * is a half-cycle), so the tick period = 405µs.
 *
 * Scope on Pad 7 should show ~1.234kHz square wave.
 */

#include "core_init.h"
#include "core_config.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "ll_rcc.h"

static void tick_callback(void *ctx)
{
    (void)ctx;
    hal_pad_toggle(7);
}

int main(void)
{
    core_init();

    /* Configure Pad 7 as GPIO output (not AF/timer — just plain GPIO) */
    hal_pad_output(7);

    /* Set up TIM2 to fire every 405µs (→ 1.234kHz toggle) */
    ll_rcc_apb1_clk_enable(LL_APB1_TIM2);
    hal_timer_t tick;
    hal_timer_tick_init(&tick, TIM2, SYSCLK_HZ, 405, tick_callback, NULL);
    hal_timer_tick_start(&tick);

    /* Main loop does nothing — all work happens in the ISR */
    while (1) {
        __asm volatile ("wfi");  /* Sleep until next interrupt */
    }
}
