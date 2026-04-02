/**
 * ll_flash.h — Low-level internal flash erase/program
 *
 * STM32L4 flash programs in double-words (8 bytes). Addresses must
 * be 8-byte aligned. Pages are 2KB on STM32L422 (64 pages, 128KB).
 *
 * Usage:
 *   ll_flash_unlock();
 *   ll_flash_erase_page(page_num);
 *   ll_flash_program_dword(addr, word0, word1);
 *   ll_flash_lock();
 */

#ifndef LL_FLASH_H
#define LL_FLASH_H

#include "ll_common.h"

/* FLASH_BASE is defined in ll_rcc.h (0x40022000 for L422) */

/* ---- Register offsets ---- */

#define FLASH_KEYR          REG32(FLASH_BASE + 0x08UL)
#define FLASH_SR            REG32(FLASH_BASE + 0x10UL)
#define FLASH_CR            REG32(FLASH_BASE + 0x14UL)

/* ---- Unlock keys ---- */

#define FLASH_KEY1          0x45670123UL
#define FLASH_KEY2          0xCDEF89ABUL

/* ---- SR bits ---- */

#define FLASH_SR_EOP        (1UL << 0)
#define FLASH_SR_OPERR      (1UL << 1)
#define FLASH_SR_PROGERR    (1UL << 3)
#define FLASH_SR_WRPERR     (1UL << 4)
#define FLASH_SR_PGAERR     (1UL << 5)
#define FLASH_SR_SIZERR     (1UL << 6)
#define FLASH_SR_PGSERR     (1UL << 7)
#define FLASH_SR_MISERR     (1UL << 8)
#define FLASH_SR_FASTERR    (1UL << 9)
#define FLASH_SR_BSY        (1UL << 16)

#define FLASH_SR_ERR_MASK   (FLASH_SR_OPERR | FLASH_SR_PROGERR | \
                             FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
                             FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
                             FLASH_SR_MISERR | FLASH_SR_FASTERR)

/* ---- CR bits ---- */

#define FLASH_CR_PG         (1UL << 0)
#define FLASH_CR_PER        (1UL << 1)
#define FLASH_CR_MER1       (1UL << 2)
#define FLASH_CR_PNB_SHIFT  3
#define FLASH_CR_PNB_MASK   (0xFFUL << FLASH_CR_PNB_SHIFT)
#define FLASH_CR_STRT       (1UL << 16)
#define FLASH_CR_LOCK       (1UL << 31)

/* ---- Page geometry ---- */

#if defined(STM32L422xx)
  #define FLASH_PAGE_SIZE   2048
  #define FLASH_START       0x08000000UL
#endif

/* ============================================================
 * Functions
 * ============================================================ */

/** Unlock flash for erase/program. */
static inline void ll_flash_unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}

/** Lock flash (re-enable write protection). */
static inline void ll_flash_lock(void)
{
    SET_BITS(FLASH_CR, FLASH_CR_LOCK);
}

/** Spin until flash is not busy. */
static inline void ll_flash_wait_bsy(void)
{
    while (FLASH_SR & FLASH_SR_BSY)
        ;
}

/** Clear all error flags in FLASH_SR. */
static inline void ll_flash_clear_errors(void)
{
    FLASH_SR = FLASH_SR_ERR_MASK | FLASH_SR_EOP;
}

/**
 * Erase a single flash page.
 * Returns 0 on success, -1 on error.
 */
static inline int ll_flash_erase_page(uint32_t page)
{
    ll_flash_wait_bsy();
    ll_flash_clear_errors();

    /* Set PER + page number, then start */
    FLASH_CR = FLASH_CR_PER | (page << FLASH_CR_PNB_SHIFT);
    SET_BITS(FLASH_CR, FLASH_CR_STRT);

    ll_flash_wait_bsy();

    /* Clear PER */
    CLR_BITS(FLASH_CR, FLASH_CR_PER);

    if (FLASH_SR & FLASH_SR_ERR_MASK)
        return -1;

    return 0;
}

/**
 * Program a double-word (8 bytes) at an 8-byte aligned address.
 * Returns 0 on success, -1 on error.
 */
static inline int ll_flash_program_dword(uint32_t addr, uint32_t word0, uint32_t word1)
{
    ll_flash_wait_bsy();
    ll_flash_clear_errors();

    /* Enable programming */
    SET_BITS(FLASH_CR, FLASH_CR_PG);

    /* Write the two 32-bit words (must be in order, low then high) */
    *(volatile uint32_t *)(addr)     = word0;
    *(volatile uint32_t *)(addr + 4) = word1;

    ll_flash_wait_bsy();

    /* Clear PG */
    CLR_BITS(FLASH_CR, FLASH_CR_PG);

    if (FLASH_SR & FLASH_SR_ERR_MASK)
        return -1;

    return 0;
}

#endif /* LL_FLASH_H */
