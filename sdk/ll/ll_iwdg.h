/**
 * ll_iwdg.h — Low-level Independent Watchdog
 *
 * The IWDG is clocked from LSI (~32kHz) and runs independently
 * of the main clock. Once started, it cannot be stopped — only
 * the MCU reset will disable it. The register layout is identical
 * across all STM32 families.
 *
 * If the watchdog is not refreshed before the timeout, it resets
 * the MCU. Use this for fault recovery in deployed systems.
 *
 * Timeout calculation:
 *   timeout_ms = (reload + 1) * prescaler / 32000 * 1000
 *
 * Prescaler values: 4, 8, 16, 32, 64, 128, 256
 * Reload range: 0-4095 (12-bit)
 *
 * Common configurations:
 *   1s:   prescaler=32, reload=999   → 32ms × 1000 = 1000ms
 *   2s:   prescaler=32, reload=1999
 *   5s:   prescaler=64, reload=2499
 *   10s:  prescaler=256, reload=1249
 *   28s:  prescaler=256, reload=4095  (maximum)
 */

#ifndef LL_IWDG_H
#define LL_IWDG_H

#include "ll_common.h"

/* ---- IWDG base address (same across all families) ---- */

#define IWDG_BASE           0x40003000UL

/* ---- IWDG registers ---- */

#define IWDG_KR             REG32(IWDG_BASE + 0x00UL)  /* Key register */
#define IWDG_PR             REG32(IWDG_BASE + 0x04UL)  /* Prescaler register */
#define IWDG_RLR            REG32(IWDG_BASE + 0x08UL)  /* Reload register */
#define IWDG_SR             REG32(IWDG_BASE + 0x0CUL)  /* Status register */

/* ---- Key values ---- */

#define LL_IWDG_KEY_UNLOCK  0x5555UL   /* Unlock PR and RLR for writing */
#define LL_IWDG_KEY_RELOAD  0xAAAAUL   /* Refresh the watchdog (feed the dog) */
#define LL_IWDG_KEY_START   0xCCCCUL   /* Start the watchdog (irreversible!) */

/* ---- Prescaler values ---- */

#define LL_IWDG_PSC_4       0x0UL      /* LSI / 4   = ~8kHz    → 0.125ms/tick */
#define LL_IWDG_PSC_8       0x1UL      /* LSI / 8   = ~4kHz    → 0.25ms/tick  */
#define LL_IWDG_PSC_16      0x2UL      /* LSI / 16  = ~2kHz    → 0.5ms/tick   */
#define LL_IWDG_PSC_32      0x3UL      /* LSI / 32  = ~1kHz    → 1ms/tick     */
#define LL_IWDG_PSC_64      0x4UL      /* LSI / 64  = ~500Hz   → 2ms/tick     */
#define LL_IWDG_PSC_128     0x5UL      /* LSI / 128 = ~250Hz   → 4ms/tick     */
#define LL_IWDG_PSC_256     0x6UL      /* LSI / 256 = ~125Hz   → 8ms/tick     */

/* ---- SR bit definitions ---- */

#define LL_IWDG_SR_PVU      (1UL << 0)    /* Prescaler value update */
#define LL_IWDG_SR_RVU      (1UL << 1)    /* Reload value update */

/* ============================================================
 * Configuration and control
 * ============================================================ */

/**
 * Initialize and start the independent watchdog.
 *   prescaler: LL_IWDG_PSC_* value
 *   reload:    reload value (0-4095)
 *
 * WARNING: Once started, the IWDG cannot be stopped. The only
 * way to disable it is a full MCU reset. Make sure your main
 * loop calls ll_iwdg_refresh() regularly.
 *
 * The LSI oscillator is automatically started when IWDG is enabled.
 */
static inline void ll_iwdg_init(uint32_t prescaler, uint32_t reload)
{
    /* Start the watchdog (enables LSI automatically) */
    IWDG_KR = LL_IWDG_KEY_START;

    /* Unlock registers */
    IWDG_KR = LL_IWDG_KEY_UNLOCK;

    /* Set prescaler */
    IWDG_PR = prescaler;

    /* Set reload value */
    IWDG_RLR = reload & 0xFFFUL;

    /* Wait for registers to be updated */
    while (IWDG_SR & (LL_IWDG_SR_PVU | LL_IWDG_SR_RVU))
        ;

    /* Initial refresh */
    IWDG_KR = LL_IWDG_KEY_RELOAD;
}

/**
 * Refresh (feed) the watchdog. Call this periodically in your
 * main loop to prevent a watchdog reset.
 */
static inline void ll_iwdg_refresh(void)
{
    IWDG_KR = LL_IWDG_KEY_RELOAD;
}

/* ============================================================
 * Convenience: common timeout values
 * ============================================================ */

/** Start IWDG with ~1 second timeout */
static inline void ll_iwdg_init_1s(void)
{
    ll_iwdg_init(LL_IWDG_PSC_32, 999);
}

/** Start IWDG with ~2 second timeout */
static inline void ll_iwdg_init_2s(void)
{
    ll_iwdg_init(LL_IWDG_PSC_32, 1999);
}

/** Start IWDG with ~5 second timeout */
static inline void ll_iwdg_init_5s(void)
{
    ll_iwdg_init(LL_IWDG_PSC_64, 2499);
}

/** Start IWDG with ~10 second timeout */
static inline void ll_iwdg_init_10s(void)
{
    ll_iwdg_init(LL_IWDG_PSC_256, 1249);
}

/* ============================================================
 * Reset detection
 * ============================================================ */

/**
 * Check if the last reset was caused by the IWDG.
 * Call early in main() to detect watchdog resets.
 */
static inline int ll_iwdg_caused_reset(void)
{
#if defined(STM32L011xx)
    return (REG32(RCC_BASE + 0x50UL) & (1UL << 29)) != 0;  /* CSR: IWDGRSTF */
#elif defined(STM32L422xx)
    return (REG32(RCC_BASE + 0x94UL) & (1UL << 29)) != 0;  /* CSR: IWDGRSTF */
#elif defined(STM32WBA55xx)
    return (REG32(RCC_BASE + 0xE4UL) & (1UL << 29)) != 0;
#elif defined(STM32H523xx)
    return (REG32(RCC_BASE + 0xF4UL) & (1UL << 29)) != 0;
#endif
}

/**
 * Clear the reset flags (so the next check gives a fresh result).
 * Clears all reset flags, not just IWDG.
 */
static inline void ll_rcc_clear_reset_flags(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x50UL), (1UL << 23));  /* CSR: RMVF */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x94UL), (1UL << 23));  /* CSR: RMVF */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xE4UL), (1UL << 23));
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xF4UL), (1UL << 23));
#endif
}

#endif /* LL_IWDG_H */
