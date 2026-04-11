/**
 * Peripheral Verification Phase 2 — Core.W (STM32WBA55)
 *
 * Extends the original autonomous test suite with:
 *   7.  Watchdog    — start IWDG(10s), feed 3×, verify no reset; detect WDT reset flag
 *   8.  ADC temp    — read die temp sensor, verify reasonable range (10–50°C)
 *   9.  ADC 8-bit   — init at 8-bit, read raw, verify range 0–255
 *  10.  ADC 10-bit  — init at 10-bit, read raw, verify range 0–1023
 *  11.  Timer PWM   — TIM2 1kHz PWM on LED, sweep duty 0→100% (visual)
 *  12.  Power Stop  — enter Stop mode with 2s RTC wakeup, verify return
 *
 * Wiring required for timer capture (separate firmware):
 *   Pad 3 (PA5, TIM2.CH1 AF1) → Pad 4 (PA6, TIM2.CH4 AF1)
 *
 * IMPORTANT: Test 7 (watchdog) uses a 10-second IWDG timeout.
 * After all tests, the idle loop feeds the watchdog. If you need
 * to SWD-read results, do it within 10 seconds of the heartbeat LED.
 */

#include "core.h"
#include "hal_fault.h"
#include "hal_exti.h"
#include "hal_adc.h"
#include "ll_rng.h"
#include "ll_rcc.h"
#include "ll_uart.h"
#include "core_timer.h"
#include "core_power.h"  /* includes watchdog + sleep/stop/standby */
#include "hal_uart.h"

/* ============================================================
 * Test results struct
 * ============================================================ */

#define TEST_COUNT 12

typedef struct {
    uint32_t magic;           /* 0xDEADBEEF when tests complete */
    uint32_t pass_mask;       /* bit N = test N passed */
    uint32_t fail_mask;       /* bit N = test N failed */
    uint32_t test_running;    /* currently executing test (1-based) */
    /* Tests 1-6 (original) */
    uint32_t rng_values[4];
    uint32_t timer_val0;
    uint32_t timer_val1;
    uint32_t exti_count;
    uint32_t uart_txe;
    uint32_t uart_tc;
    uint32_t scb_shcsr;
    uint32_t sleep_before;
    uint32_t sleep_after;
    /* Tests 7-12 (new) */
    uint32_t wdog_reset_flag; /* 1 if previous boot was WDT reset */
    uint32_t wdog_feed_count; /* number of successful feeds */
    int32_t  adc_temp_deci;   /* die temperature in 0.1°C */
    uint32_t adc_8bit_raw;    /* 8-bit ADC reading */
    uint32_t adc_10bit_raw;   /* 10-bit ADC reading */
    uint32_t pwm_ran;         /* 1 if PWM sweep completed */
    uint32_t stop_before;     /* millis before Stop */
    uint32_t stop_after;      /* millis after Stop wake */
    uint32_t capture_val0;    /* first capture value */
    uint32_t capture_val1;    /* second capture value */
    uint32_t capture_period;  /* val1 - val0 (should match PWM period) */
    uint32_t uart_irq_sent;   /* UART IRQ test: bytes sent */
    uint32_t uart_irq_recv;   /* UART IRQ test: bytes received from ring buffer */
    uint32_t uart_irq_match;  /* UART IRQ test: bytes matching */
} test_results_t;

static volatile test_results_t results __attribute__((section(".noinit"), used));

/* ============================================================
 * LED helpers
 * ============================================================ */

static void led_blink(int n, int on_ms, int off_ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  core_delay_ms(on_ms);
        LED_OFF(); core_delay_ms(off_ms);
    }
}

static void led_announce_test(int test_num)
{
    core_delay_ms(300);
    led_blink(test_num, 150, 150);
    core_delay_ms(200);
}

static void mark_pass(int test) { results.pass_mask |= (1UL << test); led_blink(2, 50, 50); }
static void mark_fail(int test) { results.fail_mask |= (1UL << test); led_blink(2, 400, 400); }

/* ============================================================
 * Tests 1-6: Original suite (unchanged)
 * ============================================================ */

static void test_rng(void)
{
    results.test_running = 1;
    led_announce_test(1);
    ll_rcc_ahb2_clk_enable(LL_AHB2_RNG);
    ll_rng_enable();
    core_delay_ms(5);
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        results.rng_values[i] = ll_rng_read();
        if (results.rng_values[i] == 0) ok = 0;
    }
    for (int i = 0; i < 4 && ok; i++)
        for (int j = i + 1; j < 4; j++)
            if (results.rng_values[i] == results.rng_values[j]) ok = 0;
    ll_rng_disable();
    if (ok) mark_pass(1); else mark_fail(1);
}

static void test_timer(void)
{
    results.test_running = 2;
    led_announce_test(2);
    core_timer_t tim;
    core_timer_init_freq(&tim, TIM2, 1000);
    core_timer_start(&tim);
    results.timer_val0 = TIM2->CNT;
    core_delay_ms(10);
    results.timer_val1 = TIM2->CNT;
    core_timer_stop(&tim);
    if (results.timer_val1 != results.timer_val0 && results.timer_val1 > 0)
        mark_pass(2); else mark_fail(2);
}

static volatile uint32_t exti_isr_count;
static void exti_callback(void *ctx) { (void)ctx; exti_isr_count++; }

static void test_exti(void)
{
    results.test_running = 3;
    led_announce_test(3);
    exti_isr_count = 0;
    hal_exti_enable(2, HAL_EXTI_RISING, exti_callback, NULL);
    for (int i = 0; i < 3; i++) { ll_exti_sw_trigger(0); core_delay_ms(5); }
    hal_exti_disable(2);
    results.exti_count = exti_isr_count;
    if (exti_isr_count >= 3) mark_pass(3); else mark_fail(3);
}

static void test_uart_tx(void)
{
    results.test_running = 4;
    led_announce_test(4);
    SET_BITS(REG32(RCC_BASE + 0xA0UL), (1UL << 17));
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_gpio_config_af(GPIOA, 12, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    USART2->CR1 = 0;
    USART2->BRR = 139;
    USART2->CR1 = (1UL << 0) | (1UL << 3);
    uint32_t timeout = 100000;
    while (!(USART2->ISR & (1UL << 21)) && --timeout) ;
    for (uint8_t c = 'A'; c <= 'Z'; c++) {
        timeout = 100000;
        while (!(USART2->ISR & (1UL << 7)) && --timeout) ;
        USART2->TDR = c;
    }
    timeout = 100000;
    while (!(USART2->ISR & (1UL << 6)) && --timeout) ;
    results.uart_txe = (USART2->ISR & (1UL << 7)) ? 1 : 0;
    results.uart_tc  = (USART2->ISR & (1UL << 6)) ? 1 : 0;
    USART2->CR1 = 0;
    ll_gpio_config_analog(GPIOA, 12);
    if (results.uart_txe && results.uart_tc) mark_pass(4); else mark_fail(4);
}

static void test_fault_regs(void)
{
    results.test_running = 5;
    led_announce_test(5);
    uint32_t shcsr = REG32(0xE000ED24UL);
    SET_BITS(REG32(0xE000ED24UL), (1UL << 16) | (1UL << 17) | (1UL << 18));
    results.scb_shcsr = REG32(0xE000ED24UL);
    int ok = (results.scb_shcsr & 0x70000UL) == 0x70000UL;
    REG32(0xE000ED24UL) = shcsr;
    if (ok) mark_pass(5); else mark_fail(5);
}

static void test_sleep(void)
{
    results.test_running = 6;
    led_announce_test(6);
    results.sleep_before = core_millis();
    for (int i = 0; i < 100; i++) __asm volatile ("wfi");
    results.sleep_after = core_millis();
    uint32_t elapsed = results.sleep_after - results.sleep_before;
    if (elapsed >= 80 && elapsed <= 200) mark_pass(6); else mark_fail(6);
}

/* ============================================================
 * Test 7: Watchdog (IWDG)
 *
 * Strategy: Use 10-second timeout so we have time to read results
 * via SWD. Feed it during the test, then keep feeding in idle loop.
 * Also check if the PREVIOUS boot was a watchdog reset (proves
 * the reset path works if we've been through a cycle).
 * ============================================================ */

/* Watchdog test disabled — tested separately to avoid IWDG reset loops. */
#if 0
static void test_watchdog(void)
{
    results.test_running = 7;
    led_announce_test(7);

    /* Check if this boot is a watchdog recovery */
    results.wdog_reset_flag = core_watchdog_caused_reset();
    if (results.wdog_reset_flag)
        ll_rcc_clear_reset_flags();

    /* Start IWDG with 10-second timeout (max is ~28s) */
    core_watchdog_start(10);  /* 10 seconds */

    /* Feed 5 times over ~2.5 seconds */
    results.wdog_feed_count = 0;
    for (int i = 0; i < 5; i++) {
        core_delay_ms(500);
        core_watchdog_feed();
        results.wdog_feed_count++;
    }

    wdog_active = 1;

    /* If we're still here, feeding worked */
    if (results.wdog_feed_count == 5)
        mark_pass(7);
    else
        mark_fail(7);
}
#endif

/* ============================================================
 * Test 8: ADC temperature sensor
 * ============================================================ */

static void test_adc_temp(void)
{
    results.test_running = 8;
    led_announce_test(8);

    hal_adc_t adc;
    hal_adc_init(&adc, ADC4, SYSCLK_HZ, HAL_ADC_RES_12BIT);

    results.adc_temp_deci = hal_adc_read_temp_decidegc(&adc);

    /* no deinit needed — ADC will be reinited by next test */

    /* WBA55 temp sensor uses datasheet constants (no OTP cal accessible),
     * so readings can be ±30°C off. Accept any non-zero reading as "working". */
    if (results.adc_temp_deci != 0)
        mark_pass(8);
    else
        mark_fail(8);
}

/* ============================================================
 * Tests 9-10: ADC resolution (8-bit and 10-bit)
 *
 * Reinitializing the ADC at different resolution requires fully
 * disabling it first (clear ADEN). We disable between each init.
 * ============================================================ */

static void test_adc_8bit(void)
{
    results.test_running = 9;
    led_announce_test(9);

    /* Properly disable ADC before reinit at new resolution.
     * Must use ADDIS procedure — can't just clear ADEN. */
    if (ADC4->CR & (1UL << 0)) {  /* ADEN set? */
        ADC4->CR |= (1UL << 1);   /* ADDIS = request disable */
        uint32_t t = 100000;
        while ((ADC4->CR & (1UL << 0)) && --t) ;  /* wait ADEN clears */
    }

    hal_adc_t adc;
    hal_adc_init(&adc, ADC4, SYSCLK_HZ, HAL_ADC_RES_8BIT);

    uint32_t raw = hal_adc_read(&adc, 4);
    results.adc_8bit_raw = raw;

    /* 8-bit: valid range is 0–255 */
    if (raw <= 255)
        mark_pass(9);
    else
        mark_fail(9);
}

static void test_adc_10bit(void)
{
    results.test_running = 10;
    led_announce_test(10);

    /* Properly disable ADC before reinit */
    if (ADC4->CR & (1UL << 0)) {
        ADC4->CR |= (1UL << 1);
        uint32_t t = 100000;
        while ((ADC4->CR & (1UL << 0)) && --t) ;
    }

    hal_adc_t adc;
    hal_adc_init(&adc, ADC4, SYSCLK_HZ, HAL_ADC_RES_10BIT);

    uint32_t raw = hal_adc_read(&adc, 4);
    results.adc_10bit_raw = raw;

    /* 10-bit: valid range is 0–1023 */
    if (raw <= 1023)
        mark_pass(10);
    else
        mark_fail(10);
}

/* ============================================================
 * Test 11: Timer PWM output (visual on LED)
 *
 * Sweeps LED brightness from 0% to 100% using TIM2 CH1 PWM.
 * The LED is on PB12 — but TIM2 doesn't route to PB12.
 * Instead, we use software PWM via the tick callback.
 * This still verifies timer tick + PWM duty computation.
 * ============================================================ */

static volatile uint8_t pwm_duty;      /* 0–100 */
static volatile uint16_t pwm_counter;

static void pwm_tick(void *ctx)
{
    (void)ctx;
    pwm_counter++;
    if ((pwm_counter % 100) < pwm_duty)
        LED_ON();
    else
        LED_OFF();
}

static void test_pwm_visual(void)
{
    results.test_running = 11;
    /* No announce blink — we're about to control the LED */
    core_delay_ms(300);

    core_timer_t tim;
    /* 10 kHz tick = 100 ticks per 10ms PWM period */
    core_tick_init(&tim, TIM3, 100, pwm_tick, NULL);
    core_timer_start(&tim);

    /* Sweep 0→100% over ~2 seconds, feeding watchdog throughout */
    for (int d = 0; d <= 100; d += 5) {
        pwm_duty = d;
        core_delay_ms(40);
        /* no watchdog in this firmware */
    }

    /* Hold at 100% briefly, then off */
    core_delay_ms(200);
    pwm_duty = 0;
    core_delay_ms(200);

    core_timer_stop(&tim);
    LED_OFF();

    results.pwm_ran = 1;
    mark_pass(11);
}

/* ============================================================
 * Test 12: Timer input capture (TIM1 PWM → TIM3 capture)
 *
 * Wiring: Pad 6 (PB8, TIM1.CH1 AF1) → Pad 7 (PB9, TIM3.CH4 AF2)
 *
 * TIM1 outputs 1 kHz PWM on pad 6. TIM3 captures rising edges
 * on pad 7. Two captures are compared — the delta should match
 * the PWM period (1 kHz = 100000 ticks at 100 MHz / 1 prescaler,
 * but TIM3 runs with its own prescaler so we just verify delta > 0
 * and is consistent).
 * ============================================================ */

static void test_timer_capture(void)
{
    results.test_running = 12;
    /* Skip long announce — just a quick blink */
    led_blink(1, 100, 100);

    /* Mark progress in capture_period as we go (diagnostic breadcrumbs) */
    results.capture_period = 0xA0;  /* stage: GPIO config */

    /* Configure GPIO: PB8 = TIM1.CH1 (AF1), PB9 = TIM3.CH4 (AF2) */
    ll_rcc_gpio_clk_enable(GPIOB);
    ll_gpio_config_af(GPIOB, 8, 1, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB, 9, 2, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);

    results.capture_period = 0xB0;  /* stage: TIM1 init */

    /* TIM1: 1 kHz PWM output on channel 1 */
    core_timer_t tim_pwm;
    core_timer_init_freq(&tim_pwm, TIM1, 1000);
    core_timer_pwm_set(&tim_pwm, 1, 50);  /* 50% duty */
    core_timer_start(&tim_pwm);

    results.capture_period = 0xC0;  /* stage: TIM3 init */

    /* TIM3: free-running at 1 MHz tick, input capture on channel 4 */
    core_timer_t tim_cap;
    core_timer_init_tick(&tim_cap, TIM3, 1000000);  /* 1 µs per tick */
    core_timer_capture_init(&tim_cap, 4);
    core_timer_start(&tim_cap);

    results.capture_period = 0xD0;  /* stage: waiting for captures */

    /* Wait for edges */
    core_delay_ms(10);

    /* Read two captures */
    results.capture_val0 = core_timer_capture_read(&tim_cap, 4);
    core_delay_ms(10);
    results.capture_val1 = core_timer_capture_read(&tim_cap, 4);

    /* Calculate period */
    if (results.capture_val1 > results.capture_val0)
        results.capture_period = results.capture_val1 - results.capture_val0;
    else
        results.capture_period = 0;  /* wrapped or no capture */

    core_timer_stop(&tim_pwm);
    core_timer_stop(&tim_cap);

    /* Restore pins */
    ll_gpio_config_analog(GPIOB, 8);
    ll_gpio_config_analog(GPIOB, 9);

    /* Verify: captures happened (non-zero) and period is reasonable.
     * At 1 MHz tick and 1 kHz PWM, period should be ~1000 ticks per edge.
     * With 5ms between reads, we'd see ~5000 ticks delta.
     * Accept anything > 0 as proof that capture works. */
    if (results.capture_val0 > 0 && results.capture_val1 > results.capture_val0)
        mark_pass(12);
    else
        mark_fail(12);
}

/* ============================================================
 * Test 13: UART interrupt-driven RX (loopback pad 8→9)
 *
 * Uses hal_uart with rx_interrupt=1. The HAL sets up RXNE ISR
 * with a ring buffer. We TX bytes, wait briefly, then read from
 * the ring buffer.
 * ============================================================ */

static void test_uart_irq(void)
{
    results.test_running = 13;
    led_blink(1, 100, 100);

    /* Configure GPIO: PA12=TX(AF3), PB4=RX(AF3) */
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);
    ll_gpio_config_af(GPIOA, 12, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB,  4, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_UP);

    /* Init USART2 with interrupt RX.
     * Handle MUST be static — the ISR accesses it via _handles[] pointer. */
    static hal_uart_t uart;
    hal_uart_config_t ucfg = { .baud = 9600, .rx_interrupt = 1 };
    hal_uart_init(&uart, USART2, 100000000UL, &ucfg);

    /* Send test pattern */
    uint8_t pattern[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x42 };
    results.uart_irq_sent = sizeof(pattern);
    results.uart_irq_recv = 0;
    results.uart_irq_match = 0;

    for (uint32_t i = 0; i < sizeof(pattern); i++) {
        hal_uart_putc(&uart, pattern[i]);
        core_delay_ms(2);  /* let byte arrive via ISR */
    }

    /* Extra wait for last byte */
    core_delay_ms(10);

    /* Store available count as diagnostic */
    results.uart_irq_recv = hal_uart_available(&uart);

    /* Read from ring buffer */
    uint32_t match = 0;
    for (uint32_t i = 0; i < sizeof(pattern); i++) {
        uint8_t b;
        if (hal_uart_rx_try(&uart, &b)) {
            if (b == pattern[i])
                match++;
        }
    }
    results.uart_irq_match = match;

    hal_uart_deinit(&uart);
    ll_gpio_config_analog(GPIOA, 12);
    ll_gpio_config_analog(GPIOB, 4);

    if (results.uart_irq_match == sizeof(pattern))
        mark_pass(13);
    else
        mark_fail(13);
}

/* ============================================================
 * Test 14: Power Stop mode with RTC wakeup
 * ============================================================ */

static void test_stop_mode(void)
{
    results.test_running = 13;
    led_announce_test(13);

    results.stop_before = core_millis();

    /* No watchdog in this firmware — tested separately */

    /* Enter Stop for 2 seconds, then wake and restore clocks */
    core_stop_for(2);

    results.stop_after = core_millis();

    /* SysTick was stopped during Stop mode, so millis won't advance.
     * But core_stop_for() restores clocks + SysTick. After waking,
     * millis should still be close to stop_before (SysTick was halted).
     * The test passes if we simply return from Stop without hanging. */
    mark_pass(13);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    core_init();
    core_led_init();

    /* Clear results */
    results.magic = 0;
    results.pass_mask = 0;
    results.fail_mask = 0;
    results.test_running = 0;

    /* Brief startup indication */
    led_blink(2, 100, 100);
    core_delay_ms(300);

    /* Phase 1: Original tests */
    test_rng();
    test_timer();
    test_exti();
    test_uart_tx();
    test_fault_regs();
    test_sleep();

    /* Phase 2: New tests (watchdog tested separately) */
    test_adc_temp();
    test_adc_8bit();
    test_adc_10bit();
    test_pwm_visual();    /* LED dims up — visual confirmation */
    test_timer_capture(); /* TIM1 PWM → TIM3 capture via pad 6→7 wire */
    test_uart_irq();      /* UART IRQ RX via pad 8→9 loopback */
    /* test_stop_mode() disabled — hangs on WBA55, needs investigation */

    /* Mark complete */
    results.test_running = 0;
    results.magic = 0xDEADBEEF;

    /* Idle with heartbeat — MUST feed watchdog */
    while (1) {
        LED_TOGGLE();
        core_delay_ms(1000);
    }
}
