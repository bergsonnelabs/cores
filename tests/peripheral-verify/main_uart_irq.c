/**
 * Minimal UART IRQ loopback test — Core.W
 *
 * Wiring: Pad 8 (PA12, TX) → Pad 9 (PB4, RX)
 *
 * Uses HAL UART with IRQ RX. Sends 5 bytes, reads back from ring buffer.
 * LED: fast blink = pass, slow blink = fail, heartbeat = done.
 * Results in SRAM struct at &R.
 */

#include "core.h"
#include "hal_uart.h"

typedef struct {
    uint32_t magic;
    uint32_t sent;
    uint32_t available;
    uint32_t received;
    uint32_t match;
    uint32_t cr1_after_init;
    uint32_t isr_after_send;
    uint32_t overrun;
    uint32_t framing_err;
} uart_irq_results_t;

static volatile uart_irq_results_t R __attribute__((section(".noinit"), used));
static hal_uart_t uart;  /* static lifetime for ISR access */

int main(void)
{
    core_init();
    core_led_init();

    R.magic = 0;

    /* LED startup */
    LED_ON(); core_delay_ms(200); LED_OFF(); core_delay_ms(500);

    /* GPIO wire check first */
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);
    ll_gpio_config_output(GPIOA, 12);
    ll_gpio_set_mode(GPIOB, 4, LL_GPIO_MODE_INPUT);
    ll_gpio_set_pull(GPIOB, 4, LL_GPIO_PULL_DOWN);
    ll_gpio_set(GPIOA, (1UL << 12));
    core_delay_ms(1);
    R.overrun = (GPIOB->IDR & (1UL << 4)) ? 0xAA : 0x00;  /* 0xAA = wire OK */
    ll_gpio_clear(GPIOA, (1UL << 12));

    /* Now configure for UART AF */
    ll_gpio_config_af(GPIOA, 12, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB,  4, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_UP);

    /* Init USART2 — polling first to isolate the issue */
    hal_uart_config_t cfg = { .baud = 9600, .rx_interrupt = 0 };
    hal_uart_init(&uart, USART2, 100000000UL, &cfg);

    /* Capture CR1 after init (should show UE+TE+RE+RXNEIE) */
    R.cr1_after_init = USART2->CR1;

    /* Send each byte and immediately poll for RXNE */
    uint8_t pattern[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x42 };
    R.sent = sizeof(pattern);
    R.received = 0;
    R.match = 0;

    for (uint32_t i = 0; i < sizeof(pattern); i++) {
        hal_uart_putc(&uart, pattern[i]);

        /* Poll for RXNE (byte looped back) */
        uint32_t t = 500000;
        while (!(USART2->ISR & (1UL << 5)) && --t) ;
        if (t > 0) {
            uint8_t b = (uint8_t)USART2->RDR;
            R.received++;
            if (b == pattern[i]) R.match++;
        }
    }

    core_delay_ms(10);

    /* Capture ISR state */
    R.isr_after_send = USART2->ISR;
    R.available = hal_uart_available(&uart);
    R.overrun = uart.rx_overrun;
    R.framing_err = uart.rx_framing_err;

    /* Read back from ring buffer */
    R.received = 0;
    R.match = 0;
    for (uint32_t i = 0; i < sizeof(pattern); i++) {
        uint8_t b;
        if (hal_uart_rx_try(&uart, &b)) {
            R.received++;
            if (b == pattern[i])
                R.match++;
        }
    }

    hal_uart_deinit(&uart);

    R.magic = 0xDEADBEEF;

    /* Result LED */
    if (R.match == R.sent)
        for (int i = 0; i < 5; i++) { LED_ON(); core_delay_ms(50); LED_OFF(); core_delay_ms(50); }
    else
        for (int i = 0; i < 5; i++) { LED_ON(); core_delay_ms(500); LED_OFF(); core_delay_ms(500); }

    /* Heartbeat */
    while (1) { LED_TOGGLE(); core_delay_ms(1000); }
}
