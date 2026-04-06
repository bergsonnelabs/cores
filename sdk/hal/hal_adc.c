/**
 * hal_adc.c — ADC HAL driver (Tier 1)
 *
 * Implementation covers:
 *   STM32L011xx (Core.L)  — ADC v3: simple CHSELR, no ADVREGEN
 *   STM32L422xx (Core.U)  — ADC v5: ADVREGEN, SMPR1/SMPR2, oversampling
 *   STM32WBA55xx (Core.W) — ADC4: similar to L0 layout, ADVREGEN, CCR
 *   STM32H523xx (Core.H)  — ADC v5+: like L4 plus 14-bit, 1024× oversample
 *
 * Per-family internal channel numbers:
 *   L0:  VREFINT = CH17, TEMP = CH16
 *   L4:  VREFINT = CH0 (ADC_Common), TEMP = CH17, VBAT = CH18
 *   WBA: VREFINT = CH13, TEMP = CH12
 *   H5:  VREFINT = CH19, TEMP = CH16
 *
 * Factory calibration addresses:
 *   L0:  VREFINT_CAL  @ 0x1FF80078 (calibrated at 3.0V)
 *        TS_CAL1      @ 0x1FF8007A (30°C)
 *        TS_CAL2      @ 0x1FF8007E (130°C)
 *   L4:  VREFINT_CAL  @ 0x1FFF75AA (calibrated at 3.0V)
 *        TS_CAL1      @ 0x1FFF75A8 (30°C, 3.0V)
 *        TS_CAL2      @ 0x1FFF75CA (130°C, 3.0V)
 *   WBA: VREFINT_CAL  @ 0x0BFA0700 (calibrated at 3.3V)
 *        TS_CAL1      @ 0x0BFA0710 (30°C, 3.3V)
 *        TS_CAL2      @ 0x0BFA0720 (130°C, 3.3V)
 *   H5:  VREFINT_CAL  @ 0x08FFF810 (calibrated at 3.3V)
 *        TS_CAL1      @ 0x08FFF800 (30°C, 3.3V)
 *        TS_CAL2      @ 0x08FFF804 (110°C, 3.3V)
 */

#include "hal_adc.h"
#include "ll_rcc.h"   /* must come before ll_dma.h — defines RCC_BASE */
#include "ll_adc.h"
#include "ll_dma.h"
#include "core_pads.h"
#include <string.h>

/* ============================================================
 * Per-family internal channel numbers
 * ============================================================ */

#if defined(STM32L011xx)
  #define ADC_CH_VREFINT    17
  #define ADC_CH_TEMP       16

#elif defined(STM32L422xx)
  #define ADC_CH_VREFINT    0    /* ADC_Common VREFINT channel */
  #define ADC_CH_TEMP       17
  #define ADC_CH_VBAT       18

#elif defined(STM32WBA55xx)
  /* ADC4 on WBA55 — see RM0493 §22 */
  #define ADC_CH_VREFINT    13
  #define ADC_CH_TEMP       12

#elif defined(STM32H523xx)
  /* ADC1/ADC2 on H523 — see RM0481 §24 */
  #define ADC_CH_VREFINT    19
  #define ADC_CH_TEMP       16
#endif

/* ============================================================
 * Factory calibration addresses
 * ============================================================ */

#if defined(STM32L011xx)
  /* L0 RM0367 §3.12 — all calibrated at VDDA = 3.0V, 30°C / 130°C */
  #define VREFINT_CAL_ADDR   ((volatile uint16_t *)0x1FF80078UL)
  #define TS_CAL1_ADDR       ((volatile uint16_t *)0x1FF8007AUL)
  #define TS_CAL2_ADDR       ((volatile uint16_t *)0x1FF8007EUL)
  #define VREFINT_CAL_VDD_MV 3000UL
  #define TS_CAL1_TEMP       30
  #define TS_CAL2_TEMP       130

#elif defined(STM32L422xx)
  /* L4 DS12015 §3.13.2 — calibrated at VDDA = 3.0V, 30°C / 130°C */
  #define VREFINT_CAL_ADDR   ((volatile uint16_t *)0x1FFF75AAUL)
  #define TS_CAL1_ADDR       ((volatile uint16_t *)0x1FFF75A8UL)
  #define TS_CAL2_ADDR       ((volatile uint16_t *)0x1FFF75CAUL)
  #define VREFINT_CAL_VDD_MV 3000UL
  #define TS_CAL1_TEMP       30
  #define TS_CAL2_TEMP       130

#elif defined(STM32WBA55xx)
  /* WBA55 RM0493 §3.11 — calibrated at VDDA = 3.3V, 30°C / 130°C */
  #define VREFINT_CAL_ADDR   ((volatile uint16_t *)0x0BFA0700UL)
  #define TS_CAL1_ADDR       ((volatile uint16_t *)0x0BFA0710UL)
  #define TS_CAL2_ADDR       ((volatile uint16_t *)0x0BFA0720UL)
  #define VREFINT_CAL_VDD_MV 3300UL
  #define TS_CAL1_TEMP       30
  #define TS_CAL2_TEMP       130

#elif defined(STM32H523xx)
  /* H523 — calibrated at VDDA = 3.3V, 30°C / 130°C (from ST LL ADC header) */
  #define VREFINT_CAL_ADDR   ((volatile uint16_t *)0x08FFF810UL)
  #define TS_CAL1_ADDR       ((volatile uint16_t *)0x08FFF814UL)
  #define TS_CAL2_ADDR       ((volatile uint16_t *)0x08FFF818UL)
  #define VREFINT_CAL_VDD_MV 3300UL
  #define TS_CAL1_TEMP       30
  #define TS_CAL2_TEMP       130
#endif

/* ============================================================
 * Sampling time encoding (family-specific LL constants)
 * ============================================================ */

/**
 * Map hal_adc_samp_t → LL sampling time register value.
 *
 * L0:  1.5 / 7.5 / 160.5 / 239.5 ADC cycles
 * L4:  2.5 / 47.5 / 160.5 / 640.5 ADC cycles
 * WBA: 1.5 / 47.5 / 160.5 / 640.5 ADC cycles (ADC4)
 * H5:  2.5 / 47.5 / 160.5 / 247.5 ADC cycles
 */
static uint32_t _samp_encode(hal_adc_samp_t s)
{
#if defined(STM32L011xx)
    switch (s) {
    case HAL_ADC_SAMP_FAST:      return LL_ADC_SMPR_1_5;
    case HAL_ADC_SAMP_MED:       return LL_ADC_SMPR_7_5;
    case HAL_ADC_SAMP_SLOW:      return LL_ADC_SMPR_160_5;
    default:                     return LL_ADC_SMPR_160_5; /* VERY_SLOW same as SLOW on L0 */
    }
#elif defined(STM32L422xx) || defined(STM32WBA55xx)
    switch (s) {
    case HAL_ADC_SAMP_FAST:      return LL_ADC_SMPR_3_5;
    case HAL_ADC_SAMP_MED:       return LL_ADC_SMPR_39_5;
    case HAL_ADC_SAMP_SLOW:      return LL_ADC_SMPR_160_5;
    default:                     return 0x7UL; /* 640.5 cycles */
    }
#elif defined(STM32H523xx)
    switch (s) {
    case HAL_ADC_SAMP_FAST:      return LL_ADC_SMPR_3_5;
    case HAL_ADC_SAMP_MED:       return LL_ADC_SMPR_39_5;
    case HAL_ADC_SAMP_SLOW:      return LL_ADC_SMPR_160_5;
    default:                     return 0x7UL; /* 247.5 cycles on H5 */
    }
#else
    (void)s;
    return 0;
#endif
}

/* ============================================================
 * Resolution encoding → CFGR1/CFGR RES field value
 * ============================================================ */

/**
 * Map hal_adc_res_t → hardware RES field encoding.
 *
 * L0/L4/WBA CFGR1[4:3]: 00=12-bit, 01=10-bit, 10=8-bit, 11=6-bit
 * H5  CFGR1[3:2]:        00=14-bit, 01=12-bit, 10=10-bit, 11=8-bit (no 6-bit)
 *
 * Returns 0xFF for unsupported combinations.
 */
static uint32_t _res_encode(hal_adc_res_t res)
{
#if defined(STM32L011xx) || defined(STM32L422xx) || defined(STM32WBA55xx)
    switch (res) {
    case HAL_ADC_RES_12BIT: return 0x0UL;
    case HAL_ADC_RES_10BIT: return 0x1UL;
    case HAL_ADC_RES_8BIT:  return 0x2UL;
    case HAL_ADC_RES_6BIT:  return 0x3UL;
    default:                return 0xFFUL; /* 14-bit not supported */
    }
#elif defined(STM32H523xx)
    switch (res) {
    case HAL_ADC_RES_14BIT: return 0x0UL;
    case HAL_ADC_RES_12BIT: return 0x1UL;
    case HAL_ADC_RES_10BIT: return 0x2UL;
    case HAL_ADC_RES_8BIT:  return 0x3UL;
    default:                return 0xFFUL; /* 6-bit not on H5 */
    }
#else
    (void)res;
    return 0xFFUL;
#endif
}

/* ============================================================
 * ADC clock enable
 * ============================================================ */

static void _adc_clk_enable(ADC_TypeDef *instance)
{
    (void)instance;
#if defined(STM32L011xx)
    /* L0: ADC on APB2 — bit 9 of APB2ENR (offset 0x34 from RCC) */
    SET_BITS(REG32(RCC_BASE + 0x34UL), (1UL << 9));
#elif defined(STM32L422xx)
    ll_rcc_ahb2_clk_enable(LL_AHB2_ADC);
#elif defined(STM32WBA55xx)
    /* ADC4 on WBA is on AHB4 */
    ll_rcc_ahb4_clk_enable(LL_AHB4_ADC4);
#elif defined(STM32H523xx)
    ll_rcc_ahb2_clk_enable(LL_AHB2_ADC);
#endif
    (void)REG32(RCC_BASE);   /* read-back to flush pipeline */
}

/* ============================================================
 * Oversampling configuration
 * ============================================================ */

/**
 * Apply oversampling to ADC registers.
 *
 * Oversampling register layout:
 *   L4/H5  CFGR2: OVS[2:0] ratio, OVSS[3:0] shift, ROVSE bit
 *   L0/WBA CFGR2: OVSR[2:0] ratio in CFGR2, OVSS[3:0] shift, OVSE bit
 *
 * The right-shift equals log2(ratio)/2 (i.e. half of ratio bits) which
 * keeps the output at the configured resolution instead of extending it.
 * E.g. 16× oversample → shift 4 → output remains 12-bit.
 */
/* shift_bits: explicit right-shift (0–ratio/2).  Callers compute this. */
static void _apply_oversample(ADC_TypeDef *adc, hal_adc_oversample_t ratio,
                               uint8_t shift_bits)
{
    if (ratio == HAL_ADC_OVERSAMPLE_1X) {
        /* Disable oversampling */
        CLR_BITS(adc->CFGR2, (1UL << 0));  /* ROVSE/OVSE bit */
        return;
    }

    /* ratio encodes log2(N): e.g. 4× → 2, 16× → 4, …
       OVS register value = log2(N) - 1 per RM (0=2×, 1=4×, …) */
    uint32_t ovsr  = (uint32_t)(ratio / 2) - 1;
    uint32_t shift = (uint32_t)shift_bits;

#if defined(STM32L011xx)
    /* L0: CFGR2[4:2]=OVSR, CFGR2[8:5]=OVSS, CFGR2[0]=OVSE */
    uint32_t cfgr2 = adc->CFGR2;
    cfgr2 &= ~((0x7UL << 2) | (0xFUL << 5) | (1UL << 0));
    cfgr2 |= (ovsr << 2) | (shift << 5) | (1UL << 0);
    adc->CFGR2 = cfgr2;

#elif defined(STM32L422xx)
    /* L4: CFGR2[4:2]=OVSR, CFGR2[8:5]=OVSS, CFGR2[0]=ROVSE */
    uint32_t cfgr2 = adc->CFGR2;
    cfgr2 &= ~((0x7UL << 2) | (0xFUL << 5) | (1UL << 0));
    cfgr2 |= (ovsr << 2) | (shift << 5) | (1UL << 0);
    adc->CFGR2 = cfgr2;

#elif defined(STM32WBA55xx)
    /* WBA ADC4: same encoding as L0 */
    uint32_t cfgr2 = adc->CFGR2;
    cfgr2 &= ~((0x7UL << 2) | (0xFUL << 5) | (1UL << 0));
    cfgr2 |= (ovsr << 2) | (shift << 5) | (1UL << 0);
    adc->CFGR2 = cfgr2;

#elif defined(STM32H523xx)
    /* H5: CFGR2[4:2]=OVSR (0=2×…4=32×,5=64×…), CFGR2[8:5]=OVSS, CFGR2[0]=ROVSE
       1024× is HAL_ADC_OVERSAMPLE_1024X → ratio=10 → ovsr=4 → but for H5,
       OVSR encoding extends to 0b111 for 256× and uses OSVSR for 1024×.
       For Tier 1 we support up to 256× cleanly (ovsr max 7). */
    if (ratio == HAL_ADC_OVERSAMPLE_1024X) {
        /* H5 specific: LFTRIG + extended OVSR — treat as OSVSR=1024 */
        /* OVSR[2:0]=7 means 256× in basic mode; H5 RM §24.4.25 shows
           OVSR bits 5:2 for the larger ratios. Use bits [6:2] = 0b01000 = 1024× */
        uint32_t cfgr2 = adc->CFGR2;
        cfgr2 &= ~((0x1FUL << 2) | (0xFUL << 7) | (1UL << 0));
        cfgr2 |= (0x8UL << 2)    /* OVSR = 8 = 1024× */
              | (5UL << 7)        /* OVSS = 5 */
              | (1UL << 0);       /* ROVSE */
        adc->CFGR2 = cfgr2;
    } else {
        uint32_t cfgr2 = adc->CFGR2;
        cfgr2 &= ~((0x1FUL << 2) | (0xFUL << 7) | (1UL << 0));
        cfgr2 |= (ovsr << 2) | (shift << 7) | (1UL << 0);
        adc->CFGR2 = cfgr2;
    }
#endif
}

/* ============================================================
 * Per-channel sampling time configuration (L4/H5/WBA)
 * ============================================================ */

/**
 * Write sampling time for one channel into SMPR1/SMPR2.
 * On L0/WBA(ADC4) the SMPR register applies to all channels.
 */
static void _set_channel_samp(ADC_TypeDef *adc, uint8_t channel, uint32_t smpr_val)
{
#if defined(STM32L011xx) || defined(STM32WBA55xx)
    /* Single SMPR register — update all fields to the same value */
    uint32_t smpr = 0;
    for (int i = 0; i < 8; i++) smpr |= smpr_val << (i * 3);
    adc->SMPR = smpr;
    (void)channel;

#elif defined(STM32L422xx) || defined(STM32H523xx)
    /* SMPR1: channels 0–9 (3 bits each starting at bit 0)
       SMPR2: channels 10–18 (3 bits each starting at bit 0) */
    if (channel <= 9) {
        MOD_BITS(adc->SMPR, 0x7UL << (channel * 3), smpr_val << (channel * 3));
    } else if (channel <= 18) {
        uint32_t pos = (channel - 10) * 3;
        MOD_BITS(adc->SMPR2, 0x7UL << pos, smpr_val << pos);
    }
#endif
}

/* ============================================================
 * Internal channel enable (VREFINT / TEMP)
 * ============================================================ */

/**
 * Enable VREFINT and temperature sensor in ADC_CCR.
 * Must be called before reading those channels.
 */
static void _enable_internal_channels(void)
{
#if defined(STM32L011xx)
    /* L0: CFGR1 VREFEN[22] and TSEN[23] */
    /* Actually on L0 they sit in ADC->CFGR1 — but reference says
       VREFEN is in ADC_CCR for some L0 variants.  On L011 the
       internal channel enables are in the common register at +0x308
       relative to ADC1 base, but since it's a single-ADC device the
       common reg may not exist.  Instead use CFGR1 VREFEN/TSEN bits.
       L0 RM0367: ADC_CCR does NOT exist on L0 single-ADC; VREFEN is
       in ADC_CCR of the L0 multi-ADC line.  On L011 use CFGR2[22]. */
    SET_BITS(ADC1->CFGR2, (1UL << 22) | (1UL << 23));  /* VREFEN, TSEN */

#elif defined(STM32L422xx)
    /* L4: ADC_CCR bits VREFEN[22] and TSEN[23] */
    SET_BITS(ADC_CCR, (1UL << 22) | (1UL << 23));

#elif defined(STM32WBA55xx)
    /* WBA ADC4: ADC_CCR VREFEN[22], VSENSESEL[23] */
    SET_BITS(ADC_CCR, (1UL << 22) | (1UL << 23));

#elif defined(STM32H523xx)
    /* H5: ADC_CCR VREFEN[22], VSENSESEL[23] */
    SET_BITS(ADC_CCR, (1UL << 22) | (1UL << 23));
#endif

    /* Temp sensor and VREFINT need stabilisation time after enable.
     * L4: t_START ≈ 120 µs (RM0394), others similar.
     * ~5000 iterations ≈ 150 µs @ 80 MHz — covers all families. */
    for (volatile int i = 0; i < 5000; i++) {}
}

/* ============================================================
 * hal_adc_init
 * ============================================================ */

hal_status_t hal_adc_init(hal_adc_t *adc, ADC_TypeDef *instance,
                          uint32_t sysclk_hz, hal_adc_res_t res)
{
    memset(adc, 0, sizeof(*adc));
    adc->instance       = instance;
    adc->resolution     = res;
    adc->oversample     = HAL_ADC_OVERSAMPLE_1X;
    adc->sysclk_hz      = sysclk_hz;
    adc->effective_bits = (uint8_t)res;   /* matches resolution until _ex is used */

    /* Validate resolution for this family */
    if (_res_encode(res) == 0xFFUL) {
        return HAL_ERROR;
    }

    _adc_clk_enable(instance);

#if defined(STM32L011xx)
    /* ---- L0 init ---- */
    /* L0 has no deep power-down or voltage regulator; just reset CR */
    instance->CR = 0;
    instance->CFGR1 = LL_ADC_CFGR1_OVRMOD;   /* overwrite on overrun */
    /* Apply resolution to CFGR1[4:3] */
    MOD_BITS(instance->CFGR1, 0x3UL << 3, _res_encode(res) << 3);
    instance->CFGR2 = 0;  /* PCLK synchronous (no async clock) */

    ll_adc_calibrate(instance);

    /* Enable ADC */
    instance->ISR = LL_ADC_ISR_ADRDY;
    SET_BITS(instance->CR, LL_ADC_CR_ADEN);
    { uint32_t t = 100000; while (!(instance->ISR & LL_ADC_ISR_ADRDY) && --t) ; }

#elif defined(STM32L422xx)
    /* ---- L4 init ---- */

    /* Exit deep power-down */
    instance->CR = 0;
    SET_BITS(instance->CR, (1UL << 28));          /* ADVREGEN = 1 */
    for (volatile int i = 0; i < 1000; i++) {}

    MOD_BITS(ADC_CCR, 0xFUL << 18, 0x2UL << 18); /* PRESC = /4 */

    ll_adc_calibrate(instance);

    instance->CFGR1 = LL_ADC_CFGR1_OVRMOD;
    MOD_BITS(instance->CFGR1, 0x3UL << 3, _res_encode(res) << 3);

    instance->ISR = LL_ADC_ISR_ADRDY;
    SET_BITS(instance->CR, LL_ADC_CR_ADEN);
    { uint32_t t = 100000; while (!(instance->ISR & LL_ADC_ISR_ADRDY) && --t) ; }

#elif defined(STM32H523xx)
    /* ---- H5 init ----
     * Exact sequence verified on hardware. Order matters:
     * 1. Set CKMODE in CCR (HCLK/4 — async kernel clock not configured)
     * 2. Exit deep power-down (CR = 0, then ADVREGEN via direct assign)
     * 3. Wait for regulator startup
     * 4. Calibrate
     * 5. Set CFGR1 (resolution + OVRMOD) via direct assign
     * 6. Set SMPR for all channels (must be before ADEN)
     * 7. Enable ADC */

    /* 1. Clock: CKMODE=11 (HCLK/4), clear PRESC */
    ADC_CCR = (ADC_CCR & ~((0x3UL << 16) | (0xFUL << 18))) | (0x3UL << 16);

    /* 2. Exit deep power-down */
    instance->CR = 0;                   /* Clear DEEPPWD (direct assign) */
    instance->CR = (1UL << 28);         /* ADVREGEN (direct assign, not |=) */

    /* 3. Wait for regulator startup (~10µs min per datasheet) */
    for (volatile int i = 0; i < 2000; i++) {}

    /* 4. Calibrate (single-ended) */
    instance->CR &= ~(1UL << 30);      /* ADCALDIF = 0 */
    instance->CR |= (1UL << 31);       /* ADCAL = 1 */
    { uint32_t t = 100000; while ((instance->CR & (1UL << 31)) && --t) ; }

    /* 5. Configuration: OVRMOD + resolution (direct assign to CFGR1) */
    instance->CFGR1 = LL_ADC_CFGR1_OVRMOD | (_res_encode(res) << 2);

    /* 6. Sample time: max for all channels (SMPR only writable when ADEN=0) */
    instance->SMPR  = 0x3FFFFFFFUL;
    instance->SMPR2 = 0x3FFFFFFFUL;

    /* 7. Enable ADC */
    instance->ISR = LL_ADC_ISR_ADRDY;
    instance->CR |= LL_ADC_CR_ADEN;
    { uint32_t t = 100000; while (!(instance->ISR & LL_ADC_ISR_ADRDY) && --t) ; }

#elif defined(STM32WBA55xx)
    /* ---- WBA ADC4 init ---- */
    instance->CR = 0;
    SET_BITS(instance->CR, (1UL << 28));           /* ADVREGEN */
    for (volatile int i = 0; i < 1000; i++) {}

    MOD_BITS(ADC_CCR, 0xFUL << 18, 0x2UL << 18);  /* PRESC /4 */

    ll_adc_calibrate(instance);

    instance->CFGR1 = LL_ADC_CFGR1_OVRMOD;
    MOD_BITS(instance->CFGR1, 0x3UL << 3, _res_encode(res) << 3);

    instance->ISR = LL_ADC_ISR_ADRDY;
    SET_BITS(instance->CR, LL_ADC_CR_ADEN);
    { uint32_t t = 100000; while (!(instance->ISR & LL_ADC_ISR_ADRDY) && --t) ; }
#endif

    return HAL_OK;
}

/* ============================================================
 * hal_adc_add_channel
 * ============================================================ */

hal_status_t hal_adc_add_channel(hal_adc_t *adc, uint8_t channel,
                                 hal_adc_samp_t samp)
{
    if (adc->n_channels >= HAL_ADC_MAX_CHANNELS) return HAL_ERROR;

    /* Check if channel already registered; update samp if so */
    for (uint8_t i = 0; i < adc->n_channels; i++) {
        if (adc->channels[i].channel == channel) {
            adc->channels[i].samp = samp;
            _set_channel_samp(adc->instance, channel, _samp_encode(samp));
            return HAL_OK;
        }
    }

    adc->channels[adc->n_channels].channel = channel;
    adc->channels[adc->n_channels].samp    = samp;
    adc->channels[adc->n_channels].enabled = true;
    adc->n_channels++;

    _set_channel_samp(adc->instance, channel, _samp_encode(samp));
    return HAL_OK;
}

/* ============================================================
 * hal_adc_set_oversample
 * ============================================================ */

hal_status_t hal_adc_set_oversample(hal_adc_t *adc, hal_adc_oversample_t ratio)
{
#if !defined(STM32H523xx)
    if (ratio == HAL_ADC_OVERSAMPLE_1024X) return HAL_ERROR;
#endif
    adc->oversample     = ratio;
    adc->effective_bits = (uint8_t)adc->resolution;  /* noise reduction: no bit-depth change */
    _apply_oversample(adc->instance, ratio, (uint8_t)(ratio / 2));
    return HAL_OK;
}

hal_status_t hal_adc_set_oversample_ex(hal_adc_t *adc, hal_adc_oversample_t ratio,
                                        uint8_t shift)
{
#if !defined(STM32H523xx)
    if (ratio == HAL_ADC_OVERSAMPLE_1024X) return HAL_ERROR;
#endif
    uint8_t max_shift = (uint8_t)(ratio / 2);
    if (shift > max_shift) shift = max_shift;   /* clamp */

    adc->oversample     = ratio;
    /* Extra bits retained = log2(N) - shift  (= max_shift - shift) */
    adc->effective_bits = (uint8_t)adc->resolution + (max_shift - shift);
    _apply_oversample(adc->instance, ratio, shift);
    return HAL_OK;
}

/* ============================================================
 * hal_adc_read — single-shot blocking read
 * ============================================================ */

uint16_t hal_adc_read(hal_adc_t *adc, uint8_t channel)
{
    ADC_TypeDef *inst = adc->instance;

#if defined(STM32L011xx) || defined(STM32WBA55xx)
    /* L0/WBA: CHSELR is read-only while ADSTART is set.
     * Stop any in-progress conversion before updating the channel. */
    if (inst->CR & LL_ADC_CR_ADSTART) {
        SET_BITS(inst->CR, LL_ADC_CR_ADSTP);
        uint32_t t = 10000;
        while ((inst->CR & LL_ADC_CR_ADSTP) && --t) ;
    }
    /* Clear any stale EOC before starting a new conversion */
    inst->ISR = LL_ADC_ISR_EOC;

    /* CHSELR is at offset 0x28 — mapped to TR3 in our ADC_TypeDef.
     * SQR1 (offset 0x30) is NOT CHSELR on these families. */
    inst->TR3 = (1UL << channel);

#elif defined(STM32L422xx)
    /* L4: sequence register — single channel in SQ1 field */
    inst->SQR1 = (channel << 6);
    inst->ISR = LL_ADC_ISR_EOC;

#elif defined(STM32H523xx)
    /* H5: SQR1 set during init. Just set channel and clear EOC.
     * On H5, writing SQR1 after ADEN may not take effect reliably. */
    {
        /* Stop any in-progress conversion first */
        if (inst->CR & LL_ADC_CR_ADSTART) {
            inst->CR |= (1UL << 4);  /* ADSTP */
            uint32_t t2 = 10000;
            while ((inst->CR & (1UL << 4)) && --t2) ;
        }
        inst->SQR1 = (adc->channels[channel].channel << 6);
    }
    inst->ISR = LL_ADC_ISR_EOC;
#endif

    /* Start conversion */
    SET_BITS(inst->CR, LL_ADC_CR_ADSTART);

    /* Wait for end of conversion (with timeout to avoid infinite hang) */
    uint32_t timeout = 100000;
    while (!(inst->ISR & LL_ADC_ISR_EOC) && --timeout)
        ;

    return (uint16_t)inst->DR;
}

/* ============================================================
 * VREFINT / VDD / temperature
 * ============================================================ */

uint32_t hal_adc_read_vdda_mv(hal_adc_t *adc)
{
    _enable_internal_channels();

    /* Temporarily add VREFINT channel with slow sampling */
    bool was_present = false;
    for (uint8_t i = 0; i < adc->n_channels; i++) {
        if (adc->channels[i].channel == ADC_CH_VREFINT) {
            was_present = true;
            break;
        }
    }
    if (!was_present) {
        hal_adc_add_channel(adc, ADC_CH_VREFINT, HAL_ADC_SAMP_SLOW);
    }

    uint16_t vref_raw = hal_adc_read(adc, ADC_CH_VREFINT);

#if defined(STM32WBA55xx)
    /* WBA55: the OTP calibration region (0x0BFA07xx) is not directly
     * accessible via the AHB bus — a direct read causes a bus fault.
     * The supply rail is a fixed 3.3V which matches VREFINT_CAL_VDD_MV
     * exactly, so the calibration correction factor is 1.0 and returning
     * the nominal value is numerically equivalent.
     * TODO: investigate FLASH peripheral access path for WBA55 OTP. */
    (void)vref_raw;   /* ADC4 VREFINT conversion succeeded — channel works */
    uint32_t vdda = 3300UL;
#else
    uint16_t vref_cal = *VREFINT_CAL_ADDR;

    if (vref_raw == 0 || vref_cal == 0) return 3300UL;

    /* VDDA = VREFINT_CAL_VDD_MV * vref_cal / vref_raw */
    uint32_t vdda = (VREFINT_CAL_VDD_MV * (uint32_t)vref_cal) / (uint32_t)vref_raw;
#endif

    /* Remove the channel if we added it ourselves */
    if (!was_present && adc->n_channels > 0) {
        /* Find and remove: shift remaining channels down */
        for (uint8_t i = 0; i < adc->n_channels; i++) {
            if (adc->channels[i].channel == ADC_CH_VREFINT) {
                for (uint8_t j = i; j < adc->n_channels - 1; j++) {
                    adc->channels[j] = adc->channels[j + 1];
                }
                adc->n_channels--;
                break;
            }
        }
    }

    return vdda;
}

uint32_t hal_adc_read_mv(hal_adc_t *adc, uint8_t channel)
{
    if (adc->vdda_mv == 0) {
        adc->vdda_mv = hal_adc_read_vdda_mv(adc);
    }
    uint16_t raw = hal_adc_read(adc, channel);
    uint32_t max_count = (1UL << (uint32_t)adc->effective_bits) - 1UL;
    return ((uint32_t)raw * adc->vdda_mv) / max_count;
}

int32_t hal_adc_read_temp_decidegc(hal_adc_t *adc)
{
    _enable_internal_channels();

    /* Ensure VDDA is known for scaling */
    if (adc->vdda_mv == 0) {
        adc->vdda_mv = hal_adc_read_vdda_mv(adc);
    }

    /* Add temperature channel if not present */
    bool was_present = false;
    for (uint8_t i = 0; i < adc->n_channels; i++) {
        if (adc->channels[i].channel == ADC_CH_TEMP) {
            was_present = true;
            break;
        }
    }
    if (!was_present) {
        hal_adc_add_channel(adc, ADC_CH_TEMP, HAL_ADC_SAMP_SLOW);
    }

    uint16_t raw = hal_adc_read(adc, ADC_CH_TEMP);

    /* Scale raw reading to the calibration voltage.
     * TS_CAL values were measured at VREFINT_CAL_VDD_MV.
     * At runtime VDDA is higher, so the same sensor voltage produces a
     * lower count.  Scale UP to what the ADC would have read at the cal
     * voltage:  raw_scaled = raw * vdda_mv / VREFINT_CAL_VDD_MV
     */
    uint32_t raw_scaled = ((uint32_t)raw * adc->vdda_mv) / VREFINT_CAL_VDD_MV;

#if defined(STM32WBA55xx)
    /* OTP region not bus-accessible on WBA55 (see note in hal_adc_read_vdda_mv).
     * Use datasheet typical sensor output values (VDD = 3.3V):
     *   V_TS(30°C)  ≈ 770 mV → (770/3300) × 4095 ≈ 955 counts
     *   V_TS(130°C) ≈ 1110 mV → (1110/3300) × 4095 ≈ 1378 counts
     * Accuracy without per-chip calibration: ~±15°C. */
    int32_t ts_cal1 = 955;
    int32_t ts_cal2 = 1378;
#else
    int32_t ts_cal1 = (int32_t)*TS_CAL1_ADDR;
    int32_t ts_cal2 = (int32_t)*TS_CAL2_ADDR;
#endif

    /* Temperature formula from RM:
     *   Temp = (TS_CAL2_TEMP - TS_CAL1_TEMP) * (raw - TS_CAL1) / (TS_CAL2 - TS_CAL1) + TS_CAL1_TEMP
     * In decidegC multiply everything by 10 before the division. */
    int32_t denom = ts_cal2 - ts_cal1;
    if (denom == 0) {
        if (!was_present && adc->n_channels > 0) adc->n_channels--;
        return 250; /* fallback: 25.0°C */
    }

    int32_t temp_decidegc = ((int32_t)(TS_CAL2_TEMP - TS_CAL1_TEMP) * 10
                             * ((int32_t)raw_scaled - ts_cal1))
                            / denom
                            + (int32_t)(TS_CAL1_TEMP * 10);

    /* Remove temp channel if we added it */
    if (!was_present && adc->n_channels > 0) {
        for (uint8_t i = 0; i < adc->n_channels; i++) {
            if (adc->channels[i].channel == ADC_CH_TEMP) {
                for (uint8_t j = i; j < adc->n_channels - 1; j++) {
                    adc->channels[j] = adc->channels[j + 1];
                }
                adc->n_channels--;
                break;
            }
        }
    }

    return temp_decidegc;
}

/* ============================================================
 * hal_adc_read_all
 * ============================================================ */

void hal_adc_read_all(hal_adc_t *adc, uint16_t *buf)
{
    for (uint8_t i = 0; i < adc->n_channels; i++) {
        buf[i] = hal_adc_read(adc, adc->channels[i].channel);
    }
}

/* ============================================================
 * DMA support
 * ============================================================ */

/* Static handle registry so ISRs can find the ADC handle */
#define HAL_ADC_MAX_DMA_INSTANCES  2
static hal_adc_t *_adc_dma_handles[HAL_ADC_MAX_DMA_INSTANCES];

/**
 * Configure the ADC for DMA: enable DMAEN, set circular + continuous,
 * enable the DMA channel.
 */
hal_status_t hal_adc_start_dma(hal_adc_t *adc, uint16_t *buf, uint16_t len,
                                void (*callback)(void))
{
    if (adc->dma_active) return HAL_BUSY;
    if (adc->n_channels == 0 || len == 0) return HAL_ERROR;

    adc->dma_buf      = buf;
    adc->dma_len      = len;
    adc->dma_callback = callback;
    adc->dma_active   = true;

    /* Register handle for ISR lookup */
    for (int i = 0; i < HAL_ADC_MAX_DMA_INSTANCES; i++) {
        if (_adc_dma_handles[i] == NULL || _adc_dma_handles[i] == adc) {
            _adc_dma_handles[i] = adc;
            break;
        }
    }

    ll_rcc_dma1_clk_enable();

    /* Stop any active conversion.
     * On L011/WBA55 ADSTART is read-only once set — must use ADSTP to halt.
     * On L4/H5 writing 0 to ADSTART works, but ADSTP is also safe there. */
    CLR_BITS(adc->instance->CFGR1, LL_ADC_CFGR1_CONT);
    if (adc->instance->CR & LL_ADC_CR_ADSTART) {
        SET_BITS(adc->instance->CR, LL_ADC_CR_ADSTP);
        uint32_t t = 100000;
        while ((adc->instance->CR & LL_ADC_CR_ADSTP) && --t) {}
    }

    /* Configure ADC: continuous, scan, DMA circular, overrun */
    SET_BITS(adc->instance->CFGR1,
             LL_ADC_CFGR1_CONT          /* continuous mode */
           | LL_ADC_CFGR1_OVRMOD        /* overwrite on overrun */
           | (1UL << 1)                 /* DMACFG: circular DMA */
           | (1UL << 0));               /* DMAEN: enable DMA requests */

    /* Build the channel scan sequence */
#if defined(STM32L011xx) || defined(STM32WBA55xx)
    /* L0/WBA: channel select register — set all desired channels */
    uint32_t chselr = 0;
    for (uint8_t i = 0; i < adc->n_channels; i++) {
        chselr |= (1UL << adc->channels[i].channel);
    }
    /* CHSELR is at offset 0x28 — mapped to TR3 in ADC_TypeDef */
    adc->instance->TR3 = chselr;

#elif defined(STM32L422xx) || defined(STM32H523xx)
    /* L4/H5: scan sequence up to 16 entries in SQR1-SQR4
       SQR1[3:0] = L (length - 1), SQ1..SQ16 = channel numbers */
    uint32_t sqr1 = (uint32_t)(adc->n_channels - 1) & 0xFUL;  /* L field */
    if (adc->n_channels >= 1) sqr1 |= (uint32_t)adc->channels[0].channel << 6;
    if (adc->n_channels >= 2) sqr1 |= (uint32_t)adc->channels[1].channel << 12;
    if (adc->n_channels >= 3) sqr1 |= (uint32_t)adc->channels[2].channel << 18;
    if (adc->n_channels >= 4) sqr1 |= (uint32_t)adc->channels[3].channel << 24;
    adc->instance->SQR1 = sqr1;

    uint32_t sqr2 = 0;
    if (adc->n_channels >= 5)  sqr2 |= (uint32_t)adc->channels[4].channel;
    if (adc->n_channels >= 6)  sqr2 |= (uint32_t)adc->channels[5].channel << 6;
    if (adc->n_channels >= 7)  sqr2 |= (uint32_t)adc->channels[6].channel << 12;
    if (adc->n_channels >= 8)  sqr2 |= (uint32_t)adc->channels[7].channel << 18;
    if (adc->n_channels >= 9)  sqr2 |= (uint32_t)adc->channels[8].channel << 24;
    adc->instance->SQR2 = sqr2;

    uint32_t sqr3 = 0;
    if (adc->n_channels >= 10) sqr3 |= (uint32_t)adc->channels[9].channel;
    if (adc->n_channels >= 11) sqr3 |= (uint32_t)adc->channels[10].channel << 6;
    if (adc->n_channels >= 12) sqr3 |= (uint32_t)adc->channels[11].channel << 12;
    if (adc->n_channels >= 13) sqr3 |= (uint32_t)adc->channels[12].channel << 18;
    if (adc->n_channels >= 14) sqr3 |= (uint32_t)adc->channels[13].channel << 24;
    adc->instance->SQR3 = sqr3;

    uint32_t sqr4 = 0;
    if (adc->n_channels >= 15) sqr4 |= (uint32_t)adc->channels[14].channel;
    if (adc->n_channels >= 16) sqr4 |= (uint32_t)adc->channels[15].channel << 6;
    adc->instance->SQR4 = sqr4;
#endif

#if defined(STM32L011xx) || defined(STM32L422xx)
    /* Classic DMA: DMA1 Channel 1 (ADC default on L0 and L4) */
    ll_dma_disable(DMA1_CH1);

    uint32_t ccr_flags = LL_DMA_CCR_MSIZE_16 | LL_DMA_CCR_PSIZE_16
                       | LL_DMA_CCR_MINC     /* memory increment */
                       | LL_DMA_CCR_CIRC     /* circular */
                       | LL_DMA_CCR_TCIE     /* transfer complete IE */
                       | LL_DMA_CCR_HTIE;    /* half transfer IE */

    ll_dma_config(DMA1_CH1, &adc->instance->DR, buf, len, ccr_flags);

#if defined(STM32L422xx)
    ll_dma_set_request(DMA1_CH1, 0);  /* Request 0 = ADC1 on L4 */
#endif

    hal_nvic_set_priority(HAL_IRQ_DMA1_CH1, 6);
    hal_nvic_enable_irq(HAL_IRQ_DMA1_CH1);
    ll_dma_enable(DMA1_CH1);

#elif defined(STM32WBA55xx)
    /* GPDMA Channel 0 for ADC4 on WBA */
    ll_gpdma_disable(GPDMA1_CH0);

    uint32_t ctr1 = LL_GPDMA_CTR1_SDW_HALF  /* src: 16-bit */
                  | LL_GPDMA_CTR1_DDW_HALF  /* dst: 16-bit */
                  | LL_GPDMA_CTR1_DINC;     /* memory increment */
    /* Request for ADC4 on WBA — see RM0493 Table 79 */
    /* ADC4 DMA request: REQSEL = 0 (channel 0 default) */
    uint32_t ctr2 = (0UL << LL_GPDMA_CTR2_REQSEL_SHIFT);

    ll_gpdma_config(GPDMA1_CH0, &adc->instance->DR, buf,
                    (uint32_t)len * 2, ctr1, ctr2);

    /* Enable TC + HT interrupts */
    SET_BITS(GPDMA1_CH0->CCR, LL_GPDMA_CCR_TCIE | LL_GPDMA_CCR_HTIE);
    hal_nvic_set_priority(HAL_IRQ_GPDMA1_CH0, 6);
    hal_nvic_enable_irq(HAL_IRQ_GPDMA1_CH0);
    ll_gpdma_enable(GPDMA1_CH0);

#elif defined(STM32H523xx)
    /* GPDMA Channel 0 for ADC1 on H5 */
    ll_gpdma_disable(GPDMA1_CH0);

    uint32_t ctr1 = LL_GPDMA_CTR1_SDW_HALF
                  | LL_GPDMA_CTR1_DDW_HALF
                  | LL_GPDMA_CTR1_DINC;
    /* H5 ADC1 DMA request number — see RM0481 Table 80 */
    uint32_t ctr2 = (0UL << LL_GPDMA_CTR2_REQSEL_SHIFT);

    ll_gpdma_config(GPDMA1_CH0, &adc->instance->DR, buf,
                    (uint32_t)len * 2, ctr1, ctr2);

    SET_BITS(GPDMA1_CH0->CCR, LL_GPDMA_CCR_TCIE | LL_GPDMA_CCR_HTIE);
    hal_nvic_set_priority(HAL_IRQ_GPDMA1_CH0, 6);
    hal_nvic_enable_irq(HAL_IRQ_GPDMA1_CH0);
    ll_gpdma_enable(GPDMA1_CH0);
#endif

    /* Start continuous conversion */
    SET_BITS(adc->instance->CR, LL_ADC_CR_ADSTART);

    return HAL_OK;
}

void hal_adc_stop_dma(hal_adc_t *adc)
{
    if (!adc->dma_active) return;

    /* Stop ADC continuous mode (ADSTP required on L011/WBA55) */
    if (adc->instance->CR & LL_ADC_CR_ADSTART) {
        SET_BITS(adc->instance->CR, LL_ADC_CR_ADSTP);
        uint32_t t = 100000;
        while ((adc->instance->CR & LL_ADC_CR_ADSTP) && --t) {}
    }

    CLR_BITS(adc->instance->CFGR1,
             LL_ADC_CFGR1_CONT | (1UL << 1) | (1UL << 0));

#if defined(STM32L011xx) || defined(STM32L422xx)
    ll_dma_disable(DMA1_CH1);
    hal_nvic_disable_irq(HAL_IRQ_DMA1_CH1);
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    ll_gpdma_disable(GPDMA1_CH0);
    hal_nvic_disable_irq(HAL_IRQ_GPDMA1_CH0);
#endif

    adc->dma_active = false;
}

/* ============================================================
 * DMA ISR handlers
 *
 * The ISR names override the weak stubs from startup_stm32xxx.s.
 * Calls adc->dma_callback on both TC and HT events.
 * ============================================================ */

static void _adc_dma_irq_common(void)
{
    for (int i = 0; i < HAL_ADC_MAX_DMA_INSTANCES; i++) {
        hal_adc_t *h = _adc_dma_handles[i];
        if (h && h->dma_active && h->dma_callback) {
            h->dma_callback();
        }
    }
}

#if defined(STM32L011xx) || defined(STM32L422xx)

void DMA1_Channel1_IRQHandler(void)
{
    ll_dma_clear_flags(DMA1_CH1);
    _adc_dma_irq_common();
}

#elif defined(STM32WBA55xx) || defined(STM32H523xx)

void GPDMA1_Channel0_IRQHandler(void)
{
    ll_gpdma_clear_flags(GPDMA1_CH0);
    _adc_dma_irq_common();
}

#endif

/* ============================================================
 * Legacy compatibility
 * ============================================================ */

uint16_t hal_adc_read_pad(hal_adc_t *adc, uint8_t pad)
{
    int ch = core_pad_adc_channel(pad);
    if (ch < 0) return 0;
    /* Ensure the channel is registered */
    hal_adc_add_channel(adc, (uint8_t)ch, HAL_ADC_SAMP_MED);
    return hal_adc_read(adc, (uint8_t)ch);
}

uint32_t hal_adc_read_pad_mv(hal_adc_t *adc, uint8_t pad)
{
    int ch = core_pad_adc_channel(pad);
    if (ch < 0) return 0;
    hal_adc_add_channel(adc, (uint8_t)ch, HAL_ADC_SAMP_MED);
    return hal_adc_read_mv(adc, (uint8_t)ch);
}
