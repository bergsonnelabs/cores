/**
 * ll_rcc.h — Low-level RCC (Reset and Clock Control)
 *
 * Peripheral clock enable/disable and basic clock source selection.
 * RCC register layouts differ significantly across STM32 families,
 * so this header provides a unified API with per-family implementations.
 */

#ifndef LL_RCC_H
#define LL_RCC_H

#include "ll_common.h"

/* ---- RCC base address ---- */

#if defined(STM32L011xx)
  #define RCC_BASE          (AHB_BASE + 0x1000UL)
#elif defined(STM32L422xx)
  #define RCC_BASE          (AHB1_BASE + 0x1000UL)
#elif defined(STM32WBA55xx)
  #define RCC_BASE          (AHB1_BASE + 0x0C00UL)
#elif defined(STM32H523xx)
  #define RCC_BASE          (AHB1_BASE + 0x0C00UL)
#endif

/* ---- GPIO clock enable ---- */

/**
 * Enable the clock for a GPIO port.
 * Must be called before accessing any GPIO registers.
 */
static inline void ll_rcc_gpio_clk_enable(GPIO_TypeDef *port)
{
    /* Compute port index from address offset (A=0, B=1, C=2, ...) */
    uint32_t index = ((uint32_t)port - (uint32_t)GPIOA) / 0x0400UL;

#if defined(STM32L011xx)
    /* L0: RCC_IOPENR at offset 0x2C */
    SET_BITS(REG32(RCC_BASE + 0x2CUL), (1UL << index));

#elif defined(STM32L422xx)
    /* L4: RCC_AHB2ENR at offset 0x4C */
    SET_BITS(REG32(RCC_BASE + 0x4CUL), (1UL << index));

#elif defined(STM32WBA55xx)
    /* WBA: RCC_AHB2ENR at offset 0x8C */
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << index));

#elif defined(STM32H523xx)
    /* H5: RCC_AHB2ENR at offset 0x8C */
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << index));
#endif

    /* Read-back to ensure the clock is stable before returning */
    (void)REG32(RCC_BASE);
    (void)REG32(RCC_BASE);
}

/**
 * Disable the clock for a GPIO port.
 */
static inline void ll_rcc_gpio_clk_disable(GPIO_TypeDef *port)
{
    uint32_t index = ((uint32_t)port - (uint32_t)GPIOA) / 0x0400UL;

#if defined(STM32L011xx)
    CLR_BITS(REG32(RCC_BASE + 0x2CUL), (1UL << index));
#elif defined(STM32L422xx)
    CLR_BITS(REG32(RCC_BASE + 0x4CUL), (1UL << index));
#elif defined(STM32WBA55xx)
    CLR_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << index));
#elif defined(STM32H523xx)
    CLR_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << index));
#endif
}

/* ---- Peripheral clock enable (by bus) ---- */

/* APB1 peripherals */
static inline void ll_rcc_apb1_clk_enable(uint32_t mask)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x38UL), mask);  /* RCC_APB1ENR */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x58UL), mask);  /* RCC_APB1ENR1 */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x9CUL), mask);  /* RCC_APB1ENR1 */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x9CUL), mask);  /* RCC_APB1LENR */
#endif
    (void)REG32(RCC_BASE);
}

/* APB2 peripherals */
static inline void ll_rcc_apb2_clk_enable(uint32_t mask)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x34UL), mask);  /* RCC_APB2ENR */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x60UL), mask);  /* RCC_APB2ENR */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xA4UL), mask);  /* RCC_APB2ENR */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xA4UL), mask);  /* RCC_APB2ENR */
#endif
    (void)REG32(RCC_BASE);
}

/* Common APB1 peripheral clock enable bits (vary by family but often similar) */
#if defined(STM32L422xx)
  #define LL_APB1_TIM2      (1UL << 0)
  #define LL_APB1_USART2    (1UL << 17)
  #define LL_APB1_I2C1      (1UL << 21)
  #define LL_APB1_I2C3      (1UL << 23)
  #define LL_APB1_USB       (1UL << 26)
  #define LL_APB2_TIM1      (1UL << 11)
  #define LL_APB2_SPI1      (1UL << 12)
  #define LL_APB2_USART1    (1UL << 14)
  #define LL_APB2_TIM15     (1UL << 16)
  #define LL_APB2_TIM16     (1UL << 17)
#elif defined(STM32L011xx)
  #define LL_APB1_TIM2      (1UL << 0)
  #define LL_APB1_USART2    (1UL << 17)
  #define LL_APB1_I2C1      (1UL << 21)
  #define LL_APB1_LPTIM1    (1UL << 31)
  #define LL_APB2_TIM21     (1UL << 2)
  #define LL_APB2_SPI1      (1UL << 12)
#endif

#endif /* LL_RCC_H */
