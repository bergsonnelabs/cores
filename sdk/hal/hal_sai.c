/**
 * hal_sai.c — SAI1 PDM master receiver + GPDMA circular capture (WBA55).
 *
 * Register sequence distilled from ST's HAL_SAI_Init / HAL_SAI_Receive_DMA for
 * the PDM path; data packing (1 bit/clock, MSB-first into 32-bit words) per
 * RM0493 §SAI. Register defs in ll_sai.h; GPDMA LLI in ll_dma.h.
 *
 * NOTE: not yet silicon-validated. The PDM clock math, CKEN line, and the
 * GPDMA self-linked circular descriptor want an on-hardware check (see the
 * MIC_PDM harness step).
 */
#include "hal_sai.h"

/* SAI exists on W5 / L4 / H5, not L0. Empty translation unit elsewhere. */
#if defined(STM32WBA55xx) || defined(STM32L422xx) || defined(STM32H523xx)

/* SAI1 GPDMA hardware request line (RM0493 Table; SAI1_A). */
#define HAL_SAI_GPDMA_REQ_SAI1_A 17u

/* SAI1 peripheral clock enable: RCC APB2ENR (offset 0x0A4) bit 21. */
static inline void sai1_clk_enable(void)
{
    SET_BITS(REG32(RCC_BASE + 0x0A4UL), (1UL << 21));
    (void)REG32(RCC_BASE + 0x0A4UL);
}

hal_status_t hal_sai_pdm_init(hal_sai_t *s, SAI_Block_TypeDef *block,
                              const hal_sai_pdm_config_t *cfg)
{
    /* PDM is SAI1 block A only; mono or one stereo pair; CK1 or CK2. */
    if (block != SAI1_Block_A || cfg == NULL) return HAL_ERROR;
    if (cfg->mics < 1 || cfg->mics > 2) return HAL_ERROR;
    if (cfg->clock_line != 1 && cfg->clock_line != 2) return HAL_ERROR;
    if (cfg->kernel_clk_hz == 0 || cfg->pcm_rate_hz == 0) return HAL_ERROR;

    s->block        = block;
    s->dma_ch       = NULL;
    s->active       = false;
    s->ready_half   = 0;

    sai1_clk_enable();

    /* Master clock divider: MCKDIV = Fsai / (Fs * 256) with NODIV=0, OSR=0.
     * Rounds to nearest; 1..63. (12.288 MHz / (16 kHz * 256) = 3 exactly.) */
    uint32_t mckdiv = (cfg->kernel_clk_hz + (cfg->pcm_rate_hz * 256u) / 2u) /
                      (cfg->pcm_rate_hz * 256u);
    if (mckdiv < 1) mckdiv = 1;
    if (mckdiv > 63) mckdiv = 63;

    /* Block must be off before (re)configuring; GCR with both blocks idle. */
    ll_sai_block_disable(block);
    SAI1->GCR = 0;

    /* CR1: master RX, free protocol, 32-bit data, MSB-first, RX clock strobe
     * on rising edge (CKSTR=1), async, mono if 1 mic, /256 divider (NODIV=0),
     * OSR off, no MCLK output. */
    uint32_t cr1 = SAI_CR1_MODE_MASTER_RX
                 | SAI_CR1_PRTCFG_FREE
                 | SAI_CR1_DS_32
                 | SAI_CR1_CKSTR
                 | ((mckdiv & 0x3FUL) << SAI_CR1_MCKDIV_Pos);
    if (cfg->mics == 1) cr1 |= SAI_CR1_MONO;
    block->CR1 = cr1;

    /* CR2: FIFO threshold half-full for DMA; no companding, no flush at init. */
    block->CR2 = SAI_CR2_FTH_HF;

    /* FRCR: one 32-bit slot → frame length 32, so FRL field = 31. FS fields are
     * don't-cares for PDM but must keep FrameLength = NbSlot * SlotSize. */
    block->FRCR = (32u - 1u) & SAI_FRCR_FRL;

    /* SLOTR: 32-bit slots, FBOFF=0, NBSLOT = mics (field = mics-1), enable the
     * active slot(s). 1 mic → slot 0; 2 mics → slots 0+1. */
    uint32_t sloten = (cfg->mics == 2) ? 0x3u : 0x1u;   /* enable slot bitmap */
    block->SLOTR = SAI_SLOTR_SLOTSZ_32
                 | (((uint32_t)cfg->mics - 1u) << SAI_SLOTR_NBSLOT_Pos)
                 | (sloten << SAI_SLOTR_SLOTEN_Pos);

    /* PDMCR: select the clock line, MICNBR = pairs-1 = 0 (one L/R pair covers
     * 1 or 2 mics), write config first, then enable PDMEN last. */
    uint32_t cken = (cfg->clock_line == 2) ? SAI_PDMCR_CKEN2 : SAI_PDMCR_CKEN1;
    SAI1->PDMCR &= ~SAI_PDMCR_PDMEN;
    SAI1->PDMCR  = cken;                 /* MICNBR = 0 */
    SAI1->PDMDLY = 0;                    /* no per-mic deskew for a single pair */
    SAI1->PDMCR |= SAI_PDMCR_PDMEN;

    return HAL_OK;
}

hal_status_t hal_sai_capture_start(hal_sai_t *s, GPDMA_Channel_TypeDef *ch,
                                   uint8_t *buf, uint32_t len,
                                   hal_callback_t callback, void *ctx)
{
    if (s->active) return HAL_BUSY;
    if (s->block == NULL || ch == NULL || buf == NULL || len == 0 || (len & 1u))
        return HAL_ERROR;

    s->dma_ch       = ch;
    s->buf          = buf;
    s->len          = len;
    s->callback     = callback;
    s->callback_ctx = ctx;
    s->ready_half   = 0;

    ll_rcc_dma1_clk_enable();

    /* Periph→mem: source = SAI DR (no increment, 32-bit), dest = buffer
     * (increment, 32-bit). HW request = SAI1_A; TCEM at block end so TCF marks
     * full-buffer and HTF marks half-buffer. */
    uint32_t ctr1 = LL_GPDMA_CTR1_SDW_WORD | LL_GPDMA_CTR1_DDW_WORD | LL_GPDMA_CTR1_DINC;
    uint32_t ctr2 = (HAL_SAI_GPDMA_REQ_SAI1_A << LL_GPDMA_CTR2_REQSEL_SHIFT)
                  | LL_GPDMA_CTR2_TCEM_BLOCK;   /* DREQ=0 → source is the HW request */

    ll_gpdma_node_init(&s->node, (volatile void *)&s->block->DR, buf, len, ctr1, ctr2);
    ll_gpdma_node_link(&s->node, &s->node);     /* self-link → circular */

    s->active = true;
    ll_gpdma_list_start(ch, &s->node,
                        LL_GPDMA_CCR_HTIE | LL_GPDMA_CCR_TCIE | LL_GPDMA_CCR_DTEIE |
                        LL_GPDMA_CCR_PRIO_HIGH);

    /* DMA requests on, then enable the block (SAIEN last → starts the clock). */
    ll_sai_block_enable_dma(s->block);
    ll_sai_block_enable(s->block);
    return HAL_OK;
}

void hal_sai_capture_stop(hal_sai_t *s)
{
    if (!s->active) return;
    ll_sai_block_disable(s->block);
    if (s->dma_ch) ll_gpdma_disable(s->dma_ch);
    s->active = false;
}

void hal_sai_irq_handler(hal_sai_t *s)
{
    GPDMA_Channel_TypeDef *ch = s->dma_ch;
    if (ch == NULL) return;
    uint32_t csr = ch->CSR;

    if (csr & LL_GPDMA_CSR_HTF) {        /* lower half filled */
        ch->CFCR = LL_GPDMA_CFCR_HTF;
        s->ready_half = 0;
        if (s->callback) s->callback(s->callback_ctx);
    }
    if (csr & LL_GPDMA_CSR_TCF) {        /* upper half filled */
        ch->CFCR = LL_GPDMA_CFCR_TCF;
        s->ready_half = 1;
        if (s->callback) s->callback(s->callback_ctx);
    }
    if (csr & LL_GPDMA_CSR_DTEF) {       /* transfer error — clear + flag overrun */
        ch->CFCR = LL_GPDMA_CFCR_DTEF;
        ll_sai_clear_overrun(s->block);
    }
}

#endif /* SAI-capable core */
