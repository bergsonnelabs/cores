/**
 * hal_common.h — HAL shared types and utilities
 *
 * Status codes, callback types, ring buffer, IRQ number definitions.
 * Used by all HAL drivers.
 */

#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include "ll_common.h"

/* ============================================================
 * Status codes
 * ============================================================ */

typedef enum {
    HAL_OK       =  0,
    HAL_ERROR    = -1,
    HAL_BUSY     = -2,
    HAL_TIMEOUT  = -3,
    HAL_NACK     = -4,
} hal_status_t;

/* ============================================================
 * Callback type
 * ============================================================ */

/** Generic completion callback */
typedef void (*hal_callback_t)(void *ctx);

/* ============================================================
 * Ring buffer (lock-free, single producer / single consumer)
 *
 * Used by UART interrupt RX. Size must be a power of 2.
 * ============================================================ */

typedef struct {
    uint8_t  *buf;
    uint16_t  size;             /* Must be power of 2 */
    volatile uint16_t head;     /* Written by producer (ISR) */
    volatile uint16_t tail;     /* Read by consumer (main) */
} hal_ringbuf_t;

static inline void hal_ringbuf_init(hal_ringbuf_t *rb, uint8_t *buf, uint16_t size)
{
    rb->buf  = buf;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

static inline int hal_ringbuf_empty(const hal_ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

static inline int hal_ringbuf_full(const hal_ringbuf_t *rb)
{
    return ((rb->head + 1) & (rb->size - 1)) == rb->tail;
}

static inline uint16_t hal_ringbuf_count(const hal_ringbuf_t *rb)
{
    return (rb->head - rb->tail) & (rb->size - 1);
}

/** Put a byte into the ring buffer. Returns 1 on success, 0 if full. */
static inline int hal_ringbuf_put(hal_ringbuf_t *rb, uint8_t byte)
{
    uint16_t next = (rb->head + 1) & (rb->size - 1);
    if (next == rb->tail)
        return 0;  /* Full */
    rb->buf[rb->head] = byte;
    rb->head = next;
    return 1;
}

/** Get a byte from the ring buffer. Returns 1 on success, 0 if empty. */
static inline int hal_ringbuf_get(hal_ringbuf_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail)
        return 0;  /* Empty */
    *byte = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & (rb->size - 1);
    return 1;
}

/** Drain up to max_len bytes into buf. Returns number of bytes read. */
static inline uint16_t hal_ringbuf_read(hal_ringbuf_t *rb, uint8_t *buf, uint16_t max_len)
{
    uint16_t count = 0;
    while (count < max_len && !hal_ringbuf_empty(rb)) {
        hal_ringbuf_get(rb, &buf[count++]);
    }
    return count;
}

/* ============================================================
 * NVIC helpers — thin wrappers around ll_common.h
 * ============================================================ */

static inline void hal_nvic_enable_irq(uint32_t irqn)  { ll_nvic_enable_irq(irqn); }
static inline void hal_nvic_disable_irq(uint32_t irqn) { ll_nvic_disable_irq(irqn); }
static inline void hal_nvic_set_priority(uint32_t irqn, uint8_t priority) { ll_nvic_set_priority(irqn, priority); }
static inline void hal_nvic_clear_pending(uint32_t irqn) { ll_nvic_clear_pending(irqn); }

/* ============================================================
 * Per-family IRQ numbers for peripherals used by HAL
 * ============================================================ */

#if defined(STM32L011xx)
  /* L0: USART2 only, no USART1 */
  #define HAL_IRQ_USART2        28
  #define HAL_IRQ_LPUART1       29
  #define HAL_IRQ_I2C1_EV       23
  #define HAL_IRQ_SPI1          25
  #define HAL_IRQ_TIM2          15
  #define HAL_IRQ_TIM21         20
  #define HAL_IRQ_ADC1          12
  #define HAL_IRQ_DMA1_CH1      9
  #define HAL_IRQ_DMA1_CH2_3    10
  #define HAL_IRQ_DMA1_CH4_7    11

#elif defined(STM32L422xx)
  #define HAL_IRQ_USART1        37
  #define HAL_IRQ_USART2        38
  #define HAL_IRQ_LPUART1       70
  #define HAL_IRQ_I2C1_EV       31
  #define HAL_IRQ_I2C1_ER       32
  #define HAL_IRQ_I2C3_EV       72
  #define HAL_IRQ_I2C3_ER       73
  #define HAL_IRQ_SPI1          35
  #define HAL_IRQ_TIM1_UP_16    25
  #define HAL_IRQ_TIM1_CC       27
  #define HAL_IRQ_TIM2          28
  #define HAL_IRQ_TIM15         24
  #define HAL_IRQ_ADC1          18
  #define HAL_IRQ_DMA1_CH1      11
  #define HAL_IRQ_DMA1_CH2      12
  #define HAL_IRQ_DMA1_CH3      13
  #define HAL_IRQ_DMA1_CH4      14
  #define HAL_IRQ_DMA1_CH5      15
  #define HAL_IRQ_DMA1_CH6      16
  #define HAL_IRQ_DMA1_CH7      17
  #define HAL_IRQ_USB            67

#elif defined(STM32WBA55xx)
  #define HAL_IRQ_USART1        46
  #define HAL_IRQ_USART2        47
  #define HAL_IRQ_LPUART1       48
  #define HAL_IRQ_I2C1_EV       43
  #define HAL_IRQ_I2C1_ER       44
  #define HAL_IRQ_I2C3_EV       54
  #define HAL_IRQ_I2C3_ER       55
  #define HAL_IRQ_SPI1          45
  #define HAL_IRQ_SPI3          53
  #define HAL_IRQ_TIM1_UP       40
  #define HAL_IRQ_TIM1_CC       42
  #define HAL_IRQ_TIM2          36
  #define HAL_IRQ_TIM3          37
  #define HAL_IRQ_TIM16         38
  #define HAL_IRQ_TIM17         39
  #define HAL_IRQ_ADC4          17
  #define HAL_IRQ_GPDMA1_CH0    29
  #define HAL_IRQ_GPDMA1_CH1    30
  #define HAL_IRQ_GPDMA1_CH2    31
  #define HAL_IRQ_GPDMA1_CH3    32
  #define HAL_IRQ_GPDMA1_CH4    33
  #define HAL_IRQ_GPDMA1_CH5    34
  #define HAL_IRQ_GPDMA1_CH6    35
  /* WBA55 has GPDMA1 channels 0-6 only; no CH7 (IRQ 36 = TIM2) */

#elif defined(STM32H523xx)
  #define HAL_IRQ_USART1        58
  #define HAL_IRQ_USART2        59
  #define HAL_IRQ_USART3        60
  #define HAL_IRQ_LPUART1       63
  #define HAL_IRQ_I2C1_EV       55
  #define HAL_IRQ_I2C1_ER       56
  #define HAL_IRQ_I2C2_EV       57
  #define HAL_IRQ_I2C2_ER       58
  #define HAL_IRQ_SPI1          51
  #define HAL_IRQ_SPI2          52
  #define HAL_IRQ_SPI3          53
  #define HAL_IRQ_TIM1_UP       41
  #define HAL_IRQ_TIM1_CC       44
  #define HAL_IRQ_TIM2          45
  #define HAL_IRQ_TIM3          46
  #define HAL_IRQ_ADC1          37
  #define HAL_IRQ_GPDMA1_CH0    29
  #define HAL_IRQ_GPDMA1_CH1    30
  #define HAL_IRQ_GPDMA1_CH2    31
  #define HAL_IRQ_GPDMA1_CH3    32
  #define HAL_IRQ_GPDMA1_CH4    33
  #define HAL_IRQ_GPDMA1_CH5    34
  #define HAL_IRQ_GPDMA1_CH6    35
  #define HAL_IRQ_GPDMA1_CH7    36
#endif

/* ============================================================
 * Utility: simple timeout helper using SysTick
 * ============================================================ */

/** Get current SysTick-based tick count (requires ll_systick_init) */
extern volatile uint32_t _systick_ticks;

static inline uint32_t hal_tick(void)
{
    return _systick_ticks;
}

static inline int hal_timeout_expired(uint32_t start, uint32_t timeout_ms)
{
    return (hal_tick() - start) >= timeout_ms;
}

#endif /* HAL_COMMON_H */
