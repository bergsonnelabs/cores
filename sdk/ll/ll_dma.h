/**
 * ll_dma.h — Low-level DMA operations
 *
 * Channel configuration, memory↔peripheral transfers, circular mode.
 *
 * Two fundamentally different DMA IPs:
 *   - L0/L4: Classic DMA with 7 channels, request multiplexing via CSELR
 *   - WBA/H5: GPDMA with linked-list capable channels
 *
 * For basic single-block transfers use ll_gpdma_config() (or ll_dma_config()
 * on L0/L4). The GPDMA also supports linked-list (LLI) chains via
 * ll_gpdma_node_t + ll_gpdma_node_init/link/terminate/list_start — used for
 * variable-size, alternating, or looping transfer sequences. 2D addressing
 * (CTR3/CBR2) is not wrapped; use registers directly if needed.
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

/*
 * Struct offsets are relative to the channel base (GPDMA1_BASE + 0x50 + 0x80*x),
 * so the comment after each field is the absolute register offset per RM0493
 * §17.8.6–17.8.15. Note the gap between CDAR (0xA0) and CLLR (0xCC): it spans
 * the 2D-only registers CTR3 (0xA4) and CBR2 (0xA8) plus reserved words — hence
 * _RESERVED2[10], not [6]. (An earlier [6] put CLLR at 0xBC, which only went
 * unnoticed because nothing used the linked-list register until now.)
 */
typedef struct {
    volatile uint32_t CLBAR;    /* base+0x00  abs 0x50: Linked-list base address */
    uint32_t _RESERVED0[2];     /*      0x04, 0x08 */
    volatile uint32_t CFCR;     /* base+0x0C  abs 0x5C: Flag clear register */
    volatile uint32_t CSR;      /* base+0x10  abs 0x60: Channel status */
    volatile uint32_t CCR;      /* base+0x14  abs 0x64: Channel control */
    uint32_t _RESERVED1[10];    /*      0x18..0x3C */
    volatile uint32_t CTR1;     /* base+0x40  abs 0x90: Transfer register 1 */
    volatile uint32_t CTR2;     /* base+0x44  abs 0x94: Transfer register 2 */
    volatile uint32_t CBR1;     /* base+0x48  abs 0x98: Block register 1 */
    volatile uint32_t CSAR;     /* base+0x4C  abs 0x9C: Source address */
    volatile uint32_t CDAR;     /* base+0x50  abs 0xA0: Destination address */
    uint32_t _RESERVED2[10];    /*      0x54..0x7B  (CTR3 0xA4, CBR2 0xA8, reserved) */
    volatile uint32_t CLLR;     /* base+0x7C  abs 0xCC: Linked-list register */
} GPDMA_Channel_TypeDef;

/* Guard the register layout against RM0493 §17.8 (offsets are channel-relative;
 * channel base = abs 0x50). Catches struct-padding mistakes at compile time. */
_Static_assert(__builtin_offsetof(GPDMA_Channel_TypeDef, CCR)  == 0x14, "GPDMA CCR offset");
_Static_assert(__builtin_offsetof(GPDMA_Channel_TypeDef, CTR1) == 0x40, "GPDMA CTR1 offset");
_Static_assert(__builtin_offsetof(GPDMA_Channel_TypeDef, CBR1) == 0x48, "GPDMA CBR1 offset");
_Static_assert(__builtin_offsetof(GPDMA_Channel_TypeDef, CDAR) == 0x50, "GPDMA CDAR offset");
_Static_assert(__builtin_offsetof(GPDMA_Channel_TypeDef, CLLR) == 0x7C, "GPDMA CLLR offset");

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
#define LL_GPDMA_CCR_USEIE      (1UL << 12)   /* User setting error IE */
#define LL_GPDMA_CCR_SUSPIE     (1UL << 13)   /* Completed suspension IE */
#define LL_GPDMA_CCR_TOIE       (1UL << 14)   /* Trigger overrun IE */
#define LL_GPDMA_CCR_LSM        (1UL << 16)   /* Link step mode (0 = run-to-completion) */
#define LL_GPDMA_CCR_LAP        (1UL << 17)   /* Link allocated port */
/* Priority level PRIO[23:22] */
#define LL_GPDMA_CCR_PRIO_LOW_LOW   (0x0UL << 22)
#define LL_GPDMA_CCR_PRIO_LOW_MID   (0x1UL << 22)
#define LL_GPDMA_CCR_PRIO_LOW_HIGH  (0x2UL << 22)
#define LL_GPDMA_CCR_PRIO_HIGH      (0x3UL << 22)

/* ---- CSR bit definitions ---- */

#define LL_GPDMA_CSR_IDLEF      (1UL << 0)    /* Idle flag */
#define LL_GPDMA_CSR_TCF        (1UL << 8)    /* Transfer complete flag */
#define LL_GPDMA_CSR_HTF        (1UL << 9)    /* Half transfer flag */
#define LL_GPDMA_CSR_DTEF       (1UL << 10)   /* Data transfer error flag */
#define LL_GPDMA_CSR_ULEF       (1UL << 11)   /* Update link error flag */
#define LL_GPDMA_CSR_USEF       (1UL << 12)   /* User setting error flag */
#define LL_GPDMA_CSR_SUSPF      (1UL << 13)   /* Completed suspension flag */
#define LL_GPDMA_CSR_TOF        (1UL << 14)   /* Trigger overrun flag */

/* ---- CFCR bit definitions ---- */

#define LL_GPDMA_CFCR_TCF       (1UL << 8)    /* Clear TC flag */
#define LL_GPDMA_CFCR_HTF       (1UL << 9)    /* Clear HT flag */
#define LL_GPDMA_CFCR_DTEF      (1UL << 10)   /* Clear DTE flag */
#define LL_GPDMA_CFCR_ULEF      (1UL << 11)   /* Clear update-link-error flag */
#define LL_GPDMA_CFCR_USEF      (1UL << 12)   /* Clear user-setting-error flag */
#define LL_GPDMA_CFCR_SUSPF     (1UL << 13)   /* Clear suspension flag */
#define LL_GPDMA_CFCR_TOF       (1UL << 14)   /* Clear trigger-overrun flag */
#define LL_GPDMA_CFCR_ALL       (LL_GPDMA_CFCR_TCF  | LL_GPDMA_CFCR_HTF  | \
                                 LL_GPDMA_CFCR_DTEF | LL_GPDMA_CFCR_ULEF | \
                                 LL_GPDMA_CFCR_USEF | LL_GPDMA_CFCR_SUSPF | \
                                 LL_GPDMA_CFCR_TOF)

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

#define LL_GPDMA_CTR2_REQSEL_SHIFT  0         /* Request selection REQSEL[5:0] */
#define LL_GPDMA_CTR2_REQSEL_MASK   (0x3FUL << 0)
#define LL_GPDMA_CTR2_SWREQ     (1UL << 9)    /* Software request (mem-to-mem) */
#define LL_GPDMA_CTR2_DREQ      (1UL << 10)   /* Dest is HW request (mem→periph) */
/* If DREQ=0, source is HW request (periph→mem) */
#define LL_GPDMA_CTR2_BREQ      (1UL << 11)   /* Block hardware request */
/* Trigger mode TRIGM[15:14] / select TRIGSEL[20:16] / polarity TRIGPOL[25:24] */
#define LL_GPDMA_CTR2_TRIGM_SHIFT   14
#define LL_GPDMA_CTR2_TRIGSEL_SHIFT 16
#define LL_GPDMA_CTR2_TRIGPOL_SHIFT 24
/* Transfer-complete event mode TCEM[31:30]: when does TCF/HTF assert */
#define LL_GPDMA_CTR2_TCEM_BLOCK    (0x0UL << 30)  /* at end of each block */
#define LL_GPDMA_CTR2_TCEM_LLI      (0x2UL << 30)  /* at end of each linked-list item */
#define LL_GPDMA_CTR2_TCEM_CHANNEL  (0x3UL << 30)  /* only at end of last LLI */

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
    ch->CFCR = LL_GPDMA_CFCR_ALL;
}

/* ============================================================
 * GPDMA linked-list (LLI) support
 *
 * GPDMA can execute a chain of transfers ("linked-list items", LLIs)
 * without CPU intervention: each LLI is a small descriptor in memory that
 * the engine loads into the channel registers, runs, then follows a link
 * to the next descriptor. This is what makes variable-size, alternating,
 * looping transfer sequences (e.g. a camera frame protocol) possible.
 *
 * We use the *static* linear-node form (RM0493 §17.4.5, Figure 53): every
 * LLI updates all of CTR1/CTR2/CBR1/CSAR/CDAR/CLLR, so each node is six
 * contiguous 32-bit words in the exact order the engine fetches them.
 *
 * Constraints (RM0493 §17.4.5):
 *   - Nodes must be 32-bit aligned.
 *   - CLBAR supplies address bits [31:16]; the per-node link supplies
 *     [15:2]. So ALL nodes of one list must live in the same aligned
 *     64-Kbyte region. A single static array in SRAM satisfies this.
 * ============================================================ */

/* ---- CLLR (linked-list register) bit definitions ---- */

#define LL_GPDMA_CLLR_UT1       (1UL << 31)   /* Update CTR1 from next node */
#define LL_GPDMA_CLLR_UT2       (1UL << 30)   /* Update CTR2 from next node */
#define LL_GPDMA_CLLR_UB1       (1UL << 29)   /* Update CBR1 from next node */
#define LL_GPDMA_CLLR_USA       (1UL << 28)   /* Update CSAR from next node */
#define LL_GPDMA_CLLR_UDA       (1UL << 27)   /* Update CDAR from next node */
#define LL_GPDMA_CLLR_ULL       (1UL << 16)   /* Update CLLR from next node */
#define LL_GPDMA_CLLR_LA_MASK   (0xFFFCUL)    /* LA[15:2]: next node low address */

/* All update bits for a full static linear node */
#define LL_GPDMA_CLLR_UPDATE_ALL  (LL_GPDMA_CLLR_UT1 | LL_GPDMA_CLLR_UT2 | \
                                   LL_GPDMA_CLLR_UB1 | LL_GPDMA_CLLR_USA | \
                                   LL_GPDMA_CLLR_UDA | LL_GPDMA_CLLR_ULL)

/* ---- LLI node (static linear form) ---- */
/*
 * Field order is fixed by hardware (CTR1, CTR2, CBR1, CSAR, CDAR, CLLR) —
 * do not reorder. Place arrays of these in SRAM within one 64-Kbyte region.
 */
typedef struct {
    uint32_t CTR1;
    uint32_t CTR2;
    uint32_t CBR1;   /* BNDT[15:0]: byte count */
    uint32_t CSAR;
    uint32_t CDAR;
    uint32_t CLLR;   /* link to next node (set via ll_gpdma_node_link/terminate) */
} ll_gpdma_node_t;

_Static_assert(sizeof(ll_gpdma_node_t) == 24, "LLI node must be 6 packed words");

/**
 * Fill an LLI node for a peripheral↔memory or memory↔memory transfer.
 * Leaves the node unlinked (terminator) — call ll_gpdma_node_link() or
 * ll_gpdma_node_terminate() afterwards.
 *   src/dst: source/destination addresses
 *   count:   byte count (BNDT, max 65535)
 *   ctr1:    data widths + increment flags (LL_GPDMA_CTR1_*)
 *   ctr2:    request/direction + TCEM (LL_GPDMA_CTR2_*)
 */
static inline void ll_gpdma_node_init(ll_gpdma_node_t *node,
                                      volatile void *src, void *dst,
                                      uint32_t count,
                                      uint32_t ctr1, uint32_t ctr2)
{
    node->CTR1 = ctr1;
    node->CTR2 = ctr2;
    node->CBR1 = count;
    node->CSAR = (uint32_t)src;
    node->CDAR = (uint32_t)dst;
    node->CLLR = 0;                 /* terminator until explicitly linked */
}

/** Link node `a` so the engine runs `next` after it (full static update). */
static inline void ll_gpdma_node_link(ll_gpdma_node_t *a,
                                      const ll_gpdma_node_t *next)
{
    a->CLLR = LL_GPDMA_CLLR_UPDATE_ALL |
              ((uint32_t)next & LL_GPDMA_CLLR_LA_MASK);
}

/** Make node `a` the end of a run-to-completion list (channel stops after it). */
static inline void ll_gpdma_node_terminate(ll_gpdma_node_t *a)
{
    a->CLLR = 0;
}

/**
 * Arm a channel on a linked list and enable it.
 *
 * Uses the "null initial LLI" entry (RM0493 §17.4.6 Note): the channel's own
 * register file describes a zero-length transfer whose CLLR points at `first`
 * with all update bits set, so the engine immediately loads and runs `first`.
 * A zero-length initial transfer raises no spurious TC/HT event.
 *
 * Run-to-completion mode (LSM=0). For a one-shot list, terminate the last node
 * with ll_gpdma_node_terminate(); for a circular list, link the last node back
 * to the loop-start node with ll_gpdma_node_link().
 *
 *   ccr_flags: interrupt-enable + priority bits (LL_GPDMA_CCR_*), EN excluded.
 *
 * Caller must enable the GPDMA clock (ll_rcc_dma1_clk_enable) and, for an
 * interrupt-driven list, configure the channel's NVIC line beforehand.
 */
static inline void ll_gpdma_list_start(GPDMA_Channel_TypeDef *ch,
                                       const ll_gpdma_node_t *first,
                                       uint32_t ccr_flags)
{
    ch->CCR  = 0;                                       /* ensure disabled */
    ll_gpdma_clear_flags(ch);
    ch->CLBAR = (uint32_t)first & 0xFFFF0000UL;         /* node region base */
    ch->CTR1 = 0;
    ch->CTR2 = 0;
    ch->CBR1 = 0;                                       /* null initial transfer */
    ch->CSAR = 0;
    ch->CDAR = 0;
    ch->CLLR = LL_GPDMA_CLLR_UPDATE_ALL |
               ((uint32_t)first & LL_GPDMA_CLLR_LA_MASK);
    ch->CCR  = ccr_flags;                               /* IRQs + priority, EN=0 */
    __asm__ volatile ("dmb 0xF" ::: "memory");          /* nodes visible before EN */
    SET_BITS(ch->CCR, LL_GPDMA_CCR_EN);
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
