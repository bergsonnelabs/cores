/**
 * ll_spi.h — Low-level SPI operations
 *
 * Polling master-mode SPI. The SPI IP differs between families:
 *   - L0/L4: "old" SPI (single DR register, SR-based status)
 *   - WBA/H5: "new" SPI v2 (separate TXDR/RXDR, different config)
 *
 * This driver abstracts both behind a common inline API.
 */

#ifndef LL_SPI_H
#define LL_SPI_H

#include "ll_common.h"

/* ============================================================
 * Register structures (two different IPs)
 * ============================================================ */

#if defined(STM32L011xx) || defined(STM32L422xx)

/* ---- Old SPI IP (L0, L4) ---- */

typedef struct {
    volatile uint32_t CR1;      /* 0x00: Control register 1 */
    volatile uint32_t CR2;      /* 0x04: Control register 2 */
    volatile uint32_t SR;       /* 0x08: Status register */
    volatile uint32_t DR;       /* 0x0C: Data register */
    volatile uint32_t CRCPR;    /* 0x10: CRC polynomial */
    volatile uint32_t RXCRCR;   /* 0x14: RX CRC */
    volatile uint32_t TXCRCR;   /* 0x18: TX CRC */
} SPI_TypeDef;

/* CR1 bits */
#define LL_SPI_CR1_CPHA         (1UL << 0)
#define LL_SPI_CR1_CPOL         (1UL << 1)
#define LL_SPI_CR1_MSTR         (1UL << 2)
#define LL_SPI_CR1_BR_SHIFT     3            /* BR[2:0] = prescaler */
#define LL_SPI_CR1_SPE          (1UL << 6)
#define LL_SPI_CR1_LSBFIRST     (1UL << 7)
#define LL_SPI_CR1_SSI          (1UL << 8)
#define LL_SPI_CR1_SSM          (1UL << 9)

/* CR2 bits */
#define LL_SPI_CR2_RXDMAEN     (1UL << 0)   /* RX DMA enable */
#define LL_SPI_CR2_TXDMAEN     (1UL << 1)   /* TX DMA enable */
#define LL_SPI_CR2_DS_SHIFT     8            /* DS[3:0] data size */
#define LL_SPI_CR2_FRXTH        (1UL << 12)  /* FIFO threshold: 1=8-bit */

/* SR bits */
#define LL_SPI_SR_RXNE          (1UL << 0)
#define LL_SPI_SR_TXE           (1UL << 1)
#define LL_SPI_SR_BSY           (1UL << 7)

#elif defined(STM32WBA55xx) || defined(STM32H523xx)

/* ---- New SPI v2 IP (WBA, H5) ---- */

typedef struct {
    volatile uint32_t CR1;      /* 0x00: Control register 1 */
    volatile uint32_t CR2;      /* 0x04: Control register 2 */
    volatile uint32_t CFG1;     /* 0x08: Configuration 1 */
    volatile uint32_t CFG2;     /* 0x0C: Configuration 2 */
    volatile uint32_t IER;      /* 0x10: Interrupt enable */
    volatile uint32_t SR;       /* 0x14: Status register */
    volatile uint32_t IFCR;     /* 0x18: Interrupt flag clear */
    volatile uint32_t _RESERVED0; /* 0x1C */
    volatile uint32_t TXDR;     /* 0x20: Transmit data */
    volatile uint32_t _RESERVED1[3];
    volatile uint32_t RXDR;     /* 0x30: Receive data */
    volatile uint32_t _RESERVED2[3];
    volatile uint32_t CRCPOLY;  /* 0x40: CRC polynomial */
    volatile uint32_t TXCRCR;   /* 0x44: TX CRC */
    volatile uint32_t RXCRCR;   /* 0x48: RX CRC */
    volatile uint32_t UDRDR;    /* 0x4C: Underrun data */
} SPI_TypeDef;

/* CR1 bits */
#define LL_SPI_CR1_SPE          (1UL << 0)
#define LL_SPI_CR1_CSTART       (1UL << 9)

/* CR2 bits */
#define LL_SPI_CR2_TSIZE_SHIFT  0

/* CFG1 bits */
#define LL_SPI_CFG1_RXDMAEN    (1UL << 14)  /* RX DMA enable */
#define LL_SPI_CFG1_TXDMAEN    (1UL << 15)  /* TX DMA enable */
#define LL_SPI_CFG1_DSIZE_SHIFT 0            /* DSIZE[4:0] */
#define LL_SPI_CFG1_MBR_SHIFT   28           /* MBR[2:0] prescaler */

/* CFG2 bits */
#define LL_SPI_CFG2_CPHA        (1UL << 24)
#define LL_SPI_CFG2_CPOL        (1UL << 25)
#define LL_SPI_CFG2_MASTER      (1UL << 22)
#define LL_SPI_CFG2_LSBFRST     (1UL << 23)
#define LL_SPI_CFG2_SSM         (1UL << 26)
#define LL_SPI_CFG2_SSOM        (1UL << 29)
#define LL_SPI_CFG2_SSOE        (1UL << 30)
#define LL_SPI_CFG2_AFCNTR      (1UL << 31)

/* SR bits */
#define LL_SPI_SR_RXP           (1UL << 0)   /* RX packet available */
#define LL_SPI_SR_TXP           (1UL << 1)   /* TX packet space available */
#define LL_SPI_SR_EOT           (1UL << 3)   /* End of transfer */
#define LL_SPI_SR_OVR           (1UL << 6)   /* Overrun */

/* IFCR bits */
#define LL_SPI_IFCR_EOTC        (1UL << 3)
#define LL_SPI_IFCR_TXTFC       (1UL << 4)
#define LL_SPI_IFCR_OVRC        (1UL << 6)

#endif /* SPI IP version */

/* ---- Instance base addresses ---- */

#if defined(STM32L011xx)
  #define SPI1      ((SPI_TypeDef *)0x40013000UL)

#elif defined(STM32L422xx)
  #define SPI1      ((SPI_TypeDef *)0x40013000UL)

#elif defined(STM32WBA55xx)
  #define SPI1      ((SPI_TypeDef *)0x40013000UL)
  #define SPI3      ((SPI_TypeDef *)0x46002000UL)

#elif defined(STM32H523xx)
  #define SPI1      ((SPI_TypeDef *)0x40013000UL)
  #define SPI2      ((SPI_TypeDef *)0x40003800UL)
  #define SPI3      ((SPI_TypeDef *)0x40003C00UL)
#endif

/* ---- Prescaler values ---- */
/* Divides peripheral clock by 2, 4, 8, 16, 32, 64, 128, 256 */

#define LL_SPI_PRESCALER_2      0
#define LL_SPI_PRESCALER_4      1
#define LL_SPI_PRESCALER_8      2
#define LL_SPI_PRESCALER_16     3
#define LL_SPI_PRESCALER_32     4
#define LL_SPI_PRESCALER_64     5
#define LL_SPI_PRESCALER_128    6
#define LL_SPI_PRESCALER_256    7

/* ---- Clock polarity / phase ---- */

#define LL_SPI_CPOL_LOW         0
#define LL_SPI_CPOL_HIGH        1
#define LL_SPI_CPHA_1EDGE       0   /* Data on first clock edge */
#define LL_SPI_CPHA_2EDGE       1   /* Data on second clock edge */

/* ============================================================
 * Configuration
 * ============================================================ */

/**
 * Initialize SPI in master mode, 8-bit, MSB-first.
 *   spi:       SPI instance
 *   prescaler: LL_SPI_PRESCALER_* (clock divider)
 *   cpol:      LL_SPI_CPOL_LOW or _HIGH
 *   cpha:      LL_SPI_CPHA_1EDGE or _2EDGE
 *
 * Prerequisites:
 *   - Peripheral clock enabled via ll_rcc_apb1/2_clk_enable()
 *   - SCK/MOSI/MISO pins configured for AF via ll_gpio_config_af()
 *   - CS managed manually via GPIO (not hardware NSS)
 */
static inline void ll_spi_init(SPI_TypeDef *spi, uint32_t prescaler,
                               uint32_t cpol, uint32_t cpha)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    /* Old SPI IP */
    spi->CR1 = 0;

    spi->CR1 = LL_SPI_CR1_MSTR                       /* Master mode */
             | LL_SPI_CR1_SSM                         /* Software CS */
             | LL_SPI_CR1_SSI                         /* Internal CS high */
             | (prescaler << LL_SPI_CR1_BR_SHIFT)     /* Baud rate */
             | (cpol ? LL_SPI_CR1_CPOL : 0)
             | (cpha ? LL_SPI_CR1_CPHA : 0);

    /* 8-bit data size, FIFO threshold for 8-bit access */
    spi->CR2 = (0x7UL << LL_SPI_CR2_DS_SHIFT)        /* DS = 0111 → 8-bit */
             | LL_SPI_CR2_FRXTH;                      /* 8-bit FIFO threshold */

    spi->CR1 |= LL_SPI_CR1_SPE;                      /* Enable */

#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    /* New SPI v2 IP */
    spi->CR1 = 0;

    spi->CFG1 = (7UL << LL_SPI_CFG1_DSIZE_SHIFT)     /* 8-bit data */
              | (prescaler << LL_SPI_CFG1_MBR_SHIFT);  /* Baud rate */

    spi->CFG2 = LL_SPI_CFG2_MASTER                    /* Master mode */
              | LL_SPI_CFG2_SSM                        /* Software CS */
              | LL_SPI_CFG2_AFCNTR                     /* Keep AF control */
              | (cpol ? LL_SPI_CFG2_CPOL : 0)
              | (cpha ? LL_SPI_CFG2_CPHA : 0);

    /* SSI must be high in master mode with SSM to prevent MODF.
     * Clear any latched MODF before enabling SPE. */
    spi->CR1 = (1UL << 12);                            /* SSI = 1 */
    spi->IFCR = (1UL << 9);                            /* Clear MODF */
    spi->CR1 = (1UL << 12) | LL_SPI_CR1_SPE;          /* SSI + Enable */
#endif
}

/* ============================================================
 * Polling transfer
 * ============================================================ */

/**
 * Transfer a single byte (full-duplex): send tx_data, return received byte.
 */
static inline uint8_t ll_spi_transfer(SPI_TypeDef *spi, uint8_t tx_data)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    /* Wait for TXE, write data */
    while (!(spi->SR & LL_SPI_SR_TXE))
        ;
    *(volatile uint8_t *)&spi->DR = tx_data;

    /* Wait for RXNE, read data */
    while (!(spi->SR & LL_SPI_SR_RXNE))
        ;
    return *(volatile uint8_t *)&spi->DR;

#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    /* SPI v2 single-byte transfer:
     *   1. Set TSIZE = 1
     *   2. Write TXDR (preload data before starting clock)
     *   3. Set CSTART (begins clocking)
     *   4. Wait for RXP (byte received)
     *   5. Read RXDR
     *   6. Wait for EOT, clear flags */

    /* TSIZE must be set while SPE=0 on SPI v2 */
    spi->CR1 &= ~LL_SPI_CR1_SPE;  /* Disable */
    spi->CR2 = 1;                  /* TSIZE = 1 byte */
    spi->CR1 |= LL_SPI_CR1_SPE;   /* Re-enable */

    /* Preload TX data, then start clocking */
    *(volatile uint8_t *)&spi->TXDR = tx_data;
    SET_BITS(spi->CR1, LL_SPI_CR1_CSTART);

    /* Wait for RX data */
    while (!(spi->SR & LL_SPI_SR_RXP))
        ;
    uint8_t rx = *(volatile uint8_t *)&spi->RXDR;

    /* Wait for end of transfer, clear flags */
    while (!(spi->SR & LL_SPI_SR_EOT))
        ;
    spi->IFCR = LL_SPI_IFCR_EOTC | LL_SPI_IFCR_TXTFC;

    return rx;
#endif
}

/**
 * Transfer a buffer (full-duplex, in-place).
 * tx_buf is sent; received data overwrites tx_buf.
 * Pass NULL for tx_buf to send zeros (read-only).
 */
static inline void ll_spi_transfer_buf(SPI_TypeDef *spi,
                                       uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint8_t tx = buf ? buf[i] : 0x00;
        uint8_t rx = ll_spi_transfer(spi, tx);
        if (buf) buf[i] = rx;
    }
}

/**
 * Write-only transfer (discard received data).
 */
static inline void ll_spi_write(SPI_TypeDef *spi, uint8_t data)
{
    (void)ll_spi_transfer(spi, data);
}

/**
 * Read-only transfer (send zeros, return received byte).
 */
static inline uint8_t ll_spi_read(SPI_TypeDef *spi)
{
    return ll_spi_transfer(spi, 0x00);
}

/* ============================================================
 * Enable / Disable
 * ============================================================ */

static inline void ll_spi_enable(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    SET_BITS(spi->CR1, LL_SPI_CR1_SPE);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    SET_BITS(spi->CR1, LL_SPI_CR1_SPE);
#endif
}

static inline void ll_spi_disable(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    /* Wait until not busy, then disable */
    while (spi->SR & LL_SPI_SR_BSY)
        ;
    CLR_BITS(spi->CR1, LL_SPI_CR1_SPE);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    CLR_BITS(spi->CR1, LL_SPI_CR1_SPE);
#endif
}

/* ============================================================
 * Busy check
 * ============================================================ */

static inline int ll_spi_busy(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    return (spi->SR & LL_SPI_SR_BSY) != 0;
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    return !(spi->SR & LL_SPI_SR_EOT);
#endif
}

/* ============================================================
 * DMA enable / disable
 * ============================================================ */

/** Enable DMA requests for RX */
static inline void ll_spi_enable_dma_rx(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    SET_BITS(spi->CR2, LL_SPI_CR2_RXDMAEN);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    SET_BITS(spi->CFG1, LL_SPI_CFG1_RXDMAEN);
#endif
}

/** Disable DMA requests for RX */
static inline void ll_spi_disable_dma_rx(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    CLR_BITS(spi->CR2, LL_SPI_CR2_RXDMAEN);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    CLR_BITS(spi->CFG1, LL_SPI_CFG1_RXDMAEN);
#endif
}

/** Enable DMA requests for TX */
static inline void ll_spi_enable_dma_tx(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    SET_BITS(spi->CR2, LL_SPI_CR2_TXDMAEN);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    SET_BITS(spi->CFG1, LL_SPI_CFG1_TXDMAEN);
#endif
}

/** Disable DMA requests for TX */
static inline void ll_spi_disable_dma_tx(SPI_TypeDef *spi)
{
#if defined(STM32L011xx) || defined(STM32L422xx)
    CLR_BITS(spi->CR2, LL_SPI_CR2_TXDMAEN);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    CLR_BITS(spi->CFG1, LL_SPI_CFG1_TXDMAEN);
#endif
}

#endif /* LL_SPI_H */
