/**
 * ll_common.h — Low-level common definitions
 *
 * Register access helpers, bit manipulation, and GPIO/peripheral
 * type definitions shared across all STM32 families.
 */

#ifndef LL_COMMON_H
#define LL_COMMON_H

#include <stdint.h>

/* ---- Register access ---- */

#define REG32(addr)         (*(volatile uint32_t *)(addr))
#define SET_BITS(reg, mask) ((reg) |= (mask))
#define CLR_BITS(reg, mask) ((reg) &= ~(mask))
#define MOD_BITS(reg, mask, val) ((reg) = ((reg) & ~(mask)) | (val))

/* ---- Peripheral base addresses ---- */

#define PERIPH_BASE         0x40000000UL

#if defined(STM32L011xx)
  #define AHB_BASE          (PERIPH_BASE + 0x00020000UL)
  #define IOPORT_BASE       (PERIPH_BASE + 0x10000000UL)
#elif defined(STM32L422xx)
  #define AHB1_BASE         (PERIPH_BASE + 0x00020000UL)
  #define AHB2_BASE         (PERIPH_BASE + 0x08000000UL)
  #define IOPORT_BASE       AHB2_BASE
#elif defined(STM32WBA55xx)
  #define AHB1_BASE         (PERIPH_BASE + 0x00020000UL)
  #define AHB2_BASE         (PERIPH_BASE + 0x02020000UL)
  #define AHB4_BASE         (PERIPH_BASE + 0x06020000UL)
  #define AHB5_BASE         (PERIPH_BASE + 0x06020000UL)  /* Same as AHB4 on WBA55 */
  #define IOPORT_BASE       (PERIPH_BASE + 0x02020000UL)
#elif defined(STM32H523xx)
  #define AHB1_BASE         (PERIPH_BASE + 0x04020000UL)
  #define AHB2_BASE         (PERIPH_BASE + 0x08020000UL)
  #define IOPORT_BASE       (PERIPH_BASE + 0x02020000UL)
#else
  #error "Unknown MCU — define one of: STM32L011xx, STM32L422xx, STM32WBA55xx, STM32H523xx"
#endif

/* ---- GPIO register structure ---- */
/* Common across all STM32 families */

typedef struct {
    volatile uint32_t MODER;       /* 0x00: Mode register */
    volatile uint32_t OTYPER;      /* 0x04: Output type register */
    volatile uint32_t OSPEEDR;     /* 0x08: Output speed register */
    volatile uint32_t PUPDR;       /* 0x0C: Pull-up/pull-down register */
    volatile uint32_t IDR;         /* 0x10: Input data register */
    volatile uint32_t ODR;         /* 0x14: Output data register */
    volatile uint32_t BSRR;        /* 0x18: Bit set/reset register */
    volatile uint32_t LCKR;        /* 0x1C: Lock register */
    volatile uint32_t AFR[2];      /* 0x20-0x24: Alternate function low/high */
    volatile uint32_t BRR;         /* 0x28: Bit reset register */
} GPIO_TypeDef;

/* GPIO port instances */
#if defined(STM32L011xx)
  #define GPIOA  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0000UL))
  #define GPIOB  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0400UL))
  #define GPIOC  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0800UL))
#elif defined(STM32L422xx)
  #define GPIOA  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0000UL))
  #define GPIOB  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0400UL))
  #define GPIOC  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0800UL))
  #define GPIOH  ((GPIO_TypeDef *)(IOPORT_BASE + 0x1C00UL))
#elif defined(STM32WBA55xx)
  #define GPIOA  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0000UL))
  #define GPIOB  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0400UL))
  #define GPIOC  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0800UL))
  #define GPIOH  ((GPIO_TypeDef *)(IOPORT_BASE + 0x1C00UL))
#elif defined(STM32H523xx)
  #define GPIOA  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0000UL))
  #define GPIOB  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0400UL))
  #define GPIOC  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0800UL))
  #define GPIOD  ((GPIO_TypeDef *)(IOPORT_BASE + 0x0C00UL))
  #define GPIOH  ((GPIO_TypeDef *)(IOPORT_BASE + 0x1C00UL))
#endif

/* GPIO mode values (2 bits per pin in MODER) */
#define LL_GPIO_MODE_INPUT      0x0UL
#define LL_GPIO_MODE_OUTPUT     0x1UL
#define LL_GPIO_MODE_AF         0x2UL
#define LL_GPIO_MODE_ANALOG     0x3UL

/* GPIO output type (1 bit per pin in OTYPER) */
#define LL_GPIO_OTYPE_PP        0x0UL   /* Push-pull */
#define LL_GPIO_OTYPE_OD        0x1UL   /* Open-drain */

/* GPIO speed (2 bits per pin in OSPEEDR) */
#define LL_GPIO_SPEED_LOW       0x0UL
#define LL_GPIO_SPEED_MED       0x1UL
#define LL_GPIO_SPEED_HIGH      0x2UL
#define LL_GPIO_SPEED_VHIGH     0x3UL

/* GPIO pull (2 bits per pin in PUPDR) */
#define LL_GPIO_PULL_NONE       0x0UL
#define LL_GPIO_PULL_UP         0x1UL
#define LL_GPIO_PULL_DOWN       0x2UL

#endif /* LL_COMMON_H */
