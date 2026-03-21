/**
 * ll_i2c.h — Low-level I2C operations
 *
 * Polling master-mode I2C. All four STM32 families use the same
 * I2C IP with TIMINGR-based configuration, so operations are
 * generic — only base addresses differ.
 */

#ifndef LL_I2C_H
#define LL_I2C_H

#include "ll_common.h"

/* ---- I2C register structure ---- */

typedef struct {
    volatile uint32_t CR1;      /* 0x00: Control register 1 */
    volatile uint32_t CR2;      /* 0x04: Control register 2 */
    volatile uint32_t OAR1;     /* 0x08: Own address 1 */
    volatile uint32_t OAR2;     /* 0x0C: Own address 2 */
    volatile uint32_t TIMINGR;  /* 0x10: Timing register */
    volatile uint32_t TIMEOUTR; /* 0x14: Timeout register */
    volatile uint32_t ISR;      /* 0x18: Interrupt and status */
    volatile uint32_t ICR;      /* 0x1C: Interrupt flag clear */
    volatile uint32_t PECR;     /* 0x20: PEC register */
    volatile uint32_t RXDR;     /* 0x24: Receive data */
    volatile uint32_t TXDR;     /* 0x28: Transmit data */
} I2C_TypeDef;

/* ---- Instance base addresses ---- */

#if defined(STM32L011xx)
  #define I2C1      ((I2C_TypeDef *)0x40005400UL)

#elif defined(STM32L422xx)
  #define I2C1      ((I2C_TypeDef *)0x40005400UL)
  #define I2C3      ((I2C_TypeDef *)0x40005C00UL)

#elif defined(STM32WBA55xx)
  #define I2C1      ((I2C_TypeDef *)0x40005400UL)
  #define I2C3      ((I2C_TypeDef *)0x46002800UL)

#elif defined(STM32H523xx)
  #define I2C1      ((I2C_TypeDef *)0x40005400UL)
  #define I2C2      ((I2C_TypeDef *)0x40005800UL)
#endif

/* ---- CR1 bit definitions ---- */

#define LL_I2C_CR1_PE           (1UL << 0)    /* Peripheral enable */
#define LL_I2C_CR1_TXIE         (1UL << 1)    /* TX interrupt enable */
#define LL_I2C_CR1_RXIE         (1UL << 2)    /* RX interrupt enable */
#define LL_I2C_CR1_NACKIE       (1UL << 4)    /* NACK interrupt enable */
#define LL_I2C_CR1_STOPIE       (1UL << 5)    /* STOP interrupt enable */
#define LL_I2C_CR1_ANFOFF       (1UL << 12)   /* Analog filter off */
#define LL_I2C_CR1_DNF_SHIFT    8             /* Digital noise filter [3:0] */

/* ---- CR2 bit definitions ---- */

#define LL_I2C_CR2_SADD_SHIFT   1             /* Slave address [7:1] for 7-bit */
#define LL_I2C_CR2_RD_WRN       (1UL << 10)   /* Transfer direction: 1=read */
#define LL_I2C_CR2_ADD10        (1UL << 11)   /* 10-bit addressing */
#define LL_I2C_CR2_START        (1UL << 13)   /* Generate START */
#define LL_I2C_CR2_STOP         (1UL << 14)   /* Generate STOP */
#define LL_I2C_CR2_NACK         (1UL << 15)   /* Generate NACK */
#define LL_I2C_CR2_NBYTES_SHIFT 16            /* Number of bytes [7:0] */
#define LL_I2C_CR2_RELOAD       (1UL << 24)   /* Reload mode */
#define LL_I2C_CR2_AUTOEND      (1UL << 25)   /* Auto-end mode */

/* ---- ISR bit definitions ---- */

#define LL_I2C_ISR_TXE          (1UL << 0)    /* TX register empty */
#define LL_I2C_ISR_TXIS         (1UL << 1)    /* TX interrupt status */
#define LL_I2C_ISR_RXNE         (1UL << 2)    /* RX register not empty */
#define LL_I2C_ISR_ADDR         (1UL << 3)    /* Address matched (slave) */
#define LL_I2C_ISR_NACKF        (1UL << 4)    /* NACK received */
#define LL_I2C_ISR_STOPF        (1UL << 5)    /* STOP detected */
#define LL_I2C_ISR_TC           (1UL << 6)    /* Transfer complete */
#define LL_I2C_ISR_TCR          (1UL << 7)    /* Transfer complete reload */
#define LL_I2C_ISR_BERR         (1UL << 8)    /* Bus error */
#define LL_I2C_ISR_ARLO         (1UL << 9)    /* Arbitration lost */
#define LL_I2C_ISR_OVR          (1UL << 10)   /* Overrun/underrun */
#define LL_I2C_ISR_BUSY         (1UL << 15)   /* Bus busy */

/* ---- ICR bit definitions ---- */

#define LL_I2C_ICR_ADDRCF       (1UL << 3)
#define LL_I2C_ICR_NACKCF       (1UL << 4)
#define LL_I2C_ICR_STOPCF       (1UL << 5)
#define LL_I2C_ICR_BERRCF       (1UL << 8)
#define LL_I2C_ICR_ARLOCF       (1UL << 9)
#define LL_I2C_ICR_OVRCF        (1UL << 10)

/* ---- Error mask ---- */

#define LL_I2C_ERR_MASK         (LL_I2C_ISR_NACKF | LL_I2C_ISR_BERR \
                               | LL_I2C_ISR_ARLO | LL_I2C_ISR_OVR)

/* ============================================================
 * Pre-computed TIMINGR values
 *
 * TIMINGR is a 32-bit register packing PRESC, SCLDEL, SDADEL,
 * SCLH, SCLL. These values are pre-calculated for common
 * configurations. Use STM32CubeMX I2C timing tool or the
 * AN4235 spreadsheet for other combinations.
 * ============================================================ */

/* Standard mode (100kHz) from various peripheral clocks */
#define LL_I2C_TIMING_100K_16MHZ    0x00503D5AUL
#define LL_I2C_TIMING_100K_48MHZ    0x20602938UL
#define LL_I2C_TIMING_100K_80MHZ    0x30A0A7FBUL

/* Fast mode (400kHz) from various peripheral clocks */
#define LL_I2C_TIMING_400K_16MHZ    0x00300617UL
#define LL_I2C_TIMING_400K_48MHZ    0x00B01A4BUL
#define LL_I2C_TIMING_400K_80MHZ    0x00B01B59UL

/* Fast mode plus (1MHz) from various peripheral clocks */
#define LL_I2C_TIMING_1M_48MHZ     0x00300B29UL
#define LL_I2C_TIMING_1M_80MHZ     0x00300F33UL

/* ============================================================
 * Configuration
 * ============================================================ */

/**
 * Initialize I2C in master mode.
 *   i2c:     I2C instance
 *   timing:  TIMINGR value (use LL_I2C_TIMING_* defines)
 *
 * Prerequisites:
 *   - Peripheral clock enabled via ll_rcc_apb1_clk_enable()
 *   - SDA/SCL pins configured for AF, open-drain, pull-up
 */
static inline void ll_i2c_init(I2C_TypeDef *i2c, uint32_t timing)
{
    /* Disable I2C while configuring */
    i2c->CR1 = 0;

    /* Set timing */
    i2c->TIMINGR = timing;

    /* Enable analog noise filter (ANFOFF=0 is enabled) */
    /* No digital filter (DNF=0000) */

    /* Enable I2C */
    i2c->CR1 = LL_I2C_CR1_PE;
}

/* ============================================================
 * Polling master write
 * ============================================================ */

/**
 * Return codes for I2C operations.
 */
#define LL_I2C_OK       0
#define LL_I2C_NACK     -1
#define LL_I2C_ERROR    -2
#define LL_I2C_TIMEOUT  -3

/**
 * Master write to a 7-bit address device.
 *   addr:  7-bit slave address (unshifted, e.g. 0x68 for MPU6050)
 *   data:  pointer to bytes to send
 *   len:   number of bytes
 *
 * Returns LL_I2C_OK on success, LL_I2C_NACK or LL_I2C_ERROR on failure.
 */
static inline int ll_i2c_write(I2C_TypeDef *i2c, uint8_t addr,
                               const uint8_t *data, uint32_t len)
{
    /* Clear any pending flags */
    i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF
             | LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF;

    /* Configure transfer: 7-bit addr, write, NBYTES, AUTOEND, START */
    i2c->CR2 = ((uint32_t)addr << LL_I2C_CR2_SADD_SHIFT)
             | (len << LL_I2C_CR2_NBYTES_SHIFT)
             | LL_I2C_CR2_AUTOEND
             | LL_I2C_CR2_START;

    for (uint32_t i = 0; i < len; i++) {
        /* Wait for TXIS or error */
        while (1) {
            uint32_t isr = i2c->ISR;
            if (isr & LL_I2C_ISR_NACKF) {
                i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF;
                return LL_I2C_NACK;
            }
            if (isr & (LL_I2C_ISR_BERR | LL_I2C_ISR_ARLO)) {
                i2c->ICR = LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF | LL_I2C_ICR_STOPCF;
                return LL_I2C_ERROR;
            }
            if (isr & LL_I2C_ISR_TXIS)
                break;
        }
        i2c->TXDR = data[i];
    }

    /* Wait for STOP (AUTOEND generates it after last byte) */
    while (!(i2c->ISR & LL_I2C_ISR_STOPF))
        ;
    i2c->ICR = LL_I2C_ICR_STOPCF;

    return LL_I2C_OK;
}

/* ============================================================
 * Polling master read
 * ============================================================ */

/**
 * Master read from a 7-bit address device.
 *   addr:  7-bit slave address (unshifted)
 *   buf:   buffer to store received bytes
 *   len:   number of bytes to read
 *
 * Returns LL_I2C_OK on success, LL_I2C_NACK or LL_I2C_ERROR on failure.
 */
static inline int ll_i2c_read(I2C_TypeDef *i2c, uint8_t addr,
                              uint8_t *buf, uint32_t len)
{
    i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF
             | LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF;

    /* Configure transfer: 7-bit addr, READ, NBYTES, AUTOEND, START */
    i2c->CR2 = ((uint32_t)addr << LL_I2C_CR2_SADD_SHIFT)
             | LL_I2C_CR2_RD_WRN
             | (len << LL_I2C_CR2_NBYTES_SHIFT)
             | LL_I2C_CR2_AUTOEND
             | LL_I2C_CR2_START;

    for (uint32_t i = 0; i < len; i++) {
        while (1) {
            uint32_t isr = i2c->ISR;
            if (isr & LL_I2C_ISR_NACKF) {
                i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF;
                return LL_I2C_NACK;
            }
            if (isr & (LL_I2C_ISR_BERR | LL_I2C_ISR_ARLO)) {
                i2c->ICR = LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF | LL_I2C_ICR_STOPCF;
                return LL_I2C_ERROR;
            }
            if (isr & LL_I2C_ISR_RXNE)
                break;
        }
        buf[i] = (uint8_t)i2c->RXDR;
    }

    while (!(i2c->ISR & LL_I2C_ISR_STOPF))
        ;
    i2c->ICR = LL_I2C_ICR_STOPCF;

    return LL_I2C_OK;
}

/* ============================================================
 * Register write/read helpers (common I2C device patterns)
 * ============================================================ */

/**
 * Write to a register on an I2C device.
 *   addr:  7-bit slave address
 *   reg:   register address
 *   data:  pointer to data bytes
 *   len:   number of data bytes
 */
static inline int ll_i2c_write_reg(I2C_TypeDef *i2c, uint8_t addr,
                                   uint8_t reg, const uint8_t *data, uint32_t len)
{
    i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF
             | LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF;

    /* Total bytes = 1 (register) + len (data) */
    uint32_t total = 1 + len;
    i2c->CR2 = ((uint32_t)addr << LL_I2C_CR2_SADD_SHIFT)
             | (total << LL_I2C_CR2_NBYTES_SHIFT)
             | LL_I2C_CR2_AUTOEND
             | LL_I2C_CR2_START;

    /* Send register address first */
    while (!(i2c->ISR & LL_I2C_ISR_TXIS)) {
        if (i2c->ISR & LL_I2C_ISR_NACKF) {
            i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF;
            return LL_I2C_NACK;
        }
    }
    i2c->TXDR = reg;

    /* Send data */
    for (uint32_t i = 0; i < len; i++) {
        while (!(i2c->ISR & LL_I2C_ISR_TXIS)) {
            if (i2c->ISR & LL_I2C_ISR_NACKF) {
                i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF;
                return LL_I2C_NACK;
            }
        }
        i2c->TXDR = data[i];
    }

    while (!(i2c->ISR & LL_I2C_ISR_STOPF))
        ;
    i2c->ICR = LL_I2C_ICR_STOPCF;

    return LL_I2C_OK;
}

/**
 * Read from a register on an I2C device (write reg addr, then read).
 *   addr:  7-bit slave address
 *   reg:   register address
 *   buf:   buffer for received data
 *   len:   number of bytes to read
 */
static inline int ll_i2c_read_reg(I2C_TypeDef *i2c, uint8_t addr,
                                  uint8_t reg, uint8_t *buf, uint32_t len)
{
    int ret;

    i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF
             | LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF;

    /* Phase 1: Write register address (no AUTOEND — we need a restart) */
    i2c->CR2 = ((uint32_t)addr << LL_I2C_CR2_SADD_SHIFT)
             | (1UL << LL_I2C_CR2_NBYTES_SHIFT)
             | LL_I2C_CR2_START;

    while (!(i2c->ISR & LL_I2C_ISR_TXIS)) {
        if (i2c->ISR & LL_I2C_ISR_NACKF) {
            i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF;
            return LL_I2C_NACK;
        }
    }
    i2c->TXDR = reg;

    /* Wait for transfer complete (TC, not TCR since no RELOAD) */
    while (!(i2c->ISR & LL_I2C_ISR_TC))
        ;

    /* Phase 2: Read data with repeated START */
    ret = ll_i2c_read(i2c, addr, buf, len);

    return ret;
}

/* ============================================================
 * Bus scan (useful for debugging)
 * ============================================================ */

/**
 * Check if a device responds at the given 7-bit address.
 * Returns LL_I2C_OK if ACK received, LL_I2C_NACK otherwise.
 */
static inline int ll_i2c_probe(I2C_TypeDef *i2c, uint8_t addr)
{
    i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF
             | LL_I2C_ICR_BERRCF | LL_I2C_ICR_ARLOCF;

    /* Send START + address with 0 bytes, AUTOEND */
    i2c->CR2 = ((uint32_t)addr << LL_I2C_CR2_SADD_SHIFT)
             | (0UL << LL_I2C_CR2_NBYTES_SHIFT)
             | LL_I2C_CR2_AUTOEND
             | LL_I2C_CR2_START;

    /* Wait for STOP (AUTOEND) */
    while (!(i2c->ISR & LL_I2C_ISR_STOPF))
        ;

    int result = (i2c->ISR & LL_I2C_ISR_NACKF) ? LL_I2C_NACK : LL_I2C_OK;

    i2c->ICR = LL_I2C_ICR_NACKCF | LL_I2C_ICR_STOPCF;

    return result;
}

/* ============================================================
 * Enable / Disable
 * ============================================================ */

static inline void ll_i2c_enable(I2C_TypeDef *i2c)
{
    SET_BITS(i2c->CR1, LL_I2C_CR1_PE);
}

static inline void ll_i2c_disable(I2C_TypeDef *i2c)
{
    CLR_BITS(i2c->CR1, LL_I2C_CR1_PE);
}

#endif /* LL_I2C_H */
