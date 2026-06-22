/**
 * Core.ST.L0 — Timer PWM + Capture via pad 2↔9
 *
 * Pad 2 = PA3 → TIM2.CH4 (AF2) PWM output
 * Pad 9 = PB0 → TIM2.CH2 (AF2) input capture
 *
 * Tests:
 *   1. PWM output verify — run PWM on PA3, read PB0 as GPIO, count toggles
 *   2. Input capture — PWM on CH4, capture on CH2, verify CCR2 updates
 *   3. Capture period — measure PWM period via two consecutive captures
 */

#include "core.h"
#include "core_timer.h"

typedef struct {
    uint32_t magic;
    uint16_t pass_mask;
    uint16_t fail_mask;
    /* Test 1: PWM output verify */
    uint32_t pwm_toggles;       /* number of PB0 transitions seen */
    /* Test 2+3: Capture */
    uint32_t cap0;
    uint32_t cap1;
    uint32_t cap_delta;
    uint32_t tim2_sr;           /* TIM2 SR after captures */
    uint32_t tim2_ccmr1;        /* CCMR1 (capture config) */
    uint32_t tim2_ccmr2;        /* CCMR2 (PWM config) */
    uint32_t tim2_ccer;         /* CCER (output/capture enables) */
} timer_results_t;

static volatile timer_results_t R __attribute__((section(".noinit"), used));

static void blink(int n, int ms) {
    for (int i = 0; i < n; i++) {
        LED_ON(); core_delay_ms(ms); LED_OFF(); core_delay_ms(ms);
    }
}
static void announce(int n) { core_delay_ms(300); blink(n, 150); core_delay_ms(200); }
static void pass(int t) { R.pass_mask |= (1U << t); blink(2, 50); }
static void fail(int t) { R.fail_mask |= (1U << t); blink(2, 400); }

/* ---- Test 1: PWM output verify via GPIO read ---- */
static void test_pwm_output(void)
{
    announce(1);

    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);

    /* PA3 = TIM2.CH4 (AF2) for PWM output */
    ll_gpio_config_af(GPIOA, 3, 2, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    /* PB0 = plain GPIO input (NOT timer AF yet) */
    ll_gpio_set_mode(GPIOB, 0, LL_GPIO_MODE_INPUT);
    ll_gpio_set_pull(GPIOB, 0, LL_GPIO_PULL_NONE);

    /* TIM2 at 100 Hz (slow enough to catch via polling at 2MHz CPU) */
    core_timer_t tim;
    core_timer_init_freq(&tim, TIM2, 100);
    core_timer_pwm_set(&tim, 4, 50);
    core_timer_start(&tim);

    /* Poll PB0 for ~50ms, count transitions */
    uint32_t toggles = 0;
    uint8_t last = (GPIOB->IDR & 1) ? 1 : 0;
    for (volatile uint32_t i = 0; i < 50000; i++) {
        uint8_t now = (GPIOB->IDR & 1) ? 1 : 0;
        if (now != last) {
            toggles++;
            last = now;
        }
    }

    core_timer_stop(&tim);
    R.pwm_toggles = toggles;

    ll_gpio_config_analog(GPIOA, 3);
    ll_gpio_config_analog(GPIOB, 0);

    /* At 100Hz PWM, ~50ms should give ~10 transitions */
    if (toggles >= 4) pass(1); else fail(1);
}

/* ---- Test 2: Input capture ---- */
static void test_capture(void)
{
    announce(2);

    /* PA3 = TIM2.CH4 (AF2) PWM, PB0 = TIM2.CH2 (AF2) capture */
    ll_gpio_config_af(GPIOA, 3, 2, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB, 0, 2, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);

    /* TIM2 at 100 Hz */
    core_timer_t tim;
    core_timer_init_freq(&tim, TIM2, 100);

    R.tim2_ccmr1 = TIM2->CCMR1;  /* diag: CCMR1 after init_freq (should be 0) */

    /* Configure CH2 for input capture on FALLING edge.
     * Rising edge captures CNT≈0 (PWM restarts). Falling edge
     * captures CNT≈CCR4 (mid-period for 50% duty). */
    core_timer_capture_init(&tim, 2);
    SET_BITS(TIM2->CCER, LL_TIM_CCER_CC2P);  /* CC2P=1 → falling edge */

    R.tim2_ccmr2 = TIM2->CCMR1;  /* diag: CCMR1 after capture init */

    /* Then PWM on CH4 */
    core_timer_pwm_set(&tim, 4, 50);

    R.tim2_ccer = TIM2->CCMR1;   /* diag: CCMR1 after PWM set (should still have CC2S=01) */

    core_timer_start(&tim);

    /* Wait for some captures */
    core_delay_ms(50);

    /* Clear capture flag, wait for fresh capture */
    TIM2->SR = 0;  /* clear all flags */
    core_delay_ms(20);

    R.cap0 = TIM2->CCR2;
    R.tim2_sr = TIM2->SR;

    /* Wait for next capture */
    core_delay_ms(20);
    R.cap1 = TIM2->CCR2;

    core_timer_stop(&tim);

    R.cap_delta = (R.cap1 > R.cap0) ? R.cap1 - R.cap0 : R.cap0 - R.cap1;

    ll_gpio_config_analog(GPIOA, 3);
    ll_gpio_config_analog(GPIOB, 0);

    /* Captures should differ if edges are being captured */
    if (R.cap0 != R.cap1 && R.cap0 > 0)
        pass(2);
    else
        fail(2);
}

int main(void)
{
    core_init();
    core_led_init();

    R.magic = 0;
    R.pass_mask = 0;
    R.fail_mask = 0;

    blink(2, 100);
    core_delay_ms(500);

    test_pwm_output();
    test_capture();

    R.magic = 0xDEADBEEF;

    while (1) { LED_TOGGLE(); core_delay_ms(1000); }
}
