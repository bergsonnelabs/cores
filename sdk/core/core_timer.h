/**
 * core_timer.h — Timer subsystem: timebase, PWM, input capture, periodic tick
 *
 * The timer is the fundamental object. It has a frequency (shared
 * timebase), and its channels can independently do PWM output,
 * input capture, or nothing. The update interrupt (tick) is
 * orthogonal — it piggybacks on the same timebase.
 *
 * Typical usage:
 *
 *   #include "core_timer.h"
 *
 *   core_timer_t tim1;
 *   core_timer_init_freq(&tim1, TIM1, 1000);   // 1 kHz overflow
 *   core_timer_pwm_set(&tim1, 1, 500);        // CH1 = 50% PWM (permil)
 *   core_timer_capture_init(&tim1, 3);         // CH3 = capture
 *   core_timer_start(&tim1);
 *
 *   while (1) {
 *       core_timer_pwm_set(&tim1, 1, new_duty);
 *       uint32_t cap = core_timer_capture_read(&tim1, 3);
 *   }
 */

#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include "hal_timer.h"
#include "ll_tim.h"

/** Core-level timer handle. Alias for hal_timer_t — use this in application code. */
typedef hal_timer_t core_timer_t;

/** Core-level callback type. */
typedef hal_callback_t core_callback_t;

#if __has_include("core_config.h")
#include "core_config.h"  /* SYSCLK_HZ */
#else
#ifndef SYSCLK_HZ
#error "SYSCLK_HZ not defined — include core_config.h or define it manually"
#endif
#endif

/* ============================================================
 * Timer timebase
 * ============================================================ */

/**
 * Initialize a timer at a given overflow frequency.
 * Use for PWM output and periodic tick — the frequency is how
 * often the counter wraps (= the PWM frequency).
 *
 *   core_timer_init_freq(&t, TIM2, 1000);   // overflows at 1 kHz
 */
static inline hal_status_t core_timer_init_freq(core_timer_t *h,
                                                 TIM_TypeDef *instance,
                                                 uint32_t freq_hz)
{
    return hal_timer_pwm_init(h, instance, SYSCLK_HZ, freq_hz);
}

/**
 * Initialize a timer at a given tick rate, free-running to max count.
 * Use for input capture — the tick rate sets the measurement
 * resolution, and the counter runs as long as possible before
 * wrapping (0xFFFF for 16-bit, 0xFFFFFFFF for 32-bit TIM2).
 *
 *   core_timer_init_tick(&t, TIM2, 1000000);  // 1 us per tick
 *   core_timer_capture_init(&t, 1);
 *   core_timer_start(&t);
 */
static inline hal_status_t core_timer_init_tick(core_timer_t *h,
                                                 TIM_TypeDef *instance,
                                                 uint32_t tick_hz)
{
    hal_status_t rc = hal_timer_pwm_init(h, instance, SYSCLK_HZ, tick_hz);
    uint32_t psc = (SYSCLK_HZ / tick_hz) - 1;
    h->instance->PSC = psc;
    h->instance->ARR = 0xFFFFFFFF;  /* 16-bit timers see 0xFFFF, 32-bit see full range */
    h->instance->EGR = 1;           /* force reload */
    return rc;
}

/* Backward compat */
#define core_timer_init          core_timer_init_freq
#define core_timer_init_freerun  core_timer_init_tick

/** Start the timer counter. */
static inline void core_timer_start(core_timer_t *h)
{
    hal_timer_pwm_start(h);
}

/** Stop the timer counter. */
static inline void core_timer_stop(core_timer_t *h)
{
    hal_timer_pwm_stop(h);
}

/** Change the timer frequency (recalculates PSC/ARR). */
static inline void core_timer_set_freq(core_timer_t *h, uint32_t freq_hz)
{
    hal_timer_pwm_set_freq(h, freq_hz);
}

/* ============================================================
 * PWM output (per-channel)
 * ============================================================ */

/**
 * Set PWM duty cycle for a channel.
 *   channel:      1–4
 *   duty_permil:  0–1000 (0 = off, 500 = 50%, 1000 = always on)
 */
static inline void core_timer_pwm_set(core_timer_t *h, uint8_t channel,
                                       uint16_t duty_permil)
{
    hal_timer_pwm_set_duty(h, channel, duty_permil);
}

/**
 * Set PWM duty cycle by pad number (requires coregen).
 * Resolves the channel from the pad's timer assignment.
 *   duty_permil:  0–1000 (0 = off, 500 = 50%, 1000 = always on)
 */
#if __has_include("core_pads.h")
#include "core_pads.h"
#ifdef CORE_HAS_TIMER_PADS
static inline void core_timer_pwm_set_pad(core_timer_t *h, uint8_t pad,
                                           uint16_t duty_permil)
{
    TIM_TypeDef *inst;
    uint8_t ch;
    if (core_pad_timer_info(pad, &inst, &ch) == 0) {
        hal_timer_pwm_set_duty(h, ch, duty_permil);
    }
}
#endif
#endif

/* ============================================================
 * Input capture (per-channel)
 * ============================================================ */

/**
 * Configure a channel for input capture (rising edge).
 * The timer must already be initialized with core_timer_init().
 */
static inline void core_timer_capture_init(core_timer_t *h, uint8_t channel)
{
    ll_tim_ic_config(h->instance, channel);
}

/** Read the last captured value from a channel. */
static inline uint32_t core_timer_capture_read(core_timer_t *h, uint8_t channel)
{
    return ll_tim_ic_read(h->instance, channel);
}

/* ============================================================
 * Periodic tick (ISR callback at the timer frequency)
 * ============================================================ */

/**
 * Enable periodic tick on an already-initialized timer.
 * Does NOT touch PSC/ARR — the timer keeps its existing timebase.
 * The callback fires on each counter overflow at the timer's frequency.
 *
 * Typical pattern:
 *   core_timer_init(&t, TIM2, 1000);         // 1 kHz timebase
 *   core_timer_pwm_set(&t, 1, 500);          // CH1 = 50% PWM (permil)
 *   core_timer_enable_tick(&t, on_tick, NULL); // also fire ISR at 1 kHz
 *   core_timer_start(&t);
 */
static inline hal_status_t core_timer_enable_tick(core_timer_t *h,
                                                   core_callback_t cb, void *ctx)
{
    return hal_timer_tick_enable(h, cb, ctx);
}

/** Disable the tick callback (clears UIE, keeps timer running). */
static inline void core_timer_disable_tick(core_timer_t *h)
{
    hal_timer_tick_disable(h);
}

/**
 * Convenience: initialize a timer as a tick-only source.
 * Sets up the timebase AND enables the update interrupt.
 * Use this when the timer's only job is a periodic callback.
 *
 *   core_tick_init(&t, TIM3, 500000, on_tick, NULL);  // 2 Hz
 *   core_timer_start(&t);
 */
static inline hal_status_t core_tick_init(core_timer_t *h,
                                           TIM_TypeDef *instance,
                                           uint32_t period_us,
                                           core_callback_t cb, void *ctx)
{
    hal_status_t rc = hal_timer_tick_init(h, instance, SYSCLK_HZ, period_us, cb, ctx);
    return rc;
}

#endif /* CORE_TIMER_H */
