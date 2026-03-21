/**
 * ll_gpio.h — Low-level GPIO operations
 *
 * Thin inline functions over the GPIO registers. No state, no
 * malloc — just direct register manipulation with type safety.
 *
 * The GPIO register layout is identical across all STM32 families,
 * so this file has no #ifdef blocks.
 */

#ifndef LL_GPIO_H
#define LL_GPIO_H

#include "ll_common.h"

/* ---- Pin state ---- */

/** Set one or more pins high (write 1s to BSRR lower half) */
static inline void ll_gpio_set(GPIO_TypeDef *port, uint32_t pin_mask)
{
    port->BSRR = pin_mask;
}

/** Set one or more pins low (write 1s to BSRR upper half) */
static inline void ll_gpio_clear(GPIO_TypeDef *port, uint32_t pin_mask)
{
    port->BSRR = pin_mask << 16;
}

/** Toggle one or more pins (XOR with ODR) */
static inline void ll_gpio_toggle(GPIO_TypeDef *port, uint32_t pin_mask)
{
    port->ODR ^= pin_mask;
}

/** Read the input state of one or more pins */
static inline uint32_t ll_gpio_read(GPIO_TypeDef *port, uint32_t pin_mask)
{
    return port->IDR & pin_mask;
}

/** Read the output state of one or more pins */
static inline uint32_t ll_gpio_read_output(GPIO_TypeDef *port, uint32_t pin_mask)
{
    return port->ODR & pin_mask;
}

/* ---- Pin configuration ---- */

/**
 * Set the mode for a single pin.
 *   mode: LL_GPIO_MODE_INPUT, _OUTPUT, _AF, or _ANALOG
 */
static inline void ll_gpio_set_mode(GPIO_TypeDef *port, uint32_t pin, uint32_t mode)
{
    uint32_t pos = pin * 2;
    MOD_BITS(port->MODER, 0x3UL << pos, mode << pos);
}

/**
 * Set the output type for a single pin.
 *   otype: LL_GPIO_OTYPE_PP (push-pull) or LL_GPIO_OTYPE_OD (open-drain)
 */
static inline void ll_gpio_set_output_type(GPIO_TypeDef *port, uint32_t pin, uint32_t otype)
{
    MOD_BITS(port->OTYPER, 1UL << pin, otype << pin);
}

/**
 * Set the output speed for a single pin.
 *   speed: LL_GPIO_SPEED_LOW, _MED, _HIGH, or _VHIGH
 */
static inline void ll_gpio_set_speed(GPIO_TypeDef *port, uint32_t pin, uint32_t speed)
{
    uint32_t pos = pin * 2;
    MOD_BITS(port->OSPEEDR, 0x3UL << pos, speed << pos);
}

/**
 * Set the pull-up/pull-down for a single pin.
 *   pull: LL_GPIO_PULL_NONE, _UP, or _DOWN
 */
static inline void ll_gpio_set_pull(GPIO_TypeDef *port, uint32_t pin, uint32_t pull)
{
    uint32_t pos = pin * 2;
    MOD_BITS(port->PUPDR, 0x3UL << pos, pull << pos);
}

/**
 * Set the alternate function for a single pin (0-15).
 * Pins 0-7 use AFR[0] (AFRL), pins 8-15 use AFR[1] (AFRH).
 */
static inline void ll_gpio_set_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    uint32_t reg_index = pin >> 3;          /* 0 for pins 0-7, 1 for pins 8-15 */
    uint32_t pos = (pin & 0x7UL) * 4;      /* 4 bits per pin within the register */
    MOD_BITS(port->AFR[reg_index], 0xFUL << pos, af << pos);
}

/* ---- Convenience: configure a pin in one call ---- */

/**
 * Configure a pin as general-purpose output (push-pull, no pull, medium speed).
 */
static inline void ll_gpio_config_output(GPIO_TypeDef *port, uint32_t pin)
{
    ll_gpio_set_mode(port, pin, LL_GPIO_MODE_OUTPUT);
    ll_gpio_set_output_type(port, pin, LL_GPIO_OTYPE_PP);
    ll_gpio_set_speed(port, pin, LL_GPIO_SPEED_MED);
    ll_gpio_set_pull(port, pin, LL_GPIO_PULL_NONE);
}

/**
 * Configure a pin as input with optional pull.
 */
static inline void ll_gpio_config_input(GPIO_TypeDef *port, uint32_t pin, uint32_t pull)
{
    ll_gpio_set_mode(port, pin, LL_GPIO_MODE_INPUT);
    ll_gpio_set_pull(port, pin, pull);
}

/**
 * Configure a pin for an alternate function.
 *   af:    alternate function number (0-15)
 *   otype: LL_GPIO_OTYPE_PP or _OD
 *   speed: LL_GPIO_SPEED_LOW/MED/HIGH/VHIGH
 *   pull:  LL_GPIO_PULL_NONE/UP/DOWN
 */
static inline void ll_gpio_config_af(GPIO_TypeDef *port, uint32_t pin,
                                     uint32_t af, uint32_t otype,
                                     uint32_t speed, uint32_t pull)
{
    ll_gpio_set_af(port, pin, af);
    ll_gpio_set_mode(port, pin, LL_GPIO_MODE_AF);
    ll_gpio_set_output_type(port, pin, otype);
    ll_gpio_set_speed(port, pin, speed);
    ll_gpio_set_pull(port, pin, pull);
}

/**
 * Configure a pin as analog (disconnects from digital logic).
 */
static inline void ll_gpio_config_analog(GPIO_TypeDef *port, uint32_t pin)
{
    ll_gpio_set_mode(port, pin, LL_GPIO_MODE_ANALOG);
    ll_gpio_set_pull(port, pin, LL_GPIO_PULL_NONE);
}

#endif /* LL_GPIO_H */
