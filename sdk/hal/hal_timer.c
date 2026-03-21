/**
 * hal_timer.c — Timer HAL driver implementation
 */

#include "hal_timer.h"
#include "ll_rcc.h"
#include <string.h>

/* ============================================================
 * Handle registry for tick callbacks
 * ============================================================ */

#if defined(STM32L011xx)
  static hal_timer_t *_tim_handles[2];  /* TIM2, TIM21 */
#elif defined(STM32L422xx)
  static hal_timer_t *_tim_handles[4];  /* TIM1, TIM2, TIM15, TIM16 */
#elif defined(STM32WBA55xx)
  static hal_timer_t *_tim_handles[5];  /* TIM1, TIM2, TIM3, TIM16, TIM17 */
#elif defined(STM32H523xx)
  static hal_timer_t *_tim_handles[5];  /* TIM1, TIM2, TIM3, TIM6, TIM7 */
#endif

static int _tim_index(TIM_TypeDef *instance)
{
#if defined(STM32L011xx)
    if (instance == TIM2)  return 0;
    if (instance == TIM21) return 1;
#elif defined(STM32L422xx)
    if (instance == TIM1)  return 0;
    if (instance == TIM2)  return 1;
    if (instance == TIM15) return 2;
    if (instance == TIM16) return 3;
#elif defined(STM32WBA55xx)
    if (instance == TIM1)  return 0;
    if (instance == TIM2)  return 1;
    if (instance == TIM3)  return 2;
    if (instance == TIM16) return 3;
    if (instance == TIM17) return 4;
#elif defined(STM32H523xx)
    if (instance == TIM1)  return 0;
    if (instance == TIM2)  return 1;
    if (instance == TIM3)  return 2;
    if (instance == TIM6)  return 3;
    if (instance == TIM7)  return 4;
#endif
    return -1;
}

static uint32_t _tim_irq(TIM_TypeDef *instance)
{
#if defined(STM32L422xx)
    if (instance == TIM1)  return HAL_IRQ_TIM1_UP_16;
    if (instance == TIM2)  return HAL_IRQ_TIM2;
    if (instance == TIM15) return HAL_IRQ_TIM15;
    if (instance == TIM16) return HAL_IRQ_TIM1_UP_16;  /* Shared with TIM1 */
#elif defined(STM32WBA55xx)
    if (instance == TIM1)  return HAL_IRQ_TIM1_UP;
    if (instance == TIM2)  return HAL_IRQ_TIM2;
    if (instance == TIM3)  return HAL_IRQ_TIM3;
    if (instance == TIM16) return HAL_IRQ_TIM16;
    if (instance == TIM17) return HAL_IRQ_TIM17;
#elif defined(STM32H523xx)
    if (instance == TIM1)  return HAL_IRQ_TIM1_UP;
    if (instance == TIM2)  return HAL_IRQ_TIM2;
    if (instance == TIM3)  return HAL_IRQ_TIM3;
#endif
    return 0;
}

/* ============================================================
 * Timer ISR handler
 * ============================================================ */

static void _tim_isr(hal_timer_t *h)
{
    if (!h) return;
    if (ll_tim_update_flag(h->instance)) {
        if (h->tick_cb) {
            h->tick_cb(h->tick_ctx);
        }
    }
}

/* ---- ISR handlers (strong symbols) ---- */

#if defined(STM32L422xx)
void TIM1_UP_TIM16_IRQHandler(void)
{
    _tim_isr(_tim_handles[0]);  /* TIM1 */
    _tim_isr(_tim_handles[3]);  /* TIM16 (shared IRQ) */
}
#endif

void TIM2_IRQHandler(void)
{
#if defined(STM32L011xx)
    _tim_isr(_tim_handles[0]);
#else
    _tim_isr(_tim_handles[1]);
#endif
}

#if defined(STM32WBA55xx) || defined(STM32H523xx)
void TIM3_IRQHandler(void)
{
    _tim_isr(_tim_handles[2]);
}
#endif

/* ============================================================
 * PWM
 * ============================================================ */

/**
 * Calculate optimal PSC/ARR for a target frequency.
 *   pclk / (psc+1) / (arr+1) = freq
 * We want arr as large as possible for duty resolution.
 */
static void _calc_psc_arr(uint32_t pclk_hz, uint32_t freq_hz,
                           uint32_t *psc, uint32_t *arr)
{
    /* Target: arr around 1000 for good duty resolution */
    uint32_t total = pclk_hz / freq_hz;

    if (total <= 65536) {
        *psc = 0;
        *arr = total - 1;
    } else {
        *psc = (total / 65536);
        *arr = (total / (*psc + 1)) - 1;
    }
}

hal_status_t hal_timer_pwm_init(hal_timer_t *h, TIM_TypeDef *instance,
                                 uint32_t pclk_hz, uint32_t freq_hz)
{
    memset(h, 0, sizeof(*h));
    h->instance = instance;
    h->pclk_hz = pclk_hz;

    uint32_t psc, arr;
    _calc_psc_arr(pclk_hz, freq_hz, &psc, &arr);
    ll_tim_config(instance, psc, arr);

    /* Enable MOE for advanced timers (TIM1) */
    if (instance == TIM1) {
        ll_tim_enable_moe(instance);
    }

    return HAL_OK;
}

void hal_timer_pwm_set_duty(hal_timer_t *h, uint8_t channel,
                             uint16_t duty_permil)
{
    uint32_t arr = h->instance->ARR;
    uint32_t ccr = (arr * duty_permil) / 1000;
    ll_tim_pwm_config(h->instance, channel, ccr);
}

void hal_timer_pwm_set_freq(hal_timer_t *h, uint32_t freq_hz)
{
    uint32_t psc, arr;
    _calc_psc_arr(h->pclk_hz, freq_hz, &psc, &arr);
    h->instance->PSC = psc;
    h->instance->ARR = arr;
    h->instance->EGR = LL_TIM_EGR_UG;
}

void hal_timer_pwm_start(hal_timer_t *h)
{
    ll_tim_start(h->instance);
}

void hal_timer_pwm_stop(hal_timer_t *h)
{
    ll_tim_stop(h->instance);
}

/* ============================================================
 * Periodic tick
 * ============================================================ */

hal_status_t hal_timer_tick_init(hal_timer_t *h, TIM_TypeDef *instance,
                                 uint32_t pclk_hz, uint32_t period_us,
                                 hal_callback_t cb, void *ctx)
{
    int idx = _tim_index(instance);
    if (idx < 0) return HAL_ERROR;

    memset(h, 0, sizeof(*h));
    h->instance = instance;
    h->pclk_hz = pclk_hz;
    h->tick_cb = cb;
    h->tick_ctx = ctx;

    /* Calculate PSC/ARR for the requested period:
       pclk / (psc+1) gives us the tick rate
       (arr+1) ticks = one period */
    uint32_t ticks = (uint64_t)pclk_hz * period_us / 1000000UL;
    uint32_t psc = 0;
    uint32_t arr = ticks - 1;

    if (arr > 65535) {
        psc = ticks / 65536;
        arr = (ticks / (psc + 1)) - 1;
    }

    ll_tim_config(instance, psc, arr);

    /* Enable update interrupt */
    SET_BITS(instance->DIER, LL_TIM_DIER_UIE);

    /* Register handle and enable NVIC */
    _tim_handles[idx] = h;
    hal_nvic_set_priority(_tim_irq(instance), 0x30);
    hal_nvic_enable_irq(_tim_irq(instance));

    return HAL_OK;
}

void hal_timer_tick_start(hal_timer_t *h)
{
    ll_tim_start(h->instance);
}

void hal_timer_tick_stop(hal_timer_t *h)
{
    ll_tim_stop(h->instance);
}
