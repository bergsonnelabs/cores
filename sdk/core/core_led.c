/**
 * core_led.c — Free-running heartbeat state machine
 *
 * The on/off/toggle/blink helpers are header-only static inlines in
 * core_led.h. The heartbeat is the one LED pattern that needs persistent
 * state and a periodic tick, so it lives here:
 *
 *   - core_led_heartbeat(period_ms, on_ms) stores the rhythm and arms it.
 *   - core_led_systick_tick() advances the state machine; SysTick_Handler
 *     calls it once per millisecond through a weak hook (see hal_systick.c).
 *
 * Keeping the toggle out of the main loop means a starter program can do
 *   on start { led.heartbeat(1000, 100) }
 * and leave the loop free for real work — the LED keeps blinking on its
 * own off the SysTick interrupt.
 */

/* Pull in the project's generated umbrella so the MCU register context
 * (REG32 / SYSTICK_BASE from ll_common.h) and board macros (LED_PORT/PIN
 * from core_board.h) are established before core_led.h's includes — the
 * same ordering every main.c relies on. core_led.o is compiled per project
 * (it depends on the generated headers), so core.h is always available. */
#include "core.h"

/* Heartbeat configuration + phase. Touched by both the caller (arming the
 * heartbeat) and the SysTick ISR (advancing it), so everything the ISR
 * reads is volatile. period_ms == 0 means "disabled". */
static volatile uint32_t s_period_ms = 0;
static volatile uint32_t s_on_ms = 0;
static volatile uint32_t s_phase_ms = 0;

void core_led_heartbeat(int period_ms, int on_ms)
{
    /* Negative inputs are nonsense (the DSL bounds-checks to [0..60000],
     * but the C entry point is public) — treat them as 0. */
    uint32_t period = period_ms > 0 ? (uint32_t)period_ms : 0u;
    uint32_t on = on_ms > 0 ? (uint32_t)on_ms : 0u;
    if (on > period) {
        on = period;  /* on-time can't exceed the cycle */
    }

    /* Disabled: stop the heartbeat and leave the LED off. */
    if (period == 0u) {
        s_period_ms = 0u;
        LED_OFF();
        return;
    }

    /* Arm it. Reset the phase and drive the first edge immediately so the
     * caller sees the LED react now rather than up to one cycle later.
     * Writing s_period_ms last means the ISR never observes a half-updated
     * config (it bails while s_period_ms is still 0). */
    s_phase_ms = 0u;
    s_on_ms = on;
    if (on > 0u) {
        LED_ON();
    } else {
        LED_OFF();
    }
    s_period_ms = period;
}

void core_led_systick_tick(void)
{
    uint32_t period = s_period_ms;
    if (period == 0u) {
        return;  /* heartbeat not running */
    }

    /* Advance one millisecond, wrapping at the cycle boundary. The on-edge
     * fires at phase 0 (cycle start), the off-edge at phase == on_ms. With
     * on_ms == 0 the LED never lights; with on_ms == period it never goes
     * dark (a solid-on "heartbeat"). */
    uint32_t phase = s_phase_ms + 1u;
    if (phase >= period) {
        phase = 0u;
        if (s_on_ms > 0u) {
            LED_ON();
        }
    } else if (phase == s_on_ms) {
        LED_OFF();
    }
    s_phase_ms = phase;
}
