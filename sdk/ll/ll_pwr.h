/**
 * ll_pwr.h — Low-level power control
 *
 * Sleep, Stop, and Standby mode entry. Voltage regulator
 * configuration and backup domain access.
 *
 * Power modes (roughly, across all families):
 *   Sleep:   CPU stopped, peripherals running. Wake by any interrupt.
 *   Stop:    Most clocks stopped, SRAM retained. Wake by EXTI.
 *   Standby: Everything off except RTC/IWDG. Wake by WKUP pin or RTC.
 */

#ifndef LL_PWR_H
#define LL_PWR_H

#include "ll_common.h"

/* ---- PWR base address ---- */

#if defined(STM32L011xx)
  #define PWR_BASE          0x40007000UL
#elif defined(STM32L422xx)
  #define PWR_BASE          0x40007000UL
#elif defined(STM32WBA55xx)
  #define PWR_BASE          0x46020800UL
#elif defined(STM32H523xx)
  #define PWR_BASE          0x44020800UL
#endif

/* ---- Cortex-M SCB registers (for sleep modes) ---- */

#define SCB_SCR             REG32(0xE000ED10UL)
#define SCB_SCR_SLEEPDEEP   (1UL << 2)
#define SCB_SCR_SLEEPONEXIT (1UL << 1)

/* ============================================================
 * PWR clock enable
 * ============================================================ */

static inline void ll_rcc_pwr_clk_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x38UL), (1UL << 28));  /* APB1ENR: PWREN */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x58UL), (1UL << 28));  /* APB1ENR1: PWREN */
#elif defined(STM32WBA55xx)
    /* WBA: PWR is always accessible, no explicit clock gate */
#elif defined(STM32H523xx)
    /* H5: PWR is always accessible via SRD domain */
#endif
    (void)REG32(RCC_BASE);
}

/* ============================================================
 * Backup domain access
 * ============================================================ */

/**
 * Enable access to the backup domain (RTC, backup registers).
 * Must be called before writing to RTC or backup registers.
 */
static inline void ll_pwr_enable_backup_access(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 8));   /* CR: DBP */
#elif defined(STM32L422xx)
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 8));   /* CR1: DBP */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 8));   /* CR1: DBP */
#elif defined(STM32H523xx)
    SET_BITS(REG32(PWR_BASE + 0x04UL), (1UL << 8));   /* DBPCR: DBP */
#endif
}

/* ============================================================
 * Sleep mode
 * ============================================================ */

/**
 * Enter Sleep mode.
 * CPU clock stopped, all peripherals continue running.
 * Wakes on any enabled interrupt.
 *
 * Use WFI (wait for interrupt) or WFE (wait for event).
 */
static inline void ll_pwr_sleep_wfi(void)
{
    CLR_BITS(SCB_SCR, SCB_SCR_SLEEPDEEP);
    __asm volatile ("wfi");
}

static inline void ll_pwr_sleep_wfe(void)
{
    CLR_BITS(SCB_SCR, SCB_SCR_SLEEPDEEP);
    __asm volatile ("wfe");
}

/* ============================================================
 * Stop mode
 * ============================================================ */

/**
 * Enter Stop mode (lowest power with SRAM retention).
 *
 * All oscillators stopped except LSI/LSE. Wakes via EXTI
 * (GPIO, RTC alarm, etc.). After waking, clock is MSI/HSI
 * — you must reinitialize the PLL if needed.
 *
 * On L4: Stop 0, Stop 1, or Stop 2 available.
 * On WBA/H5: Stop 0 or Stop 1.
 */
static inline void ll_pwr_stop(void)
{
#if defined(STM32L011xx)
    /* L0: Set LPSDSR for low-power in Stop, clear PDDS for Stop (not Standby) */
    CLR_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 1));   /* CR: PDDS=0 → Stop */
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 0));   /* CR: LPSDSR → low-power regulator */

#elif defined(STM32L422xx)
    /* L4: Enter Stop 1 mode (good balance of power vs wake time)
       CR1 LPMS[2:0] = 001 → Stop 1 */
    MOD_BITS(REG32(PWR_BASE + 0x00UL), 0x7UL, 0x1UL);

#elif defined(STM32WBA55xx)
    /* WBA: CR1 LPMS[2:0] = 001 → Stop 1 */
    MOD_BITS(REG32(PWR_BASE + 0x00UL), 0x7UL, 0x1UL);

#elif defined(STM32H523xx)
    /* H5: PMCR LPMS = 01 → Stop 1 */
    MOD_BITS(REG32(PWR_BASE + 0x00UL), 0x7UL, 0x1UL);
#endif

    /* Set SLEEPDEEP bit and execute WFI */
    SET_BITS(SCB_SCR, SCB_SCR_SLEEPDEEP);
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("wfi");

    /* After waking: clear SLEEPDEEP */
    CLR_BITS(SCB_SCR, SCB_SCR_SLEEPDEEP);
}

/* ============================================================
 * Standby mode
 * ============================================================ */

/**
 * Enter Standby mode (lowest power, SRAM lost).
 *
 * Only RTC, IWDG, and wakeup pins remain powered.
 * On wake, the MCU resets (starts from Reset_Handler).
 *
 * WARNING: All SRAM contents are lost. Save state to backup
 * registers or flash before entering standby.
 */
static inline void ll_pwr_standby(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 1));   /* CR: PDDS=1 → Standby */

#elif defined(STM32L422xx)
    /* CR1 LPMS[2:0] = 011 → Standby */
    MOD_BITS(REG32(PWR_BASE + 0x00UL), 0x7UL, 0x3UL);

#elif defined(STM32WBA55xx)
    MOD_BITS(REG32(PWR_BASE + 0x00UL), 0x7UL, 0x3UL);

#elif defined(STM32H523xx)
    MOD_BITS(REG32(PWR_BASE + 0x00UL), 0x7UL, 0x3UL);
#endif

    /* Clear wakeup flags */
#if defined(STM32L011xx)
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 2));   /* CR: CWUF */
#elif defined(STM32L422xx)
    REG32(PWR_BASE + 0x14UL) = 0x1F;                  /* SCR: clear all WUF */
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    REG32(PWR_BASE + 0x14UL) = 0x1F;                  /* WUSCR/SCR: clear WUF */
#endif

    SET_BITS(SCB_SCR, SCB_SCR_SLEEPDEEP);
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("wfi");

    /* Should never reach here — MCU resets on standby wake */
    while (1)
        ;
}

/* ============================================================
 * Wakeup flags
 * ============================================================ */

/** Check if we woke from standby */
static inline int ll_pwr_woke_from_standby(void)
{
#if defined(STM32L011xx)
    return (REG32(PWR_BASE + 0x04UL) & (1UL << 1)) != 0;  /* CSR: SBF */
#elif defined(STM32L422xx)
    return (REG32(PWR_BASE + 0x10UL) & (1UL << 8)) != 0;  /* SR1: SBF */
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    return (REG32(PWR_BASE + 0x10UL) & (1UL << 8)) != 0;  /* SR1: SBF */
#endif
}

/** Clear the standby flag */
static inline void ll_pwr_clear_standby_flag(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(PWR_BASE + 0x00UL), (1UL << 3));   /* CR: CSBF */
#elif defined(STM32L422xx)
    SET_BITS(REG32(PWR_BASE + 0x14UL), (1UL << 8));   /* SCR: CSBF */
#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    SET_BITS(REG32(PWR_BASE + 0x14UL), (1UL << 8));
#endif
}

#endif /* LL_PWR_H */
