/**
 * Wire + UART + SPI Loopback Diagnostic — Core.W
 *
 * Phase 1: GPIO wire check (PA12 output → PB4 input, no AF)
 * Phase 2: UART loopback (USART2: PA12 TX → PB4 RX)
 * Phase 3: SPI loopback (SPI3: PB8 MOSI → PB9 MISO)
 */

#include "core.h"
#include "hal_spi.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_uart.h"
#include "hal_uart.h"

typedef struct {
    uint32_t magic;
    /* Phase 1: GPIO wire test (pad 8→9) */
    uint32_t wire_high;       /* PB4 reads 1 when PA12 driven high */
    uint32_t wire_low;        /* PB4 reads 0 when PA12 driven low */
    /* Phase 1b: GPIO wire test (pad 6→7) */
    uint32_t wire2_high;      /* PB9 reads 1 when PB8 driven high */
    uint32_t wire2_low;       /* PB9 reads 0 when PB8 driven low */
    /* Phase 2: UART */
    uint32_t uart_pass;
    uint32_t uart_sent;
    uint32_t uart_received;
    uint32_t uart_match;
    uint32_t uart_isr;        /* ISR after first byte TX+wait */
    /* Phase 3: SPI */
    uint32_t spi_pass;
    uint32_t spi_sent;
    uint32_t spi_received;
    uint32_t spi_match;
    uint32_t spi_sr;
} diag_results_t;

static volatile diag_results_t R __attribute__((section(".noinit"), used));

static void led_blink(int n, int on_ms, int off_ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  core_delay_ms(on_ms);
        LED_OFF(); core_delay_ms(off_ms);
    }
}

/* ============================================================
 * Phase 1: GPIO wire test — is pad 8 physically connected to pad 9?
 * ============================================================ */
static void test_wire(void)
{
    led_blink(1, 200, 200);

    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);

    /* PA12 = push-pull output, PB4 = input with pull-down */
    ll_gpio_config_output(GPIOA, 12);
    ll_gpio_set_mode(GPIOB, 4, LL_GPIO_MODE_INPUT);
    ll_gpio_set_pull(GPIOB, 4, LL_GPIO_PULL_DOWN);

    /* Drive PA12 high, read PB4 */
    ll_gpio_set(GPIOA, (1UL << 12));
    core_delay_ms(1);
    R.wire_high = (GPIOB->IDR & (1UL << 4)) ? 1 : 0;

    /* Drive PA12 low, read PB4 */
    ll_gpio_clear(GPIOA, (1UL << 12));
    core_delay_ms(1);
    R.wire_low = (GPIOB->IDR & (1UL << 4)) ? 1 : 0;

    /* Restore to analog */
    ll_gpio_config_analog(GPIOA, 12);
    ll_gpio_config_analog(GPIOB, 4);

    if (R.wire_high && !R.wire_low)
        led_blink(3, 50, 50);   /* pass */
    else
        led_blink(3, 500, 500); /* fail */

    /* Wire test 2: pad 6 (PB8) → pad 7 (PB9) */
    core_delay_ms(300);

    ll_gpio_config_output(GPIOB, 8);
    ll_gpio_set_mode(GPIOB, 9, LL_GPIO_MODE_INPUT);
    ll_gpio_set_pull(GPIOB, 9, LL_GPIO_PULL_DOWN);

    ll_gpio_set(GPIOB, (1UL << 8));
    core_delay_ms(1);
    R.wire2_high = (GPIOB->IDR & (1UL << 9)) ? 1 : 0;

    ll_gpio_clear(GPIOB, (1UL << 8));
    core_delay_ms(1);
    R.wire2_low = (GPIOB->IDR & (1UL << 9)) ? 1 : 0;

    ll_gpio_config_analog(GPIOB, 8);
    ll_gpio_config_analog(GPIOB, 9);

    if (R.wire2_high && !R.wire2_low)
        led_blink(3, 50, 50);   /* pass */
    else
        led_blink(3, 500, 500); /* fail */
}

/* ============================================================
 * Phase 2: UART loopback (PA12 TX → PB4 RX, USART2, AF3)
 * ============================================================ */
static void test_uart(void)
{
    core_delay_ms(300);
    led_blink(2, 200, 200);

    /* Configure GPIO: PA12=TX(AF3), PB4=RX(AF3) */
    ll_gpio_config_af(GPIOA, 12, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB,  4, 3, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_UP);

    /* Use HAL driver — it handles clock enable and BRR correctly */
    hal_uart_t uart;
    hal_uart_config_t ucfg = { .baud = 9600, .rx_interrupt = 0 };
    hal_uart_init(&uart, USART2, 100000000UL, &ucfg);

    /* Also try: explicitly read USART2->BRR to see what was set */
    R.uart_isr = USART2->BRR;  /* repurpose: store BRR for diagnostics */

    uint8_t pattern[] = { 0xAA, 0x55, 0x01, 0xFE, 0x42 };
    R.uart_sent = sizeof(pattern);
    R.uart_received = 0;
    R.uart_match = 0;

    for (uint32_t i = 0; i < sizeof(pattern); i++) {
        /* Blocking TX via HAL */
        hal_uart_putc(&uart, pattern[i]);

        /* Wait for byte to fully shift out and back in */
        core_delay_ms(5);

        /* Check RXNE (bit 5) */
        if (USART2->ISR & (1UL << 5)) {
            uint8_t rx = (uint8_t)USART2->RDR;
            R.uart_received++;
            if (rx == pattern[i])
                R.uart_match++;
        }
    }

    hal_uart_deinit(&uart);
    ll_gpio_config_analog(GPIOA, 12);
    ll_gpio_config_analog(GPIOB, 4);

    R.uart_pass = (R.uart_match == R.uart_sent) ? 1 : 0;
    led_blink(R.uart_pass ? 3 : 3, R.uart_pass ? 50 : 500, R.uart_pass ? 50 : 500);
}

/* ============================================================
 * Phase 3: SPI loopback (SPI3: CLK=PA0, MOSI=PB8, MISO=PB9, AF6)
 * ============================================================ */
static void test_spi(void)
{
    core_delay_ms(300);
    led_blink(3, 200, 200);

    ll_gpio_config_af(GPIOA, 0, 6, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB, 8, 6, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOB, 9, 6, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_HIGH, LL_GPIO_PULL_NONE);

    hal_spi_t spi;
    hal_spi_config_t cfg = { .prescaler = 5, .cpol = 0, .cpha = 0 };
    hal_spi_init(&spi, SPI3, &cfg);

    /* Instrumented single-byte SPI v2 transfer with full diagnostics.
     * Repurpose result fields for register snapshots. */
    R.spi_sr = SPI3->SR;         /* SR after init */
    R.spi_sent = SPI3->CR1;      /* CR1 after init */

    /* SPI v2 transfer using TSIZE=0 (endless mode) — no CSTART needed.
     * In endless mode, the SPI clocks out data whenever TXDR is written. */
    SPI3->CR1 = 0;                                     /* Fully disable */
    SPI3->IFCR = 0x1FF;                                /* Clear all flags */
    SPI3->CR2 = 0;                                     /* TSIZE = 0 (endless) */
    SPI3->CR1 = (1UL << 12);                           /* SSI */
    SPI3->CR1 = (1UL << 12) | (1UL << 0);             /* SSI + SPE */
    SPI3->CR1 = (1UL << 12) | (1UL << 0) | (1UL << 9); /* SSI + SPE + CSTART */

    R.spi_sr = SPI3->SR;                               /* SR after CSTART */
    R.spi_sent = SPI3->CR1;                            /* CR1 readback */

    /* Write TX byte */
    *(volatile uint8_t *)&SPI3->TXDR = 0xA5;

    R.spi_received = SPI3->SR;                         /* SR after TXDR write */

    /* Wait for RXP with timeout */
    uint32_t t = 500000;
    while (!(SPI3->SR & (1UL << 0)) && --t) ;

    if (t > 0) {
        uint8_t rx = *(volatile uint8_t *)&SPI3->RXDR;
        t = 100000;
        while (!(SPI3->SR & (1UL << 3)) && --t) ;
        SPI3->IFCR = 0x1FF;
        R.spi_pass = (rx == 0xA5) ? 0xA500 | rx : rx;  /* encode result */
    } else {
        R.spi_pass = 0xDEAD;  /* timeout */
    }

    hal_spi_deinit(&spi);
    ll_gpio_config_analog(GPIOA, 0);
    ll_gpio_config_analog(GPIOB, 8);
    ll_gpio_config_analog(GPIOB, 9);

    led_blink(3, (R.spi_pass == 0xA5A5) ? 50 : 500,
                  (R.spi_pass == 0xA5A5) ? 50 : 500);
}

int main(void)
{
    core_init();
    core_led_init();

    R.magic = 0;
    led_blink(2, 100, 100);
    core_delay_ms(500);

    test_wire();
    test_uart();
    test_spi();

    R.magic = 0xDEADBEEF;

    while (1) {
        LED_TOGGLE();
        core_delay_ms(1000);
    }
}
