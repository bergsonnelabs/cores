/**
 * ll_rcc.h — Low-level RCC (Reset and Clock Control)
 *
 * Peripheral clock enable/disable, clock source selection, PLL
 * configuration, and flash latency. RCC register layouts differ
 * significantly across STM32 families, so this header provides a
 * unified API with per-family implementations.
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
  #define RCC_BASE          (AHB5_BASE + 0x0C00UL)  /* 0x46020C00 — RCC is on AHB5, not AHB1 */
#elif defined(STM32H523xx)
  #define RCC_BASE          (AHB1_BASE + 0x0C00UL)
#endif

/* ---- Flash base address (for wait state config) ---- */

#if defined(STM32L011xx)
  #define FLASH_BASE        (PERIPH_BASE + 0x00022000UL)
#elif defined(STM32L422xx)
  #define FLASH_BASE        (PERIPH_BASE + 0x00022000UL)
#elif defined(STM32WBA55xx)
  #define FLASH_BASE        (PERIPH_BASE + 0x00022000UL)  /* 0x40022000 — same AHB1 domain as L4 */
#elif defined(STM32H523xx)
  #define FLASH_BASE        (PERIPH_BASE + 0x08022000UL)
#endif

#define FLASH_ACR           REG32(FLASH_BASE + 0x00UL)

/* ============================================================
 * Clock source enable/ready
 * ============================================================ */

/* ---- HSI16 (16MHz internal, available on L0/L4/WBA) ---- */

#if defined(STM32L011xx)

static inline void ll_rcc_hsi16_enable(void)
{
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 0));  /* CR: HSION */
}
static inline int ll_rcc_hsi16_ready(void)
{
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 2)) != 0;  /* CR: HSIRDY */
}

#elif defined(STM32L422xx)

static inline void ll_rcc_hsi16_enable(void)
{
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 8));  /* CR: HSION */
}
static inline int ll_rcc_hsi16_ready(void)
{
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 10)) != 0;  /* CR: HSIRDY */
}

#elif defined(STM32WBA55xx)

static inline void ll_rcc_hsi16_enable(void)
{
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 8));  /* CR: HSION */
}
static inline int ll_rcc_hsi16_ready(void)
{
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 10)) != 0;  /* CR: HSIRDY */
}

#endif /* HSI16 */

/* ---- HSE (external oscillator) ---- */

static inline void ll_rcc_hse_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 16));  /* CR: HSEON */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 16));  /* CR: HSEON */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 16));  /* CR: HSEON */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 16));  /* CR: HSEON */
#endif
}

static inline int ll_rcc_hse_ready(void)
{
#if defined(STM32L011xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 17)) != 0;
#elif defined(STM32L422xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 17)) != 0;
#elif defined(STM32WBA55xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 17)) != 0;
#elif defined(STM32H523xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 17)) != 0;
#else
    return 0;
#endif
}

/* ---- HSI48 (48MHz internal, L4/H5) ---- */

#if defined(STM32L422xx) || defined(STM32H523xx)

static inline void ll_rcc_hsi48_enable(void)
{
#if defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x98UL), (1UL << 0));  /* CRRCR: HSI48ON */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 12));  /* CR: HSI48ON */
#endif
}

static inline int ll_rcc_hsi48_ready(void)
{
#if defined(STM32L422xx)
    return (REG32(RCC_BASE + 0x98UL) & (1UL << 1)) != 0;  /* CRRCR: HSI48RDY */
#elif defined(STM32H523xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 13)) != 0;  /* CR: HSI48RDY */
#else
    return 0;
#endif
}

#endif /* HSI48 */

/* ============================================================
 * Flash wait states
 * ============================================================ */

/**
 * Set flash read latency (wait states).
 * Must be set BEFORE increasing SYSCLK, or AFTER decreasing it.
 *
 * Typical values for VDD >= 2.7V:
 *   L0:   0 WS ≤ 16MHz, 1 WS ≤ 32MHz
 *   L4:   0 WS ≤ 16MHz, 1 WS ≤ 32MHz, 2 WS ≤ 48MHz, 3 WS ≤ 64MHz, 4 WS ≤ 80MHz
 *   WBA:  0 WS ≤ 32MHz, 1 WS ≤ 64MHz, 2 WS ≤ 96MHz, 3 WS ≤ 100MHz
 *   H5:   0 WS ≤ 32MHz, 1 WS ≤ 64MHz, 2 WS ≤ 96MHz, ... 5 WS ≤ 250MHz
 */
static inline void ll_flash_set_latency(uint32_t wait_states)
{
    MOD_BITS(FLASH_ACR, 0xFUL, wait_states);
    /* Wait until the new latency is effective */
    uint32_t timeout = 100000;
    while ((FLASH_ACR & 0xFUL) != wait_states && --timeout)
        ;
}

/**
 * Calculate required flash wait states from SYSCLK frequency.
 */
static inline uint32_t ll_flash_latency_for_mhz(uint32_t mhz)
{
#if defined(STM32L011xx)
    return (mhz > 16) ? 1 : 0;
#elif defined(STM32L422xx)
    if (mhz <= 16) return 0;
    if (mhz <= 32) return 1;
    if (mhz <= 48) return 2;
    if (mhz <= 64) return 3;
    return 4;  /* up to 80 MHz */
#elif defined(STM32WBA55xx)
    if (mhz <= 32) return 0;
    if (mhz <= 64) return 1;
    if (mhz <= 96) return 2;
    return 3;  /* up to 100 MHz */
#elif defined(STM32H523xx)
    if (mhz <= 32) return 0;
    if (mhz <= 64) return 1;
    if (mhz <= 96) return 2;
    if (mhz <= 128) return 3;
    if (mhz <= 160) return 4;
    return 5;  /* up to 250 MHz */
#else
    return 0;
#endif
}

/* ============================================================
 * Voltage scaling (must be set before increasing SYSCLK)
 * ============================================================ */

/**
 * Set voltage scaling range. Must be called BEFORE configuring
 * PLL or increasing SYSCLK above the current range's limit.
 *
 *   WBA55: Range 1 = up to 100MHz, Range 2 = up to 16MHz
 *   H5:    Range 0 = up to 250MHz, Range 1-3 for lower speeds
 *   L4:    Range 1 = up to 80MHz (default), Range 2 = up to 26MHz
 *   L0:    Range 1 = up to 32MHz, Range 2 = up to 16MHz, Range 3 = up to 4MHz
 */
static inline void ll_rcc_set_vos_range1(void)
{
#if defined(STM32WBA55xx)
    /* PWR_BASE on WBA55 = 0x46020800 (AHB5 domain) */
    #define PWR_BASE_WBA  (AHB5_BASE + 0x0800UL)
    /* PWR_VOSR at offset 0x0C, VOS bits [9:8], VOSRDY bit 15 */
    MOD_BITS(REG32(PWR_BASE_WBA + 0x0CUL), 0x3UL << 8, 0x1UL << 8);  /* VOS = 01 = Range 1 */
    /* Wait for voltage scaling ready */
    while (!(REG32(PWR_BASE_WBA + 0x0CUL) & (1UL << 15)))
        ;
#elif defined(STM32H523xx)
    /* PWR_VOSCR at offset 0x10, VOS bits [5:4] */
    #define PWR_BASE_H5  (0x44020800UL)
    MOD_BITS(REG32(PWR_BASE_H5 + 0x10UL), 0x3UL << 4, 0x3UL << 4);  /* VOS = 11 = Range 0 */
    while (!(REG32(PWR_BASE_H5 + 0x14UL) & (1UL << 3)))  /* VOSRDY in PWR_VOSSR */
        ;
#elif defined(STM32L422xx)
    /* Already in Range 1 after reset — nothing to do */
#elif defined(STM32L011xx)
    /* Already in Range 1 after reset — nothing to do */
#endif
}

/* ============================================================
 * PLL configuration
 * ============================================================ */

/* PLL source selection values */
#if defined(STM32L011xx)
  #define LL_RCC_PLLSRC_HSI16  0x0UL
  #define LL_RCC_PLLSRC_HSE    0x1UL
#elif defined(STM32L422xx)
  #define LL_RCC_PLLSRC_NONE   0x0UL
  #define LL_RCC_PLLSRC_MSI    0x1UL
  #define LL_RCC_PLLSRC_HSI16  0x2UL
  #define LL_RCC_PLLSRC_HSE    0x3UL
#elif defined(STM32WBA55xx)
  #define LL_RCC_PLLSRC_NONE   0x0UL
  #define LL_RCC_PLLSRC_HSI16  0x2UL
  #define LL_RCC_PLLSRC_HSE    0x3UL
#elif defined(STM32H523xx)
  #define LL_RCC_PLLSRC_NONE   0x0UL
  #define LL_RCC_PLLSRC_HSI48  0x1UL  /* Actually uses HSI for PLL */
  #define LL_RCC_PLLSRC_CSI    0x2UL
  #define LL_RCC_PLLSRC_HSE    0x3UL
#endif

/**
 * Configure and enable the main PLL.
 *   src:  PLL source (LL_RCC_PLLSRC_*)
 *   m:    input divider (1-based)
 *   n:    VCO multiplier
 *   r:    output divider for SYSCLK
 *
 * PLL must be disabled before calling this function.
 */
static inline void ll_rcc_pll_config(uint32_t src, uint32_t m, uint32_t n, uint32_t r)
{
#if defined(STM32L011xx)
    /* L0: RCC_CFGR bits [21:18]=PLLMUL, [16]=PLLSRC, no PLLM/PLLR
       This is a simpler PLL — output = src × MUL / DIV
       We'll handle this differently in the generated code */
    (void)src; (void)m; (void)n; (void)r;

#elif defined(STM32L422xx)
    /* L4: RCC_PLLCFGR at offset 0x0C
       [1:0] PLLSRC, [6:4] PLLM-1, [14:8] PLLN, [26:25] PLLR/2-1, [24] PLLREN */
    uint32_t val = src
                 | ((m - 1) << 4)
                 | (n << 8)
                 | (((r / 2) - 1) << 25)
                 | (1UL << 24);  /* PLLREN: enable R output */
    REG32(RCC_BASE + 0x0CUL) = val;

#elif defined(STM32WBA55xx)
    /* WBA: RCC_PLL1CFGR at offset 0x28, RCC_PLL1DIVR at offset 0x34
       CFGR: [1:0] PLL1SRC, [5:4] PLL1M-1, [12:11] PLL1RGE, [16] PLL1REN
       DIVR: [8:0] PLL1N-1, [30:24] PLL1R-1 */
    /* Note: PLL1RGE (input freq range) must match VCO input.
       Use ll_rcc_pll_config_full() or set RGE via ll_rcc_pll_set_rge(). */
    {
        uint32_t cfgr = src
                      | ((m - 1) << 4)
                      | (1UL << 16);      /* PLL1REN: enable R output */
        uint32_t divr = ((n - 1) << 0)
                      | ((r - 1) << 24);
        REG32(RCC_BASE + 0x28UL) = cfgr;
        REG32(RCC_BASE + 0x34UL) = divr;
    }

#elif defined(STM32H523xx)
    /* H5: RCC_PLL1CFGR at offset 0x28, RCC_PLL1DIVR at offset 0x34
       CFGR: [1:0] PLL1SRC, [5:2] PLL1M-1, [18] PLL1REN
       DIVR: [8:0] PLL1N-1, [30:24] PLL1R-1 */
    uint32_t cfgr = src
                  | ((m - 1) << 2)
                  | (1UL << 18);  /* PLL1REN */
    uint32_t divr = ((n - 1) << 0)
                  | ((r - 1) << 24);
    REG32(RCC_BASE + 0x28UL) = cfgr;
    REG32(RCC_BASE + 0x34UL) = divr;
#endif
}

/**
 * Set PLL input frequency range (WBA55/H5 only).
 * Call AFTER ll_rcc_pll_config() but BEFORE ll_rcc_pll_enable().
 *   vco_input_mhz: VCO input frequency (source / M) in MHz
 */
static inline void ll_rcc_pll_set_input_range(uint32_t vco_input_mhz)
{
#if defined(STM32WBA55xx)
    /* PLL1CFGR bits [12:11] = PLL1RGE
       00 = 4-8 MHz, 10 = 8-16 MHz */
    uint32_t rge = (vco_input_mhz > 8) ? 0x2UL : 0x0UL;
    MOD_BITS(REG32(RCC_BASE + 0x28UL), 0x3UL << 11, rge << 11);
#elif defined(STM32H523xx)
    uint32_t rge = (vco_input_mhz > 8) ? 0x3UL : (vco_input_mhz > 4) ? 0x2UL : (vco_input_mhz > 2) ? 0x1UL : 0x0UL;
    MOD_BITS(REG32(RCC_BASE + 0x28UL), 0x3UL << 3, rge << 3);
#else
    (void)vco_input_mhz;
#endif
}

/** Enable the main PLL */
static inline void ll_rcc_pll_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 24));  /* CR: PLLON */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 24));  /* CR: PLLON */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 24));  /* CR: PLL1ON */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x00UL), (1UL << 24));  /* CR: PLL1ON */
#endif
}

/** Check if PLL is locked and ready */
static inline int ll_rcc_pll_ready(void)
{
#if defined(STM32L011xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 25)) != 0;  /* CR: PLLRDY */
#elif defined(STM32L422xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 25)) != 0;
#elif defined(STM32WBA55xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 25)) != 0;
#elif defined(STM32H523xx)
    return (REG32(RCC_BASE + 0x00UL) & (1UL << 25)) != 0;
#else
    return 0;
#endif
}

/**
 * Enable PLL and wait for lock with timeout.
 * Returns 1 on success, 0 on timeout (PLL failed to lock).
 */
static inline int ll_rcc_pll_enable_timeout(uint32_t retries)
{
    ll_rcc_pll_enable();
    while (!ll_rcc_pll_ready()) {
        if (--retries == 0) return 0;
    }
    return 1;
}

/**
 * Wait for HSI16 ready with timeout.
 * Returns 1 on success, 0 on timeout.
 */
static inline int ll_rcc_hsi16_enable_timeout(uint32_t retries)
{
#if !defined(STM32H523xx)
    ll_rcc_hsi16_enable();
    while (!ll_rcc_hsi16_ready()) {
        if (--retries == 0) return 0;
    }
    return 1;
#else
    (void)retries;
    return 0;
#endif
}

/* ============================================================
 * SYSCLK source selection
 * ============================================================ */

/* SYSCLK source values (for RCC_CFGR SW bits) */
#if defined(STM32L011xx)
  #define LL_RCC_SYSCLK_MSI    0x0UL
  #define LL_RCC_SYSCLK_HSI16  0x1UL
  #define LL_RCC_SYSCLK_HSE    0x2UL
  #define LL_RCC_SYSCLK_PLL    0x3UL
  #define RCC_CFGR_OFFSET      0x0CUL
  #define RCC_CFGR_SW_MASK     0x3UL
  #define RCC_CFGR_SWS_SHIFT   2
#elif defined(STM32L422xx)
  #define LL_RCC_SYSCLK_MSI    0x0UL
  #define LL_RCC_SYSCLK_HSI16  0x1UL
  #define LL_RCC_SYSCLK_HSE    0x2UL
  #define LL_RCC_SYSCLK_PLL    0x3UL
  #define RCC_CFGR_OFFSET      0x08UL
  #define RCC_CFGR_SW_MASK     0x3UL
  #define RCC_CFGR_SWS_SHIFT   2
#elif defined(STM32WBA55xx)
  #define LL_RCC_SYSCLK_HSI16  0x0UL
  #define LL_RCC_SYSCLK_HSE    0x1UL
  #define LL_RCC_SYSCLK_PLL    0x2UL
  #define RCC_CFGR_OFFSET      0x1CUL  /* RCC_CFGR1 */
  #define RCC_CFGR_SW_MASK     0x3UL
  #define RCC_CFGR_SWS_SHIFT   3
#elif defined(STM32H523xx)
  #define LL_RCC_SYSCLK_HSI48  0x0UL
  #define LL_RCC_SYSCLK_CSI    0x1UL
  #define LL_RCC_SYSCLK_HSE    0x2UL
  #define LL_RCC_SYSCLK_PLL    0x3UL
  #define RCC_CFGR_OFFSET      0x1CUL  /* RCC_CFGR1 */
  #define RCC_CFGR_SW_MASK     0x7UL
  #define RCC_CFGR_SWS_SHIFT   3
#endif

/** Set the SYSCLK source */
static inline void ll_rcc_set_sysclk(uint32_t source)
{
    MOD_BITS(REG32(RCC_BASE + RCC_CFGR_OFFSET), RCC_CFGR_SW_MASK, source);
}

/** Wait until the SYSCLK source has switched */
static inline void ll_rcc_wait_sysclk(uint32_t source)
{
    uint32_t sws_mask = RCC_CFGR_SW_MASK << RCC_CFGR_SWS_SHIFT;
    uint32_t sws_val = source << RCC_CFGR_SWS_SHIFT;
    uint32_t timeout = 100000;
    while ((REG32(RCC_BASE + RCC_CFGR_OFFSET) & sws_mask) != sws_val && --timeout)
        ;
}

/* ============================================================
 * Bus prescalers (AHB, APB1, APB2)
 * ============================================================ */

/**
 * Set AHB prescaler.
 *   div: 1, 2, 4, 8, 16, 64, 128, 256, 512
 */
static inline void ll_rcc_set_ahb_div(uint32_t div)
{
    uint32_t val;
    switch (div) {
        case 1:   val = 0x0; break;
        case 2:   val = 0x8; break;
        case 4:   val = 0x9; break;
        case 8:   val = 0xA; break;
        case 16:  val = 0xB; break;
        case 64:  val = 0xC; break;
        case 128: val = 0xD; break;
        case 256: val = 0xE; break;
        default:  val = 0xF; break;  /* 512 */
    }
#if defined(STM32L011xx)
    MOD_BITS(REG32(RCC_BASE + 0x0CUL), 0xFUL << 4, val << 4);    /* CFGR HPRE */
#elif defined(STM32L422xx)
    MOD_BITS(REG32(RCC_BASE + 0x08UL), 0xFUL << 4, val << 4);    /* CFGR HPRE */
#elif defined(STM32WBA55xx)
    MOD_BITS(REG32(RCC_BASE + 0x1CUL), 0xFUL << 8, val << 8);    /* CFGR1 HPRE */
#elif defined(STM32H523xx)
    MOD_BITS(REG32(RCC_BASE + 0x1CUL), 0xFUL << 8, val << 8);    /* CFGR1 HPRE */
#endif
}

/**
 * Set APB1 prescaler.
 *   div: 1, 2, 4, 8, 16
 */
static inline void ll_rcc_set_apb1_div(uint32_t div)
{
    uint32_t val;
    switch (div) {
        case 1:  val = 0x0; break;
        case 2:  val = 0x4; break;
        case 4:  val = 0x5; break;
        case 8:  val = 0x6; break;
        default: val = 0x7; break;  /* 16 */
    }
#if defined(STM32L011xx)
    MOD_BITS(REG32(RCC_BASE + 0x0CUL), 0x7UL << 8, val << 8);    /* CFGR PPRE1 */
#elif defined(STM32L422xx)
    MOD_BITS(REG32(RCC_BASE + 0x08UL), 0x7UL << 8, val << 8);    /* CFGR PPRE1 */
#elif defined(STM32WBA55xx)
    MOD_BITS(REG32(RCC_BASE + 0x1CUL), 0x7UL << 12, val << 12);  /* CFGR1 PPRE1 */
#elif defined(STM32H523xx)
    MOD_BITS(REG32(RCC_BASE + 0x1CUL), 0x7UL << 12, val << 12);  /* CFGR1 PPRE1 */
#endif
}

/**
 * Set APB2 prescaler.
 *   div: 1, 2, 4, 8, 16
 */
static inline void ll_rcc_set_apb2_div(uint32_t div)
{
#if defined(STM32L011xx)
    /* L011 has only one APB bus (no APB2) — nothing to do */
    (void)div;
#else
    uint32_t val;
    switch (div) {
        case 1:  val = 0x0; break;
        case 2:  val = 0x4; break;
        case 4:  val = 0x5; break;
        case 8:  val = 0x6; break;
        default: val = 0x7; break;  /* 16 */
    }
#if defined(STM32L422xx)
    MOD_BITS(REG32(RCC_BASE + 0x08UL), 0x7UL << 11, val << 11);  /* CFGR PPRE2 */
#elif defined(STM32WBA55xx)
    MOD_BITS(REG32(RCC_BASE + 0x20UL), 0x7UL << 4, val << 4);    /* CFGR2 PPRE2 */
#elif defined(STM32H523xx)
    MOD_BITS(REG32(RCC_BASE + 0x20UL), 0x7UL << 4, val << 4);    /* CFGR2 PPRE2 */
#endif
#endif /* !STM32L011xx */
}

/* ============================================================
 * GPIO clock enable/disable (unchanged from before)
 * ============================================================ */

/** Enable the clock for a GPIO port. */
static inline void ll_rcc_gpio_clk_enable(GPIO_TypeDef *port)
{
    uint32_t index = ((uint32_t)port - (uint32_t)GPIOA) / 0x0400UL;
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x2CUL), (1UL << index));
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x4CUL), (1UL << index));
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << index));
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x8CUL), (1UL << index));
#endif
    (void)REG32(RCC_BASE);
    (void)REG32(RCC_BASE);
}

/** Disable the clock for a GPIO port. */
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

/* ============================================================
 * Peripheral clock enable (by bus)
 * ============================================================ */

static inline void ll_rcc_apb1_clk_enable(uint32_t mask)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x38UL), mask);
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x58UL), mask);
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0x9CUL), mask);
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0x9CUL), mask);
#endif
    (void)REG32(RCC_BASE);
}

static inline void ll_rcc_apb2_clk_enable(uint32_t mask)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x34UL), mask);
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x60UL), mask);
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xA4UL), mask);
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xA4UL), mask);
#endif
    (void)REG32(RCC_BASE);
}

/* ============================================================
 * USB 48MHz clock source selection (L4 only)
 * ============================================================ */

#if defined(STM32L422xx)

/**
 * Select the 48MHz clock source for USB.
 *   0 = HSI48 (default, crystal-less with CRS)
 *   1 = PLL48M1CLK (PLLSAI1 Q output)
 *   2 = PLL48M2CLK (PLL Q output)
 *   3 = MSI
 */
static inline void ll_rcc_set_usb_clk_source(uint32_t src)
{
    /* RCC_CCIPR at offset 0x88, CLK48SEL bits [27:26] */
    MOD_BITS(REG32(RCC_BASE + 0x88UL), 3UL << 26, (src & 3UL) << 26);
}

#define LL_RCC_USB48_HSI48      0x0UL
#define LL_RCC_USB48_PLLSAI1Q   0x1UL
#define LL_RCC_USB48_PLLQ       0x2UL
#define LL_RCC_USB48_MSI        0x3UL

#endif /* STM32L422xx */

/* ============================================================
 * WBA55-specific: AHB4/AHB5, PWR, LSI, radio sleep clock
 * ============================================================ */

#if defined(STM32WBA55xx)

/* AHB4/AHB5 bit masks (needed by functions below) */
#define LL_AHB4_PWR       (1UL << 2)
#define LL_AHB5_RADIO     (1UL << 0)

/* ---- AHB4 peripheral clock (PWR, ADC4) ---- */

static inline void ll_rcc_ahb4_clk_enable(uint32_t mask)
{
    /* RCC_AHB4ENR at offset 0x094 */
    SET_BITS(REG32(RCC_BASE + 0x094UL), mask);
    (void)REG32(RCC_BASE);
    (void)REG32(RCC_BASE);
}

/* ---- AHB5 peripheral clock (RADIO) ---- */

static inline void ll_rcc_ahb5_clk_enable(uint32_t mask)
{
    /* RCC_AHB5ENR at offset 0x098 */
    SET_BITS(REG32(RCC_BASE + 0x098UL), mask);
    (void)REG32(RCC_BASE);
    (void)REG32(RCC_BASE);
}

static inline void ll_rcc_ahb5_clk_sleep_enable(void)
{
    /* RCC_AHB5SMENR at offset 0x0D8 — keep RADIO clock in sleep */
    SET_BITS(REG32(RCC_BASE + 0x0D8UL), LL_AHB5_RADIO);
}

static inline void ll_rcc_ahb5_clk_sleep_disable(void)
{
    CLR_BITS(REG32(RCC_BASE + 0x0D8UL), LL_AHB5_RADIO);
}

/* ---- PWR: backup domain access ---- */

#define PWR_BASE_WBA    (AHB5_BASE + 0x0800UL)  /* 0x46020800 */
#define PWR_DBPR_REG    REG32(PWR_BASE_WBA + 0x28UL)
#define PWR_VOSR_REG    REG32(PWR_BASE_WBA + 0x0CUL)

/** Enable PWR peripheral clock and backup domain write access. */
static inline void ll_pwr_enable_backup_access(void)
{
    ll_rcc_ahb4_clk_enable(LL_AHB4_PWR);
    SET_BITS(PWR_DBPR_REG, (1UL << 0));  /* DBP */
}

/** Get radio power mode (PWR_SR1 bits [2:1]) */
static inline uint32_t ll_pwr_get_radio_mode(void)
{
    /* PWR_SR1 at offset 0x04, RADIOST bits [2:1] */
    return (REG32(PWR_BASE_WBA + 0x04UL) >> 1) & 0x3UL;
}

#define LL_PWR_RADIO_ACTIVE_MODE    0x2UL
#define LL_PWR_RADIO_DEEPSLEEP      0x0UL

/* ---- LSI1 oscillator ---- */

/** Enable LSI1 (32kHz internal RC). Requires backup domain access. */
static inline void ll_rcc_lsi1_enable(void)
{
    /* RCC_BDCR1 at offset 0x0F0, LSI1ON = bit 26 */
    SET_BITS(REG32(RCC_BASE + 0x0F0UL), (1UL << 26));
}

/** Check if LSI1 is ready. */
static inline int ll_rcc_lsi1_ready(void)
{
    /* RCC_BDCR1, LSI1RDY = bit 27 */
    return (REG32(RCC_BASE + 0x0F0UL) & (1UL << 27)) != 0;
}

/** Wait for LSI1 ready with timeout. Returns 1 on success, 0 on timeout. */
static inline int ll_rcc_lsi1_enable_wait(void)
{
    ll_rcc_lsi1_enable();
    for (volatile uint32_t t = 0; t < 1000000; t++) {
        if (ll_rcc_lsi1_ready()) return 1;
    }
    return 0;
}

/* ---- Radio sleep timer clock source ---- */

#define LL_RCC_RADIOSLEEPSOURCE_NONE     0x0UL
#define LL_RCC_RADIOSLEEPSOURCE_LSE      0x1UL
#define LL_RCC_RADIOSLEEPSOURCE_LSI      0x2UL
#define LL_RCC_RADIOSLEEPSOURCE_HSE_DIV  0x3UL  /* HSE / 1024 */

/** Set the radio sleep timer clock source. Requires backup domain access. */
static inline void ll_rcc_set_radio_sleep_clk(uint32_t src)
{
    /* RCC_BDCR1, RADIOSTSEL bits [19:18] */
    MOD_BITS(REG32(RCC_BASE + 0x0F0UL), 0x3UL << 18, (src & 0x3UL) << 18);
}

/** Get the current radio sleep timer clock source. */
static inline uint32_t ll_rcc_get_radio_sleep_clk(void)
{
    return (REG32(RCC_BASE + 0x0F0UL) >> 18) & 0x3UL;
}

/* ---- Radio baseband clock (active clock for 2.4GHz radio) ---- */

static inline void ll_rcc_radio_bb_clk_enable(void)
{
    /* RCC_RADIOENR at offset 0x0A8, bit 1 = RADIOSTCKEN (sleep timer),
       bit 0 = RADIOENEN (radio enable) — wait for RFSSMEN bit? */
    SET_BITS(REG32(RCC_BASE + 0x208UL), (1UL << 0));  /* RADIOENEN */
}

static inline void ll_rcc_radio_bb_clk_disable(void)
{
    CLR_BITS(REG32(RCC_BASE + 0x208UL), (1UL << 0));
}

static inline void ll_rcc_radio_slp_tmr_clk_enable(void)
{
    SET_BITS(REG32(RCC_BASE + 0x208UL), (1UL << 1));  /* RADIOSTCKEN */
}

static inline int ll_rcc_radio_slp_tmr_clk_enabled(void)
{
    return (REG32(RCC_BASE + 0x208UL) & (1UL << 1)) != 0;
}

#endif /* STM32WBA55xx */

/* ============================================================
 * Peripheral clock enable bit masks (per family)
 * ============================================================ */

#if defined(STM32L011xx)
  /* APB1 */
  #define LL_APB1_TIM2      (1UL << 0)
  #define LL_APB1_USART2    (1UL << 17)
  #define LL_APB1_I2C1      (1UL << 21)
  #define LL_APB1_LPTIM1    (1UL << 31)
  /* APB2 */
  #define LL_APB2_TIM21     (1UL << 2)
  #define LL_APB2_SPI1      (1UL << 12)

#elif defined(STM32L422xx)
  /* APB1 */
  #define LL_APB1_TIM2      (1UL << 0)
  #define LL_APB1_USART2    (1UL << 17)
  #define LL_APB1_I2C1      (1UL << 21)
  #define LL_APB1_I2C3      (1UL << 23)
  #define LL_APB1_USB       (1UL << 26)
  /* APB2 */
  #define LL_APB2_TIM1      (1UL << 11)
  #define LL_APB2_SPI1      (1UL << 12)
  #define LL_APB2_USART1    (1UL << 14)
  #define LL_APB2_TIM15     (1UL << 16)
  #define LL_APB2_TIM16     (1UL << 17)
  /* AHB2 */
  #define LL_AHB2_ADC       (1UL << 13)

#elif defined(STM32WBA55xx)
  /* APB1 */
  #define LL_APB1_TIM2      (1UL << 0)
  #define LL_APB1_TIM3      (1UL << 1)
  #define LL_APB1_USART2    (1UL << 17)
  #define LL_APB1_I2C1      (1UL << 21)
  /* APB2 */
  #define LL_APB2_TIM1      (1UL << 11)
  #define LL_APB2_SPI1      (1UL << 12)
  #define LL_APB2_USART1    (1UL << 14)
  #define LL_APB2_TIM16     (1UL << 17)
  #define LL_APB2_TIM17     (1UL << 18)
  /* APB7 (WBA-specific) */
  #define LL_APB7_I2C3      (1UL << 2)
  #define LL_APB7_LPTIM1    (1UL << 0)
  #define LL_APB7_SPI3      (1UL << 3)
  #define LL_APB7_LPUART1   (1UL << 6)
  /* AHB2 */
  #define LL_AHB2_ADC4      (1UL << 10)
  /* AHB4/AHB5 defined above with WBA55 functions */

#elif defined(STM32H523xx)
  /* APB1 */
  #define LL_APB1_TIM2      (1UL << 0)
  #define LL_APB1_TIM3      (1UL << 1)
  #define LL_APB1_TIM6      (1UL << 4)
  #define LL_APB1_TIM7      (1UL << 5)
  #define LL_APB1_USART2    (1UL << 17)
  #define LL_APB1_USART3    (1UL << 18)
  #define LL_APB1_I2C1      (1UL << 21)
  #define LL_APB1_I2C2      (1UL << 22)
  /* APB2 */
  #define LL_APB2_TIM1      (1UL << 11)
  #define LL_APB2_SPI1      (1UL << 12)
  #define LL_APB2_USART1    (1UL << 14)
  /* APB3 */
  #define LL_APB3_LPUART1   (1UL << 6)
  #define LL_APB3_I2C3      (1UL << 23)
  #define LL_APB3_SPI3      (1UL << 5)
  /* AHB2 */
  #define LL_AHB2_ADC       (1UL << 10)
#endif

#endif /* LL_RCC_H */
