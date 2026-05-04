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
 *
 * @tessera coverage
 *   id:    backup
 *   name:  Backup — persistent registers
 *   blurb: 32-bit registers retained through reset and Standby (and across
 *          power loss when VBAT is wired). Tier 2 read/write helpers take
 *          an index and resolve the underlying RTC/TAMP block per Core
 *          family — Core.L exposes 5 registers, others expose 32. Useful
 *          for boot counters, sticky flags, and small scraps of state.
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
 * @tessera twin full
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
 * @tessera twin full
 * @param index [0..31] Register index (Core.L caps at 4; others at 31).
 * @param value 32-bit value to store.
 */
static inline void core_backup_write(uint8_t index, uint32_t value)
{
    if (index >= CORE_BACKUP_COUNT) return;
    _core_backup_ensure_clk();
    ll_rtc_bkp_write(index, value);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=L title="Twin backup state wipes on Reset"
//   read/write round-trip within a single run, but the per-slot store
//   is cleared on every project reload — DSL programs that rely on
//   backup state surviving a soft reset (e.g., boot counters across
//   the user clicking Reset in the IDE) can't be exercised end-to-end.
//
// @tessera unsupported tier=1 value=L title="No tamper / anti-tamper integration"
//   The TAMP block on WBA/H5 hosts the backup registers but also drives
//   tamper detection (active edge, anti-tamper erase, time-stamping on
//   tamper). None of that is wrapped — backup is read/write only.

#endif /* CORE_BACKUP_H */
