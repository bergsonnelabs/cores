/**
 * Blink — bare-metal LED blink for Core.U.2
 *
 * Toggles the onboard LED (PA8, active-high) using direct register
 * access. No HAL, no CMSIS device headers — just the reference manual
 * and the datasheet.
 *
 * This is the "prove the toolchain works" starting point.
 */

#include <stdint.h>

/* ---- Base addresses (from STM32L4 reference manual RM0394) ---- */

#define PERIPH_BASE       0x40000000UL
#define AHB2_BASE         (PERIPH_BASE + 0x08000000UL)
#define APB2_BASE         (PERIPH_BASE + 0x00010000UL)

/* RCC — Reset and Clock Control */
#define RCC_BASE          (PERIPH_BASE + 0x00021000UL)
#define RCC_AHB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x4CUL))

/* GPIO Port A */
#define GPIOA_BASE        (AHB2_BASE + 0x0000UL)
#define GPIOA_MODER       (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_ODR         (*(volatile uint32_t *)(GPIOA_BASE + 0x14UL))
#define GPIOA_BSRR        (*(volatile uint32_t *)(GPIOA_BASE + 0x18UL))

/* ---- Bit definitions ---- */

#define RCC_AHB2ENR_GPIOAEN   (1UL << 0)   /* GPIOA clock enable */

/* PA8 — LED pin */
#define LED_PIN           8
#define LED_MODER_MASK    (0x3UL << (LED_PIN * 2))
#define LED_MODER_OUTPUT  (0x1UL << (LED_PIN * 2))
#define LED_SET           (1UL << LED_PIN)
#define LED_RESET         (1UL << (LED_PIN + 16))

/* ---- Simple delay ---- */

static void delay(volatile uint32_t count)
{
    while (count--)
        ;
}

/* ---- Main ---- */

int main(void)
{
    /* Enable GPIOA clock */
    RCC_AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* Small delay for clock to stabilize */
    delay(10);

    /* Configure PA8 as general-purpose output (MODER = 01) */
    GPIOA_MODER = (GPIOA_MODER & ~LED_MODER_MASK) | LED_MODER_OUTPUT;

    /* Blink forever */
    while (1) {
        GPIOA_BSRR = LED_SET;       /* LED on */
        delay(200000);
        GPIOA_BSRR = LED_RESET;     /* LED off */
        delay(200000);
    }

    return 0;  /* never reached */
}
