/**
 * core_rng.h — Hardware Random Number Generator
 *
 * Generates true random numbers using analog noise sources.
 * Available on Core.U (STM32L422), Core.W (STM32WBA55), and Core.H (STM32H523).
 * Not available on Core.L (STM32L011).
 *
 * Usage:
 *   core_rng_init();
 *   uint32_t val = core_rng_read();     // single 32-bit random value
 *   uint32_t buf[4];
 *   core_rng_fill(buf, 4);              // fill buffer with random values
 *   core_rng_deinit();                  // power down (optional)
 *
 * @tessera coverage
 *   id:    rng
 *   name:  RNG — hardware random numbers
 *   blurb: Tier 1 only. Wraps the analog-noise RNG peripheral on
 *          Core.U / Core.W / Core.H (Core.L has no RNG). Exposes single
 *          32-bit reads, bulk fill, seed-error check, and deinit.
 *          No DSL surface yet — exposing as a Tier 2 default-instance
 *          (`rng.read32 -> int`) is straightforward but not landed.
 */

#ifndef CORE_RNG_H
#define CORE_RNG_H

#if !defined(STM32L422xx) && !defined(STM32WBA55xx) && !defined(STM32H523xx)
#error "core_rng.h: Hardware RNG is not available on this Core tile. Only Core.U, Core.W, and Core.H have an RNG peripheral."
#endif

#include "ll_rng.h"
#include "ll_rcc.h"

/**
 * Initialize the hardware RNG.
 * Enables the peripheral clock, configures the RNG, and waits
 * for the first random number to be ready.
 *
 * On Core.U: requires HSI48 to be running (auto-enabled if USB is used,
 * otherwise call ll_rcc_hsi48_enable() first).
 */
static inline void core_rng_init(void)
{
#if defined(STM32L422xx)
    /* L422 RNG is clocked from HSI48 — ensure it's running */
    ll_rcc_hsi48_enable();
    while (!ll_rcc_hsi48_ready())
        ;
    /* Select HSI48 as RNG clock: RCC_CCIPR bits [29:28] = 0b10 (HSI48) */
    MOD_BITS(REG32(RCC_BASE + 0x88UL), 0x3UL << 28, 0x2UL << 28);
#elif defined(STM32H523xx)
    /* H5 RNG is clocked from HSI48. Enable it if not already running
     * (USB init may have already enabled it). */
    ll_rcc_hsi48_enable();
    while (!ll_rcc_hsi48_ready())
        ;
    /* Select HSI48 as RNG clock: RCC_CCIPR5 bits [3:2] = 0b11 (HSI48) */
    MOD_BITS(REG32(RCC_BASE + 0xD4UL), 0x3UL << 2, 0x3UL << 2);
#endif
    /* WBA55: RNG clock source is set inside ll_rng_enable() (HSI16 via CCIPR2) */

    ll_rcc_ahb2_clk_enable(LL_AHB2_RNG);
    ll_rng_enable();
}

/**
 * Read a single 32-bit random value (blocking).
 * Returns 0 on timeout — check core_rng_error() if this happens.
 */
static inline uint32_t core_rng_read(void)
{
    return ll_rng_read();
}

/**
 * Fill a buffer with random 32-bit values (blocking).
 * @param buf    Destination buffer
 * @param count  Number of uint32_t values to generate
 * @return       Number of values successfully generated
 */
static inline uint32_t core_rng_fill(uint32_t *buf, uint32_t count)
{
    return ll_rng_fill(buf, count);
}

/** Check if the RNG has a seed error (entropy source failure). */
static inline int core_rng_error(void)
{
    return ll_rng_seed_error() || ll_rng_clock_error();
}

/** Power down the RNG peripheral. */
static inline void core_rng_deinit(void)
{
    ll_rng_disable();
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=H title="No DSL surface for RNG"
//   Wrapping core_rng_read as `rng.read32 -> int` is one-line cheap.
//   Until it lands, DSL programs that need randomness (jitter, salt,
//   game seeds) have to use software PRNGs in C.
//
// @tessera unsupported tier=2 value=M title="Twin would need a deterministic PRNG"
//   Real RNG output isn't reproducible across worker runs, so the
//   simulator should back the host call with a seeded PRNG (LCG /
//   xorshift) so the same DSL program plays back identically each
//   replay. Worth thinking through before exposing.
//
// @tessera unsupported tier=1 value=L title="No bias correction / health tests"
//   The hardware exposes a CED (clock error) and SEIS (seed error)
//   bit which core_rng_error checks, but there's no NIST SP 800-90B
//   continuous-health-test wrapper. Crypto-grade users should consume
//   the raw words through a vetted DRBG (mbedTLS, etc.).

#endif /* CORE_RNG_H */
