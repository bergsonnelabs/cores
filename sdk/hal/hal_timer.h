/**
 * hal_timer.h — Timer HAL driver
 *
 * PWM output with frequency/duty control, periodic tick callbacks.
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "hal_common.h"
#include "ll_tim.h"

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    TIM_TypeDef    *instance;
    uint32_t        pclk_hz;
    hal_callback_t  tick_cb;
    void           *tick_ctx;
} hal_timer_t;

/* ============================================================
 * API declarations (implemented in hal_timer.c)
 * ============================================================ */

/* ---- PWM ---- */

/**
 * Initialize a timer for PWM output at the given frequency.
 * Auto-calculates PSC and ARR from pclk_hz and freq_hz.
 *
 * For advanced timers (TIM1), also enables MOE.
 */
hal_status_t hal_timer_pwm_init(hal_timer_t *h, TIM_TypeDef *instance,
                                 uint32_t pclk_hz, uint32_t freq_hz);

/**
 * Set PWM duty cycle for a channel.
 *   channel:      1–4
 *   duty_permil:  0–1000 (0 = off, 500 = 50%, 1000 = 100%)
 */
void hal_timer_pwm_set_duty(hal_timer_t *h, uint8_t channel,
                             uint16_t duty_permil);

/** Change PWM frequency (recalculates PSC/ARR, resets all channel duties) */
void hal_timer_pwm_set_freq(hal_timer_t *h, uint32_t freq_hz);

/** Start the PWM timer */
void hal_timer_pwm_start(hal_timer_t *h);

/** Stop the PWM timer */
void hal_timer_pwm_stop(hal_timer_t *h);

/* ---- Periodic tick ---- */

/**
 * Configure a timer to call a callback at a fixed interval.
 *   period_us: interval in microseconds (1 – 1000000)
 *   cb:        callback function (called from ISR context!)
 *   ctx:       user context passed to callback
 */
hal_status_t hal_timer_tick_init(hal_timer_t *h, TIM_TypeDef *instance,
                                 uint32_t pclk_hz, uint32_t period_us,
                                 hal_callback_t cb, void *ctx);

void hal_timer_tick_start(hal_timer_t *h);
void hal_timer_tick_stop(hal_timer_t *h);

/**
 * Enable periodic tick on an already-initialized timer.
 * Does NOT touch PSC/ARR — the timer keeps its existing timebase.
 * Use this to add a tick callback to a timer that's already set up
 * for PWM or capture via hal_timer_pwm_init().
 *
 * The callback fires on each counter overflow (update event) at the
 * timer's current frequency.
 */
hal_status_t hal_timer_tick_enable(hal_timer_t *h, hal_callback_t cb, void *ctx);

/** Disable the tick callback (clears UIE, keeps timer running). */
void hal_timer_tick_disable(hal_timer_t *h);

#endif /* HAL_TIMER_H */
