/**
 * core_pwm.h — PWM output and periodic timers
 *
 * Two usage modes:
 *
 *   1. Pad-centric (recommended — requires coregen):
 *        core_pwm_init_pad(&pwm, 7, 1000);   // pad → TIM + channel auto-resolved
 *        core_pwm_set_pad(&pwm, 7, 500);      // 50% duty on pad 7
 *
 *   2. Explicit instance (no coregen needed):
 *        core_pwm_init(&pwm, TIM2, SYSCLK_HZ, 1000);
 *        core_pwm_set(&pwm, 3, 500);           // channel 3 directly
 */

#ifndef CORE_PWM_H
#define CORE_PWM_H

#include "core_timer.h"

/* ============================================================
 * PWM output
 * ============================================================ */

/** Initialize a timer for PWM output at the given frequency.
 *  Clock is auto-resolved from SYSCLK_HZ (core_config.h).
 */
static inline hal_status_t core_pwm_init(core_timer_t *h, TIM_TypeDef *instance,
                                          uint32_t freq_hz)
{
    return hal_timer_pwm_init(h, instance, SYSCLK_HZ, freq_hz);
}

/** @deprecated Use core_pwm_init(h, instance, freq_hz) — clock auto-resolved. */
static inline hal_status_t core_pwm_init_clk(core_timer_t *h, TIM_TypeDef *instance,
                                              uint32_t pclk_hz, uint32_t freq_hz)
{
    return hal_timer_pwm_init(h, instance, pclk_hz, freq_hz);
}

/**
 * Set PWM duty cycle for a channel.
 *   channel:      1–4
 *   duty_permil:  0–1000 (0 = off, 500 = 50%, 1000 = 100%)
 */
static inline void core_pwm_set(core_timer_t *h, uint8_t channel,
                                 uint16_t duty_permil)
{
    hal_timer_pwm_set_duty(h, channel, duty_permil);
}

/** Change PWM frequency (recalculates PSC/ARR, resets all channel duties). */
static inline void core_pwm_set_freq(core_timer_t *h, uint32_t freq_hz)
{
    hal_timer_pwm_set_freq(h, freq_hz);
}

/** Start the PWM timer. */
static inline void core_pwm_start(core_timer_t *h)
{
    hal_timer_pwm_start(h);
}

/** Stop the PWM timer. */
static inline void core_pwm_stop(core_timer_t *h)
{
    hal_timer_pwm_stop(h);
}

/* ============================================================
 * PWM output — pad-centric (requires coregen)
 *
 * These wrap tal_timer.h and auto-resolve the timer instance and
 * channel from the pad number using coregen-generated mappings.
 * Only available when core_pads.h provides core_pad_timer_info().
 * ============================================================ */

#if __has_include("core_pads.h") && __has_include("core_config.h")
#include "tal_timer.h"

/** Initialize PWM on a pad. Timer instance resolved from project.json. */
static inline hal_status_t core_pwm_init_pad(core_timer_t *h, uint8_t pad,
                                              uint32_t freq_hz)
{
    return tal_pwm_init(h, pad, freq_hz);
}

/** Set PWM duty on a pad (0–1000 permil). */
static inline void core_pwm_set_pad(core_timer_t *h, uint8_t pad,
                                     uint16_t duty_permil)
{
    tal_pwm_set(h, pad, duty_permil);
}
#endif

/* ============================================================
 * Periodic tick ("every")
 * ============================================================ */

/**
 * Configure a periodic callback at a fixed interval.
 *   period_us: interval in microseconds (1 – 1000000)
 *   cb:        callback function (called from ISR context!)
 *   ctx:       user context passed to callback
 *
 * Clock is auto-resolved from SYSCLK_HZ (core_config.h).
 */
static inline hal_status_t core_every_us(core_timer_t *h, TIM_TypeDef *instance,
                                          uint32_t period_us,
                                          hal_callback_t cb, void *ctx)
{
    return hal_timer_tick_init(h, instance, SYSCLK_HZ, period_us, cb, ctx);
}

/** @deprecated Use core_every_us(h, instance, period_us, cb, ctx) — clock auto-resolved. */
static inline hal_status_t core_every_us_clk(core_timer_t *h, TIM_TypeDef *instance,
                                              uint32_t pclk_hz, uint32_t period_us,
                                              hal_callback_t cb, void *ctx)
{
    return hal_timer_tick_init(h, instance, pclk_hz, period_us, cb, ctx);
}

/** Start the periodic timer. */
static inline void core_every_start(core_timer_t *h)
{
    hal_timer_tick_start(h);
}

/** Stop the periodic timer. */
static inline void core_every_stop(core_timer_t *h)
{
    hal_timer_tick_stop(h);
}

#endif /* CORE_PWM_H */
