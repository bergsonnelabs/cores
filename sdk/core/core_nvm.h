/**
 * core_nvm.h — Non-volatile memory (EEPROM / flash emulation)
 *
 * Provides a simple byte-level read/write API that works across
 * all Core tiles:
 *   - Core.L (STM32L011): True EEPROM, 512 bytes at 0x08080000
 *   - Core.U (STM32L422): Flash emulation (TODO)
 *   - Core.W (STM32WBA55): Flash emulation (TODO)
 *   - Core.H (STM32H523): Flash emulation (TODO)
 *
 * Usage:
 *   core_nvm_write(0, &data, sizeof(data));  // write at offset 0
 *   core_nvm_read(0, &data, sizeof(data));   // read back
 *
 * The NVM region is persistent across power cycles and resets.
 * On EEPROM-equipped cores, writes are byte-granular and require
 * no erase. On flash-emulated cores, wear-leveling is handled
 * internally (not yet implemented).
 *
 * @tessera category nvm label=Core.NVM icon=▤
 */

#ifndef CORE_NVM_H
#define CORE_NVM_H

#include <stdint.h>
#include <string.h>
#include "ll_common.h"

/* ============================================================
 * Platform-specific NVM configuration
 * ============================================================ */

#if defined(STM32L011xx)
  /* True EEPROM: 512 bytes, byte-writable, no erase needed */
  #define CORE_NVM_BASE     0x08080000UL
  #define CORE_NVM_SIZE     512
  #define CORE_NVM_EEPROM   1

  /* FLASH PECR register for EEPROM unlock */
  #define FLASH_PECR        REG32(0x40022004UL)  /* FLASH_BASE + 0x04 */
  #define FLASH_PEKEYR      REG32(0x4002200CUL)  /* FLASH_BASE + 0x0C */
  #define FLASH_PECR_PELOCK (1UL << 0)

  /* Unlock keys for PECR (same across L0 family) */
  #define FLASH_PEKEY1      0x89ABCDEFUL
  #define FLASH_PEKEY2      0x02030405UL

#elif defined(STM32L422xx) || defined(STM32WBA55xx) || defined(STM32H523xx)
  /* Flash emulation — not yet implemented */
  #define CORE_NVM_BASE     0
  #define CORE_NVM_SIZE     0
  #define CORE_NVM_EEPROM   0
#endif

/* ============================================================
 * API
 * ============================================================ */

/**
 * Read bytes from NVM.
 *   offset: byte offset within NVM region (0 to CORE_NVM_SIZE-1)
 *   buf:    destination buffer
 *   len:    number of bytes to read
 *
 * Returns 0 on success, -1 if out of range.
 */
static inline int core_nvm_read(uint32_t offset, void *buf, uint32_t len)
{
    if (offset + len > CORE_NVM_SIZE)
        return -1;

    memcpy(buf, (const void *)(CORE_NVM_BASE + offset), len);
    return 0;
}

/**
 * Write bytes to NVM.
 *   offset: byte offset within NVM region (0 to CORE_NVM_SIZE-1)
 *   data:   source buffer
 *   len:    number of bytes to write
 *
 * Returns 0 on success, -1 if out of range or write error.
 */
static inline int core_nvm_write(uint32_t offset, const void *data, uint32_t len)
{
    if (offset + len > CORE_NVM_SIZE)
        return -1;

#if CORE_NVM_EEPROM
    /* STM32L0: True EEPROM — unlock PECR, write bytes directly, re-lock.
     * The hardware handles erase-before-write internally.
     * Each byte write takes ~3.2ms (tPROG). */

    /* Unlock PECR if locked */
    if (FLASH_PECR & FLASH_PECR_PELOCK) {
        FLASH_PEKEYR = FLASH_PEKEY1;
        FLASH_PEKEYR = FLASH_PEKEY2;
    }

    /* Write byte-by-byte */
    const uint8_t *src = (const uint8_t *)data;
    volatile uint8_t *dst = (volatile uint8_t *)(CORE_NVM_BASE + offset);
    for (uint32_t i = 0; i < len; i++) {
        dst[i] = src[i];
        /* Wait for write to complete (BSY flag in FLASH_SR) */
        while (REG32(0x40022018UL) & (1UL << 0))  /* FLASH_SR: BSY */
            ;
    }

    /* Re-lock */
    SET_BITS(FLASH_PECR, FLASH_PECR_PELOCK);

    return 0;
#else
    /* Flash emulation not yet implemented */
    (void)data;
    return -1;
#endif
}

/**
 * Returns the total NVM size in bytes for this Core.
 *
 * @tessera expose category=nvm name=size returns=int
 */
static inline uint32_t core_nvm_size(void)
{
    return CORE_NVM_SIZE;
}

#endif /* CORE_NVM_H */
