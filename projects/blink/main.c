/**
 * Blink — LED blink using tilegen-generated board defines
 *
 * Uses tile_board.h for LED port/pin, so the same source
 * compiles for any Core tile with an onboard LED.
 */

#include <stdint.h>
#include "tile_board.h"

/* ---- Minimal register definitions ---- */
/* (These will move to the LL layer in Phase 3) */

#define PERIPH_BASE       0x40000000UL

/* GPIO register layout (common across all STM32 families) */
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
} GPIO_TypeDef;

/* GPIO port base addresses */
#define GPIOA  ((GPIO_TypeDef *)(PERIPH_BASE + 0x08000000UL + 0x0000UL))
#define GPIOB  ((GPIO_TypeDef *)(PERIPH_BASE + 0x08000000UL + 0x0400UL))

/* RCC — clock enable (chip-specific) */
#if defined(STM32L422xx)
  #define RCC_GPIOEN      (*(volatile uint32_t *)(PERIPH_BASE + 0x0002104CUL))
#elif defined(STM32L011xx)
  #define RCC_GPIOEN      (*(volatile uint32_t *)(PERIPH_BASE + 0x0002102CUL))
#elif defined(STM32WBA55xx)
  #define RCC_GPIOEN      (*(volatile uint32_t *)(PERIPH_BASE + 0x00020C8CUL))
#elif defined(STM32H523xx)
  #define RCC_GPIOEN      (*(volatile uint32_t *)(PERIPH_BASE + 0x04020C8CUL))
#else
  #error "Unknown MCU — add RCC GPIO clock enable address"
#endif

/* ---- Helpers ---- */

static void delay(volatile uint32_t count)
{
    while (count--)
        ;
}

static void gpio_clock_enable(GPIO_TypeDef *port)
{
    uint32_t index = ((uint32_t)port - (uint32_t)GPIOA) / 0x0400UL;
    RCC_GPIOEN |= (1UL << index);
    delay(10);
}

static void gpio_set_output(GPIO_TypeDef *port, uint32_t pin)
{
    uint32_t mask = 0x3UL << (pin * 2);
    uint32_t mode = 0x1UL << (pin * 2);
    port->MODER = (port->MODER & ~mask) | mode;
}

/* ---- Main ---- */

int main(void)
{
    gpio_clock_enable(LED_PORT);
    gpio_set_output(LED_PORT, LED_PIN);

    while (1) {
        LED_ON();
        delay(200000);
        LED_OFF();
        delay(200000);
    }
}
