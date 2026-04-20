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
 * No init required — the RTC clock domain is enabled automatically.
 *
 * @tessera category backup label=Core.Backup icon=◫
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
 * Ensure the RTC/backup register clock domain is accessible.
 * On L0/L4 the backup registers live inside the RTC peripheral block,
 * so RTCEN (and a clock source) must be active for reads AND writes.
 * On WBA/H5 they're in TAMP, which only needs PWR+DBP.
 */
static inline void _core_backup_ensure_clk(void)
{
    ll_rcc_pwr_clk_enable();
    ll_pwr_enable_backup_access();

#if defined(STM32L011xx)
    /* L0: RTCEN in RCC_CSR bit 18 */
    if (!(REG32(RCC_BASE + 0x50UL) & (1UL << 18))) {
        ll_rcc_lsi_enable();
        while (!ll_rcc_lsi_ready()) ;
        ll_rcc_rtc_set_source(2);
        ll_rcc_rtc_enable();
    }
#elif defined(STM32L422xx)
    /* L4: RTCEN in RCC_BDCR bit 15 */
    if (!(REG32(RCC_BASE + 0x90UL) & (1UL << 15))) {
        ll_rcc_lsi_enable();
        while (!ll_rcc_lsi_ready()) ;
        ll_rcc_rtc_set_source(2);
        ll_rcc_rtc_enable();
    }
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xA8UL), (1UL << 21));  /* APB3ENR: RTCAPBEN */
#endif
}

/**
 * Read a backup register.
 *
 * @tessera expose category=backup name=read returns=int
 * @param index [0..31] Register index (Core.L caps at 4; others at 31).
 */
static inline uint32_t core_backup_read(uint8_t index)
{
    if (index >= CORE_BACKUP_COUNT) return 0;
    _core_backup_ensure_clk();
    return ll_rtc_bkp_read(index);
}

/**
 * Write a backup register. Backup domain write access is enabled automatically.
 *
 * @tessera expose category=backup name=write
 * @param index [0..31] Register index (Core.L caps at 4; others at 31).
 * @param value 32-bit value to store.
 */
static inline void core_backup_write(uint8_t index, uint32_t value)
{
    if (index >= CORE_BACKUP_COUNT) return;
    _core_backup_ensure_clk();
    ll_rtc_bkp_write(index, value);
}

#endif /* CORE_BACKUP_H */
