/**
 * ll_uart.h — Low-level USART / LPUART operations
 *
 * Polling TX/RX, baud rate configuration.
 * All four STM32 families use the same USART register IP,
 * so operations are generic — only base addresses differ.
 */

#ifndef LL_UART_H
#define LL_UART_H

#include "ll_common.h"

/* ---- USART register structure ---- */

typedef struct {
    volatile uint32_t CR1;      /* 0x00: Control register 1 */
    volatile uint32_t CR2;      /* 0x04: Control register 2 */
    volatile uint32_t CR3;      /* 0x08: Control register 3 */
    volatile uint32_t BRR;      /* 0x0C: Baud rate register */
    volatile uint32_t GTPR;     /* 0x10: Guard time and prescaler */
    volatile uint32_t RTOR;     /* 0x14: Receiver timeout */
    volatile uint32_t RQR;      /* 0x18: Request register */
    volatile uint32_t ISR;      /* 0x1C: Interrupt and status */
    volatile uint32_t ICR;      /* 0x20: Interrupt flag clear */
    volatile uint32_t RDR;      /* 0x24: Receive data */
    volatile uint32_t TDR;      /* 0x28: Transmit data */
    volatile uint32_t PRESC;    /* 0x2C: Prescaler (L4+) */
} USART_TypeDef;

/* ---- Instance base addresses ---- */

#if defined(STM32L011xx)
  #define USART2    ((USART_TypeDef *)0x40004400UL)
  #define LPUART1   ((USART_TypeDef *)0x40004800UL)

#elif defined(STM32L422xx)
  #define USART1    ((USART_TypeDef *)0x40013800UL)
  #define USART2    ((USART_TypeDef *)0x40004400UL)
  #define LPUART1   ((USART_TypeDef *)0x40008000UL)

#elif defined(STM32WBA55xx)
  #define USART1    ((USART_TypeDef *)0x40013800UL)
  #define USART2    ((USART_TypeDef *)0x40004400UL)
  #define LPUART1   ((USART_TypeDef *)0x46002400UL)

#elif defined(STM32H523xx)
  #define USART1    ((USART_TypeDef *)0x40013800UL)
  #define USART2    ((USART_TypeDef *)0x40004400UL)
  #define USART3    ((USART_TypeDef *)0x40004800UL)
  #define LPUART1   ((USART_TypeDef *)0x44002400UL)
#endif

/* ---- CR1 bit definitions ---- */

#define LL_USART_CR1_UE         (1UL << 0)    /* USART enable */
#define LL_USART_CR1_RE         (1UL << 2)    /* Receiver enable */
#define LL_USART_CR1_TE         (1UL << 3)    /* Transmitter enable */
#define LL_USART_CR1_RXNEIE     (1UL << 5)    /* RXNE interrupt enable */
#define LL_USART_CR1_TCIE       (1UL << 6)    /* TC interrupt enable */
#define LL_USART_CR1_TXEIE      (1UL << 7)    /* TXE interrupt enable */
#define LL_USART_CR1_OVER8      (1UL << 15)   /* Oversampling 8 */

/* ---- ISR bit definitions ---- */

#define LL_USART_ISR_PE         (1UL << 0)    /* Parity error */
#define LL_USART_ISR_FE         (1UL << 1)    /* Framing error */
#define LL_USART_ISR_NE         (1UL << 2)    /* Noise error */
#define LL_USART_ISR_ORE        (1UL << 3)    /* Overrun error */
#define LL_USART_ISR_IDLE       (1UL << 4)    /* IDLE line detected */
#define LL_USART_ISR_RXNE       (1UL << 5)    /* Read data register not empty */
#define LL_USART_ISR_TC         (1UL << 6)    /* Transmission complete */
#define LL_USART_ISR_TXE        (1UL << 7)    /* Transmit data register empty */
#define LL_USART_ISR_TEACK      (1UL << 21)   /* Transmit enable acknowledge */
#define LL_USART_ISR_REACK      (1UL << 22)   /* Receive enable acknowledge */

/* ---- ICR bit definitions ---- */

#define LL_USART_ICR_PECF       (1UL << 0)
#define LL_USART_ICR_FECF       (1UL << 1)
#define LL_USART_ICR_NECF       (1UL << 2)
#define LL_USART_ICR_ORECF      (1UL << 3)
#define LL_USART_ICR_IDLECF     (1UL << 4)
#define LL_USART_ICR_TCCF       (1UL << 6)

/* ============================================================
 * Configuration
 * ============================================================ */

/**
 * Initialize a USART for 8N1 at the given baud rate.
 *   uart:     USART instance (USART1, USART2, etc.)
 *   pclk_hz:  peripheral clock feeding this USART (APB1 or APB2)
 *   baud:     desired baud rate
 *
 * Prerequisites:
 *   - Peripheral clock enabled via ll_rcc_apb1/2_clk_enable()
 *   - TX/RX pins configured for AF via ll_gpio_config_af()
 */
static inline void ll_uart_init(USART_TypeDef *uart, uint32_t pclk_hz, uint32_t baud)
{
    /* Disable USART while configuring */
    uart->CR1 = 0;
    uart->CR2 = 0;
    uart->CR3 = 0;

    /* 8N1: word length 8 (M[1:0] = 00), no parity, 1 stop bit — all default 0 */

    /* Baud rate: BRR = pclk / baud (16x oversampling) */
    uart->BRR = (pclk_hz + baud / 2) / baud;

    /* Enable USART, transmitter, and receiver */
    uart->CR1 = LL_USART_CR1_UE | LL_USART_CR1_TE | LL_USART_CR1_RE;

    /* Wait for TE and RE to be acknowledged */
    while (!(uart->ISR & LL_USART_ISR_TEACK))
        ;
}

/**
 * Initialize an LPUART for 8N1.
 * LPUART BRR calculation differs: BRR = 256 * pclk / baud
 */
static inline void ll_lpuart_init(USART_TypeDef *uart, uint32_t pclk_hz, uint32_t baud)
{
    uart->CR1 = 0;
    uart->CR2 = 0;
    uart->CR3 = 0;

    /* LPUART BRR = 256 × fck / baud */
    uart->BRR = (uint32_t)(((uint64_t)256 * pclk_hz + baud / 2) / baud);

    uart->CR1 = LL_USART_CR1_UE | LL_USART_CR1_TE | LL_USART_CR1_RE;

    while (!(uart->ISR & LL_USART_ISR_TEACK))
        ;
}

/* ============================================================
 * Polling TX
 * ============================================================ */

/** Wait until transmit data register is empty */
static inline void ll_uart_wait_txe(USART_TypeDef *uart)
{
    while (!(uart->ISR & LL_USART_ISR_TXE))
        ;
}

/** Wait until transmission is complete (last byte fully shifted out) */
static inline void ll_uart_wait_tc(USART_TypeDef *uart)
{
    while (!(uart->ISR & LL_USART_ISR_TC))
        ;
}

/** Transmit a single byte (blocking) */
static inline void ll_uart_tx(USART_TypeDef *uart, uint8_t data)
{
    ll_uart_wait_txe(uart);
    uart->TDR = data;
}

/** Transmit a buffer (blocking) */
static inline void ll_uart_tx_buf(USART_TypeDef *uart, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        ll_uart_tx(uart, data[i]);
    }
    ll_uart_wait_tc(uart);
}

/** Transmit a null-terminated string (blocking) */
static inline void ll_uart_tx_str(USART_TypeDef *uart, const char *str)
{
    while (*str) {
        ll_uart_tx(uart, (uint8_t)*str++);
    }
    ll_uart_wait_tc(uart);
}

/* ============================================================
 * Polling RX
 * ============================================================ */

/** Check if received data is available */
static inline int ll_uart_rx_ready(USART_TypeDef *uart)
{
    return (uart->ISR & LL_USART_ISR_RXNE) != 0;
}

/** Receive a single byte (blocking — waits until data arrives) */
static inline uint8_t ll_uart_rx(USART_TypeDef *uart)
{
    while (!ll_uart_rx_ready(uart))
        ;
    return (uint8_t)uart->RDR;
}

/**
 * Try to receive a byte without blocking.
 * Returns 1 if a byte was read (stored in *data), 0 if nothing available.
 */
static inline int ll_uart_rx_try(USART_TypeDef *uart, uint8_t *data)
{
    if (uart->ISR & LL_USART_ISR_RXNE) {
        *data = (uint8_t)uart->RDR;
        return 1;
    }
    return 0;
}

/* ============================================================
 * Error handling
 * ============================================================ */

/** Clear all error flags (PE, FE, NE, ORE) */
static inline void ll_uart_clear_errors(USART_TypeDef *uart)
{
    uart->ICR = LL_USART_ICR_PECF | LL_USART_ICR_FECF
              | LL_USART_ICR_NECF | LL_USART_ICR_ORECF;
}

/** Check for any error flags */
static inline uint32_t ll_uart_errors(USART_TypeDef *uart)
{
    return uart->ISR & (LL_USART_ISR_PE | LL_USART_ISR_FE
                      | LL_USART_ISR_NE | LL_USART_ISR_ORE);
}

/* ============================================================
 * Enable / Disable
 * ============================================================ */

static inline void ll_uart_enable(USART_TypeDef *uart)
{
    SET_BITS(uart->CR1, LL_USART_CR1_UE);
}

static inline void ll_uart_disable(USART_TypeDef *uart)
{
    CLR_BITS(uart->CR1, LL_USART_CR1_UE);
}

#endif /* LL_UART_H */
