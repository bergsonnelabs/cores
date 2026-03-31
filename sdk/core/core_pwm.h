/**
 * core_pwm.h — PWM output and periodic timers
 *
 * Wraps hal_timer with friendlier names. Timer instance
 * resolution still requires explicit init (pad-based auto-resolve
 * from project.json is planned).
 */

#ifndef CORE_PWM_H
#define CORE_PWM_H

#include "hal_timer.h"

/* ============================================================
 * PWM output
 * ============================================================ */

/** Initialize a timer for PWM output at the given frequency. */
static inline hal_status_t core_pwm_init(hal_timer_t *h, TIM_TypeDef *instance,
                                          uint32_t pclk_hz, uint32_t freq_hz)
{
    return hal_timer_pwm_init(h, instance, pclk_hz, freq_hz);
}

/**
 * Set PWM duty cycle for a channel.
 *   channel:      1–4
 *   duty_permil:  0–1000 (0 = off, 500 = 50%, 1000 = 100%)
 */
static inline void core_pwm_set(hal_timer_t *h, uint8_t channel,
                                 uint16_t duty_permil)
{
    hal_timer_pwm_set_duty(h, channel, duty_permil);
}

/** Change PWM frequency (recalculates PSC/ARR, resets all channel duties). */
static inline void core_pwm_set_freq(hal_timer_t *h, uint32_t freq_hz)
{
    hal_timer_pwm_set_freq(h, freq_hz);
}

/** Start the PWM timer. */
static inline void core_pwm_start(hal_timer_t *h)
{
    hal_timer_pwm_start(h);
}

/** Stop the PWM timer. */
static inline void core_pwm_stop(hal_timer_t *h)
{
    hal_timer_pwm_stop(h);
}

/* ============================================================
 * Periodic tick ("every")
 * ============================================================ */

/**
 * Configure a periodic callback at a fixed interval.
 *   period_us: interval in microseconds (1 – 1000000)
 *   cb:        callback function (called from ISR context!)
 *   ctx:       user context passed to callback
 */
static inline hal_status_t core_every_us(hal_timer_t *h, TIM_TypeDef *instance,
                                          uint32_t pclk_hz, uint32_t period_us,
                                          hal_callback_t cb, void *ctx)
{
    return hal_timer_tick_init(h, instance, pclk_hz, period_us, cb, ctx);
}

/** Start the periodic timer. */
static inline void core_every_start(hal_timer_t *h)
{
    hal_timer_tick_start(h);
}

/** Stop the periodic timer. */
static inline void core_every_stop(hal_timer_t *h)
{
    hal_timer_tick_stop(h);
}

#endif /* CORE_PWM_H */
