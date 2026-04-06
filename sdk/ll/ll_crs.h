/**
 * ll_crs.h — Clock Recovery System
 *
 * Synchronizes the HSI48 oscillator to USB SOF packets for
 * crystal-less USB operation. Available on STM32L4.
 *
 * Usage:
 *   1. Enable HSI48 (ll_rcc_hsi48_enable)
 *   2. Enable CRS peripheral clock
 *   3. Call ll_crs_usb_sync_enable() — auto-trims HSI48 to USB SOF
 *
 * The CRS automatically adjusts HSI48 frequency to maintain
 * ±0.25% accuracy required by USB Full-Speed.
 */

#ifndef LL_CRS_H
#define LL_CRS_H

#include "ll_common.h"

#if defined(STM32L422xx)

/* CRS base address */
#define CRS_BASE            (PERIPH_BASE + 0x6000UL)

/* CRS registers */
#define CRS_CR              REG32(CRS_BASE + 0x00UL)
#define CRS_CFGR            REG32(CRS_BASE + 0x04UL)
#define CRS_ISR             REG32(CRS_BASE + 0x08UL)
#define CRS_ICR             REG32(CRS_BASE + 0x0CUL)

/* CRS_CR bits */
#define CRS_CR_AUTOTRIMEN   (1UL << 6)    /* Auto-trimming enable */
#define CRS_CR_CEN          (1UL << 5)    /* Frequency error counter enable */

/* CRS_CFGR bits */
#define CRS_CFGR_SYNCSRC_MASK  (3UL << 28)
#define CRS_CFGR_SYNCSRC_GPIO  (0UL << 28)
#define CRS_CFGR_SYNCSRC_LSE   (1UL << 28)
#define CRS_CFGR_SYNCSRC_USB   (2UL << 28)

/* CRS_CFGR reset value has correct RELOAD (0x0000BB7F = 47999 for 48MHz/1kHz SOF)
 * and FELIM (0x22 = 34). We only need to change SYNCSRC. */

/* CRS peripheral clock enable (APB1ENR1 bit 24) */
#define LL_APB1_CRS         (1UL << 24)

/**
 * Enable CRS auto-sync from USB SOF packets.
 * Prerequisites: HSI48 enabled and stable, CRS clock enabled.
 */
static inline void ll_crs_usb_sync_enable(void)
{
    /* Set sync source to USB SOF */
    MOD_BITS(CRS_CFGR, CRS_CFGR_SYNCSRC_MASK, CRS_CFGR_SYNCSRC_USB);

    /* Enable auto-trimming and frequency error counter */
    SET_BITS(CRS_CR, CRS_CR_AUTOTRIMEN | CRS_CR_CEN);
}

/**
 * Disable CRS.
 */
static inline void ll_crs_disable(void)
{
    CLR_BITS(CRS_CR, CRS_CR_AUTOTRIMEN | CRS_CR_CEN);
}

#endif /* STM32L422xx */

#if defined(STM32H523xx)

/* CRS base address — same APB1 offset as L4 */
#define CRS_BASE            (PERIPH_BASE + 0x6000UL)

/* CRS registers */
#define CRS_CR              REG32(CRS_BASE + 0x00UL)
#define CRS_CFGR            REG32(CRS_BASE + 0x04UL)
#define CRS_ISR             REG32(CRS_BASE + 0x08UL)
#define CRS_ICR             REG32(CRS_BASE + 0x0CUL)

/* CRS_CR bits */
#define CRS_CR_AUTOTRIMEN   (1UL << 6)
#define CRS_CR_CEN          (1UL << 5)

/* CRS_CFGR bits */
#define CRS_CFGR_SYNCSRC_MASK  (3UL << 28)
#define CRS_CFGR_SYNCSRC_USB   (2UL << 28)

/**
 * Enable CRS auto-sync from USB SOF packets.
 * Prerequisites: HSI48 enabled and stable, CRS clock enabled.
 */
static inline void ll_crs_usb_sync_enable(void)
{
    MOD_BITS(CRS_CFGR, CRS_CFGR_SYNCSRC_MASK, CRS_CFGR_SYNCSRC_USB);
    SET_BITS(CRS_CR, CRS_CR_AUTOTRIMEN | CRS_CR_CEN);
}

static inline void ll_crs_disable(void)
{
    CLR_BITS(CRS_CR, CRS_CR_AUTOTRIMEN | CRS_CR_CEN);
}

#endif /* STM32H523xx */

#endif /* LL_CRS_H */
