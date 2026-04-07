/**
 * ll_rng.h — Low-level True Random Number Generator (TRNG)
 *
 * The RNG peripheral provides hardware-generated random numbers using
 * analog noise sources. Available on STM32WBA55 and STM32H523.
 *
 * Usage:
 *   ll_rcc_ahb2_clk_enable(LL_AHB2_RNG);
 *   ll_rng_enable();
 *   uint32_t val = ll_rng_read();   // blocking
 *   ll_rng_disable();
 */

#ifndef LL_RNG_H
#define LL_RNG_H

#include "ll_common.h"

/* ---- RNG base address ---- */

#if defined(STM32L422xx)
  #define RNG_BASE    0x50060800UL  /* AHB2 */
#elif defined(STM32WBA55xx)
  #define RNG_BASE    (PERIPH_BASE + 0x020C0800UL)  /* 0x420C0800 — AHB2 */
#elif defined(STM32H523xx)
  #define RNG_BASE    (PERIPH_BASE + 0x020C0800UL)  /* Same offset on H5 */
#endif

/* ---- Registers ---- */

#define RNG_CR      REG32(RNG_BASE + 0x00UL)
#define RNG_SR      REG32(RNG_BASE + 0x04UL)
#define RNG_DR      REG32(RNG_BASE + 0x08UL)

/* ---- CR bits ---- */

#define LL_RNG_CR_RNGEN     (1UL << 2)   /* RNG enable */
#define LL_RNG_CR_IE        (1UL << 3)   /* Interrupt enable */
#define LL_RNG_CR_CED       (1UL << 5)   /* Clock error detection disable */
#define LL_RNG_CR_CONDRST   (1UL << 6)   /* Conditioning soft reset */

/* ---- SR bits ---- */

#define LL_RNG_SR_DRDY      (1UL << 0)   /* Data ready */
#define LL_RNG_SR_CECS      (1UL << 1)   /* Clock error current status */
#define LL_RNG_SR_SECS      (1UL << 2)   /* Seed error current status */
#define LL_RNG_SR_CEIS      (1UL << 5)   /* Clock error interrupt status */
#define LL_RNG_SR_SEIS      (1UL << 6)   /* Seed error interrupt status */

/* ---- AHB2ENR bit for RNG clock ---- */

#define LL_AHB2_RNG         (1UL << 18)

/* ============================================================
 * Control
 * ============================================================ */

/**
 * Enable the RNG peripheral with proper clock and conditioning.
 *
 * On STM32WBA55, two things are needed before RNGEN:
 *   1. Select the RNG kernel clock source (HSI16 via RCC_CCIPR2)
 *   2. Perform a conditioning soft reset (CONDRST toggle)
 *
 * Must call ll_rcc_ahb2_clk_enable(LL_AHB2_RNG) before this.
 */
static inline void ll_rng_enable(void)
{
#if defined(STM32WBA55xx)
    /* Select HSI16 as RNG clock source: CCIPR2 bits [13:12] = 0b10 */
    MOD_BITS(REG32(RCC_BASE + 0xE4UL), 0x3UL << 12, 0x2UL << 12);
#endif

#if defined(STM32L422xx)
    /* L422: simple RNG — just enable, no conditioning reset needed.
     * RNG clock comes from HSI48 (must be enabled for USB or RNG). */
    RNG_CR = LL_RNG_CR_RNGEN;
#else
    /* WBA55/H523: conditioning reset + enable */
    RNG_CR = 0;
    RNG_CR = LL_RNG_CR_CONDRST | LL_RNG_CR_RNGEN;

    /* Brief delay for conditioning logic to latch */
    for (volatile int i = 0; i < 100; i++) ;

    /* Clear CONDRST, keep RNGEN — completes conditioning */
    RNG_CR = LL_RNG_CR_RNGEN;
#endif

    /* Wait for first DRDY (first number ready) */
    for (volatile uint32_t t = 200000; t; t--) {
        if (RNG_SR & LL_RNG_SR_DRDY) {
            (void)RNG_DR;  /* Discard first value */
            break;
        }
    }
}

/** Disable the RNG peripheral. */
static inline void ll_rng_disable(void)
{
    RNG_CR = 0;
}

/** Check if a random number is ready. */
static inline int ll_rng_data_ready(void)
{
    return (RNG_SR & LL_RNG_SR_DRDY) != 0;
}

/** Check for seed error. */
static inline int ll_rng_seed_error(void)
{
    return (RNG_SR & LL_RNG_SR_SECS) != 0;
}

/** Check for clock error. */
static inline int ll_rng_clock_error(void)
{
    return (RNG_SR & LL_RNG_SR_CECS) != 0;
}

/**
 * Read a random 32-bit value (blocking).
 * Returns 0 on timeout or error.
 */
static inline uint32_t ll_rng_read(void)
{
    for (volatile uint32_t t = 100000; t; t--) {
        if (RNG_SR & LL_RNG_SR_DRDY)
            return RNG_DR;
    }
    return 0;  /* timeout */
}

/**
 * Fill a buffer with random 32-bit values (blocking).
 * Returns number of values successfully read.
 */
static inline uint32_t ll_rng_fill(uint32_t *buf, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        uint32_t val = ll_rng_read();
        if (val == 0 && !ll_rng_data_ready())
            return i;  /* timeout */
        buf[i] = val;
    }
    return count;
}

#endif /* LL_RNG_H */
