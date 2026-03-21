/**
 * ll_dma.h — Low-level DMA operations
 *
 * Channel configuration, memory↔peripheral transfers, circular mode.
 *
 * Two fundamentally different DMA IPs:
 *   - L0/L4: Classic DMA with 7 channels, request multiplexing via CSELR
 *   - WBA/H5: GPDMA with linked-list capable channels
 *
 * This driver provides a common API for basic transfers. For advanced
 * GPDMA features (linked lists, 2D addressing), use registers directly.
 */

#ifndef LL_DMA_H
#define LL_DMA_H

#include "ll_common.h"

/* ============================================================
 * Classic DMA (L0, L4)
 * ============================================================ */

#if defined(STM32L011xx) || defined(STM32L422xx)

/* ---- DMA global registers ---- */

#define DMA1_BASE           0x40020000UL
#define DMA1_ISR            REG32(DMA1_BASE + 0x00UL)
#define DMA1_IFCR           REG32(DMA1_BASE + 0x04UL)

#if defined(STM32L422xx)
  #define DMA1_CSELR        REG32(DMA1_BASE + 0xA8UL)  /* Channel selection */
#endif

/* ---- DMA channel register structure ---- */

typedef struct {
    volatile uint32_t CCR;      /* 0x00: Channel configuration */
    volatile uint32_t CNDTR;    /* 0x04: Number of data to transfer */
    volatile uint32_t CPAR;     /* 0x08: Peripheral address */
    volatile uint32_t CMAR;     /* 0x0C: Memory address */
    volatile uint32_t _RESERVED;
} DMA_Channel_TypeDef;

/* Channel instances (each channel is 0x14 bytes, starting at offset 0x08) */
#define DMA1_CH1    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x08UL))
#define DMA1_CH2    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x1CUL))
#define DMA1_CH3    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x30UL))
#define DMA1_CH4    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x44UL))
#define DMA1_CH5    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x58UL))
#define DMA1_CH6    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x6CUL))
#define DMA1_CH7    ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x80UL))

/* ---- CCR bit definitions ---- */

#define LL_DMA_CCR_EN           (1UL << 0)    /* Channel enable */
#define LL_DMA_CCR_TCIE         (1UL << 1)    /* Transfer complete interrupt */
#define LL_DMA_CCR_HTIE         (1UL << 2)    /* Half transfer interrupt */
#define LL_DMA_CCR_TEIE         (1UL << 3)    /* Transfer error interrupt */
#define LL_DMA_CCR_DIR          (1UL << 4)    /* Direction: 1=read from memory */
#define LL_DMA_CCR_CIRC         (1UL << 5)    /* Circular mode */
#define LL_DMA_CCR_PINC         (1UL << 6)    /* Peripheral increment */
#define LL_DMA_CCR_MINC         (1UL << 7)    /* Memory increment */

/* Peripheral data size */
#define LL_DMA_CCR_PSIZE_8      (0x0UL << 8)
#define LL_DMA_CCR_PSIZE_16     (0x1UL << 8)
#define LL_DMA_CCR_PSIZE_32     (0x2UL << 8)

/* Memory data size */
#define LL_DMA_CCR_MSIZE_8      (0x0UL << 10)
#define LL_DMA_CCR_MSIZE_16     (0x1UL << 10)
#define LL_DMA_CCR_MSIZE_32     (0x2UL << 10)

/* Priority level */
#define LL_DMA_CCR_PL_LOW       (0x0UL << 12)
#define LL_DMA_CCR_PL_MED       (0x1UL << 12)
#define LL_DMA_CCR_PL_HIGH      (0x2UL << 12)
#define LL_DMA_CCR_PL_VHIGH     (0x3UL << 12)

/* Memory-to-memory mode */
#define LL_DMA_CCR_MEM2MEM      (1UL << 14)

/* ---- ISR/IFCR flag positions per channel ---- */
/* Each channel occupies 4 bits: GIF, TCIF, HTIF, TEIF */

#define LL_DMA_ISR_GIF(ch)      (1UL << (((ch) - 1) * 4))
#define LL_DMA_ISR_TCIF(ch)     (1UL << (((ch) - 1) * 4 + 1))
#define LL_DMA_ISR_HTIF(ch)     (1UL << (((ch) - 1) * 4 + 2))
#define LL_DMA_ISR_TEIF(ch)     (1UL << (((ch) - 1) * 4 + 3))

/* ---- Channel number helper ---- */

static inline uint32_t ll_dma_channel_num(DMA_Channel_TypeDef *ch)
{
    return (((uint32_t)ch - DMA1_BASE - 0x08UL) / 0x14UL) + 1;
}

/* ---- Request mapping (L4 CSELR) ---- */

#if defined(STM32L422xx)
/**
 * Set the DMA request source for a channel.
 *   ch:      DMA channel instance
 *   request: request number (0-7, see reference manual Table 41)
 *
 * Common request mappings for STM32L422:
 *   ADC1:       request 0 on any channel
 *   SPI1_RX:    request 1  |  SPI1_TX:    request 1
 *   USART1_RX:  request 2  |  USART1_TX:  request 2
 *   USART2_RX:  request 2  |  USART2_TX:  request 2
 *   I2C1_RX:    request 3  |  I2C1_TX:    request 3
 *   TIM2:       request 4
 */
static inline void ll_dma_set_request(DMA_Channel_TypeDef *ch, uint32_t request)
{
    uint32_t num = ll_dma_channel_num(ch);
    uint32_t shift = (num - 1) * 4;
    MOD_BITS(DMA1_CSELR, 0xFUL << shift, request << shift);
}
#endif

/* ---- Configuration ---- */

/**
 * Configure a DMA channel for peripheral↔memory transfer.
 *   ch:        DMA channel instance (DMA1_CH1..DMA1_CH7)
 *   periph:    peripheral register address (e.g. &USART1->RDR)
 *   mem:       memory buffer address
 *   count:     number of data items to transfer
 *   ccr_flags: ORed CCR flags (direction, sizes, increment, circular, etc.)
 *
 * Channel must be disabled before calling. Use ll_dma_disable() first.
 */
static inline void ll_dma_config(DMA_Channel_TypeDef *ch,
                                 volatile void *periph, void *mem,
                                 uint32_t count, uint32_t ccr_flags)
{
    ch->CCR = 0;                                /* Disable and reset */
    ch->CPAR = (uint32_t)periph;
    ch->CMAR = (uint32_t)mem;
    ch->CNDTR = count;
    ch->CCR = ccr_flags;                        /* Configure (don't enable yet) */
}

/** Enable a DMA channel (start transfer) */
static inline void ll_dma_enable(DMA_Channel_TypeDef *ch)
{
    SET_BITS(ch->CCR, LL_DMA_CCR_EN);
}

/** Disable a DMA channel */
static inline void ll_dma_disable(DMA_Channel_TypeDef *ch)
{
    CLR_BITS(ch->CCR, LL_DMA_CCR_EN);
}

/** Get remaining transfer count */
static inline uint32_t ll_dma_remaining(DMA_Channel_TypeDef *ch)
{
    return ch->CNDTR;
}

/** Check if transfer complete flag is set */
static inline int ll_dma_transfer_complete(DMA_Channel_TypeDef *ch)
{
    uint32_t num = ll_dma_channel_num(ch);
    return (DMA1_ISR & LL_DMA_ISR_TCIF(num)) != 0;
}

/** Check if half-transfer flag is set */
static inline int ll_dma_half_transfer(DMA_Channel_TypeDef *ch)
{
    uint32_t num = ll_dma_channel_num(ch);
    return (DMA1_ISR & LL_DMA_ISR_HTIF(num)) != 0;
}

/** Check if transfer error flag is set */
static inline int ll_dma_transfer_error(DMA_Channel_TypeDef *ch)
{
    uint32_t num = ll_dma_channel_num(ch);
    return (DMA1_ISR & LL_DMA_ISR_TEIF(num)) != 0;
}

/** Clear all flags for a channel */
static inline void ll_dma_clear_flags(DMA_Channel_TypeDef *ch)
{
    uint32_t num = ll_dma_channel_num(ch);
    DMA1_IFCR = LL_DMA_ISR_GIF(num) | LL_DMA_ISR_TCIF(num)
              | LL_DMA_ISR_HTIF(num) | LL_DMA_ISR_TEIF(num);
}

/* ============================================================
 * GPDMA (WBA, H5)
 * ============================================================ */

#elif defined(STM32WBA55xx) || defined(STM32H523xx)

/* GPDMA base addresses */
#if defined(STM32WBA55xx)
  #define GPDMA1_BASE       0x40020000UL
#elif defined(STM32H523xx)
  #define GPDMA1_BASE       0x40020000UL
#endif

/* ---- GPDMA channel register structure ---- */

typedef struct {
    volatile uint32_t CLBAR;    /* 0x00: Linked-list base address */
    uint32_t _RESERVED0[2];
    volatile uint32_t CFCR;     /* 0x0C: Flag clear register */
    volatile uint32_t CSR;      /* 0x10: Channel status */
    volatile uint32_t CCR;      /* 0x14: Channel control */
    uint32_t _RESERVED1[10];
    volatile uint32_t CTR1;     /* 0x40: Transfer register 1 */
    volatile uint32_t CTR2;     /* 0x44: Transfer register 2 */
    volatile uint32_t CBR1;     /* 0x48: Block register 1 */
    volatile uint32_t CSAR;     /* 0x4C: Source address */
    volatile uint32_t CDAR;     /* 0x50: Destination address */
    uint32_t _RESERVED2[6];
    volatile uint32_t CLLR;     /* 0x6C: Linked-list register */
    uint32_t _RESERVED3[4];
} GPDMA_Channel_TypeDef;

/* Channel instances (each channel is 0x80 bytes, starting at offset 0x50) */
#define GPDMA1_CH0  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x050UL))
#define GPDMA1_CH1  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x0D0UL))
#define GPDMA1_CH2  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x150UL))
#define GPDMA1_CH3  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x1D0UL))
#define GPDMA1_CH4  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x250UL))
#define GPDMA1_CH5  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x2D0UL))
#define GPDMA1_CH6  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x350UL))
#define GPDMA1_CH7  ((GPDMA_Channel_TypeDef *)(GPDMA1_BASE + 0x3D0UL))

/* ---- CCR bit definitions ---- */

#define LL_GPDMA_CCR_EN         (1UL << 0)    /* Channel enable */
#define LL_GPDMA_CCR_RESET      (1UL << 1)    /* Channel reset */
#define LL_GPDMA_CCR_SUSP       (1UL << 2)    /* Suspend request */
#define LL_GPDMA_CCR_TCIE       (1UL << 8)    /* Transfer complete IE */
#define LL_GPDMA_CCR_HTIE       (1UL << 9)    /* Half transfer IE */
#define LL_GPDMA_CCR_DTEIE      (1UL << 10)   /* Data transfer error IE */
#define LL_GPDMA_CCR_ULEIE      (1UL << 11)   /* Update link error IE */

/* ---- CSR bit definitions ---- */

#define LL_GPDMA_CSR_IDLEF      (1UL << 0)    /* Idle flag */
#define LL_GPDMA_CSR_TCF        (1UL << 8)    /* Transfer complete flag */
#define LL_GPDMA_CSR_HTF        (1UL << 9)    /* Half transfer flag */
#define LL_GPDMA_CSR_DTEF       (1UL << 10)   /* Data transfer error flag */

/* ---- CFCR bit definitions ---- */

#define LL_GPDMA_CFCR_TCF       (1UL << 8)    /* Clear TC flag */
#define LL_GPDMA_CFCR_HTF       (1UL << 9)    /* Clear HT flag */
#define LL_GPDMA_CFCR_DTEF      (1UL << 10)   /* Clear DTE flag */

/* ---- CTR1 bit definitions ---- */

/* Source data width */
#define LL_GPDMA_CTR1_SDW_BYTE  (0x0UL << 0)
#define LL_GPDMA_CTR1_SDW_HALF  (0x1UL << 0)
#define LL_GPDMA_CTR1_SDW_WORD  (0x2UL << 0)

/* Source increment */
#define LL_GPDMA_CTR1_SINC      (1UL << 3)

/* Destination data width */
#define LL_GPDMA_CTR1_DDW_BYTE  (0x0UL << 16)
#define LL_GPDMA_CTR1_DDW_HALF  (0x1UL << 16)
#define LL_GPDMA_CTR1_DDW_WORD  (0x2UL << 16)

/* Destination increment */
#define LL_GPDMA_CTR1_DINC      (1UL << 19)

/* ---- CTR2 bit definitions ---- */

#define LL_GPDMA_CTR2_REQSEL_SHIFT  0         /* Request selection [6:0] */
#define LL_GPDMA_CTR2_SWREQ     (1UL << 9)    /* Software request */
#define LL_GPDMA_CTR2_DREQ      (1UL << 10)   /* Dest is HW request (mem→periph) */
/* If DREQ=0, source is HW request (periph→mem) */

/* ---- Configuration ---- */

/**
 * Configure a GPDMA channel for a simple peripheral↔memory transfer.
 *   ch:      GPDMA channel instance
 *   src:     source address (peripheral or memory)
 *   dst:     destination address (peripheral or memory)
 *   count:   number of bytes to transfer
 *   request: hardware request number (see RM for mapping table)
 *   ctr1:    transfer config (data widths, increment flags)
 *   ctr2:    request config (DREQ direction, request selection)
 */
static inline void ll_gpdma_config(GPDMA_Channel_TypeDef *ch,
                                   volatile void *src, void *dst,
                                   uint32_t count,
                                   uint32_t ctr1, uint32_t ctr2)
{
    ch->CCR = 0;
    ch->CLBAR = 0;
    ch->CLLR = 0;                              /* No linked list */
    ch->CSAR = (uint32_t)src;
    ch->CDAR = (uint32_t)dst;
    ch->CBR1 = count;                          /* Byte count */
    ch->CTR1 = ctr1;
    ch->CTR2 = ctr2;
}

/** Enable a GPDMA channel */
static inline void ll_gpdma_enable(GPDMA_Channel_TypeDef *ch)
{
    SET_BITS(ch->CCR, LL_GPDMA_CCR_EN);
}

/** Disable a GPDMA channel */
static inline void ll_gpdma_disable(GPDMA_Channel_TypeDef *ch)
{
    SET_BITS(ch->CCR, LL_GPDMA_CCR_SUSP);     /* Request suspend */
    while (!(ch->CSR & LL_GPDMA_CSR_IDLEF))   /* Wait for idle */
        ;
    CLR_BITS(ch->CCR, LL_GPDMA_CCR_EN);
}

/** Check if transfer complete */
static inline int ll_gpdma_transfer_complete(GPDMA_Channel_TypeDef *ch)
{
    return (ch->CSR & LL_GPDMA_CSR_TCF) != 0;
}

/** Check if half transfer */
static inline int ll_gpdma_half_transfer(GPDMA_Channel_TypeDef *ch)
{
    return (ch->CSR & LL_GPDMA_CSR_HTF) != 0;
}

/** Check if transfer error */
static inline int ll_gpdma_transfer_error(GPDMA_Channel_TypeDef *ch)
{
    return (ch->CSR & LL_GPDMA_CSR_DTEF) != 0;
}

/** Clear all flags */
static inline void ll_gpdma_clear_flags(GPDMA_Channel_TypeDef *ch)
{
    ch->CFCR = LL_GPDMA_CFCR_TCF | LL_GPDMA_CFCR_HTF | LL_GPDMA_CFCR_DTEF;
}

#endif /* DMA IP version */

/* ============================================================
 * RCC clock enable for DMA
 * ============================================================ */

static inline void ll_rcc_dma1_clk_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x30UL), (1UL << 0));   /* AHBENR: DMA1EN */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x48UL), (1UL << 0));   /* AHB1ENR: DMA1EN */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x88UL), (1UL << 0));   /* AHB1ENR: GPDMA1EN */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x88UL), (1UL << 0));   /* AHB1ENR: GPDMA1EN */
#endif
    (void)REG32(RCC_BASE);
}

#endif /* LL_DMA_H */
