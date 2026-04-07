/**
 * core_backup.h — Backup register access
 *
 * 32-bit registers that persist through resets and Standby mode.
 * Useful for crash counters, boot flags, and state preservation.
 *
 * Count: Core.L has 5 registers (0–4), all others have 32 (0–31).
 * Retained as long as VBAT or VDD is present.
 *
 * Usage:
 *   core_backup_write(0, 0xDEADBEEF);   // store a value
 *   uint32_t val = core_backup_read(0);  // read it back
 *
 * No init required — backup registers are always accessible for reading.
 * Writing requires backup domain access, which core_init() enables.
 */

#ifndef CORE_BACKUP_H
#define CORE_BACKUP_H

#include "ll_rtc.h"
#include "ll_pwr.h"
#include "ll_rcc.h"

/* Number of backup registers per core */
#if defined(STM32L011xx)
  #define CORE_BACKUP_COUNT  5
#else
  #define CORE_BACKUP_COUNT  32
#endif

/**
 * Read a backup register.
 * @param index  Register index (0 to CORE_BACKUP_COUNT-1)
 * @return       32-bit value, or 0 if index is out of range
 */
static inline uint32_t core_backup_read(uint8_t index)
{
    if (index >= CORE_BACKUP_COUNT) return 0;

#if defined(STM32H523xx)
    /* H5: RTCAPBEN must be set for backup register reads.
     * If core_rtc_init() hasn't been called, enable it here. */
    SET_BITS(REG32(RCC_BASE + 0xA8UL), (1UL << 21));  /* APB3ENR: RTCAPBEN */
#endif

    return ll_rtc_bkp_read(index);
}

/**
 * Write a backup register.
 * @param index  Register index (0 to CORE_BACKUP_COUNT-1)
 * @param value  32-bit value to store
 *
 * Backup domain write access is enabled automatically.
 */
static inline void core_backup_write(uint8_t index, uint32_t value)
{
    if (index >= CORE_BACKUP_COUNT) return;

    /* Ensure backup domain access is enabled */
    ll_rcc_pwr_clk_enable();
    ll_pwr_enable_backup_access();

#if defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xA8UL), (1UL << 21));  /* APB3ENR: RTCAPBEN */
#endif

    ll_rtc_bkp_write(index, value);
}

#endif /* CORE_BACKUP_H */
