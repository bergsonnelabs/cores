/**
 * nvm_stub.c — Flash-backed NVM for BLE bonding data
 *
 * Uses the last flash page (page 127, 8KB) on WBA55 to persist
 * BLE security database records across power cycles.
 *
 * Layout: simple append-only log of records. Each record has a
 * 4-byte header (magic + type + size) followed by data, aligned
 * to 8 bytes (flash double-word program granularity).
 *
 * When the page fills up, it's erased and records are compacted.
 * For simplicity, the current implementation uses RAM as a write
 * buffer and flushes to flash on NVM_Add.
 */

#include <stdint.h>
#include <string.h>
#include "nvm.h"
#include "ll_common.h"
#include "ll_flash.h"

/* NVM flash page — last page of 1MB flash */
#define NVM_PAGE         127
#define NVM_FLASH_ADDR   (FLASH_START + NVM_PAGE * FLASH_PAGE_SIZE)
#define NVM_FLASH_SIZE   FLASH_PAGE_SIZE  /* 8KB */

/* Record header */
#define NVM_RECORD_MAGIC 0xBE
typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  type;
    uint16_t size;
} nvm_record_header_t;

/* RAM mirror for read operations */
#define NVM_RAM_SIZE     4096
static uint8_t nvm_ram[NVM_RAM_SIZE];
static uint8_t nvm_initialized;
static uint16_t nvm_write_pos;

/* Read cursor */
static uint16_t nvm_read_cursor;

/* ---- Flash helpers ---- */

#if defined(STM32WBA55xx)

static void nvm_flash_write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    ll_flash_unlock();
    ll_flash_clear_errors();

    /* Program in double-words (8 bytes) */
    uint32_t aligned_addr = addr & ~7UL;
    const uint32_t *src = (const uint32_t *)data;

    for (uint16_t i = 0; i < len; i += 8) {
        uint32_t w0 = (i < len) ? src[i/4] : 0xFFFFFFFF;
        uint32_t w1 = (i+4 < len) ? src[i/4 + 1] : 0xFFFFFFFF;
        ll_flash_program_dword(aligned_addr + i, w0, w1);
    }

    ll_flash_lock();
}

static void nvm_flash_erase(void)
{
    ll_flash_unlock();
    ll_flash_erase_page(NVM_PAGE);
    ll_flash_lock();
}

static void nvm_load_from_flash(void)
{
    /* Copy flash contents to RAM mirror */
    const uint8_t *flash = (const uint8_t *)NVM_FLASH_ADDR;

    nvm_write_pos = 0;
    memset(nvm_ram, 0xFF, NVM_RAM_SIZE);

    /* Scan for valid records */
    uint16_t pos = 0;
    while (pos + sizeof(nvm_record_header_t) < NVM_FLASH_SIZE) {
        const nvm_record_header_t *hdr = (const nvm_record_header_t *)&flash[pos];
        if (hdr->magic != NVM_RECORD_MAGIC) break;

        uint16_t total = sizeof(nvm_record_header_t) + hdr->size;
        uint16_t aligned = (total + 7) & ~7;  /* 8-byte aligned */

        if (pos + aligned > NVM_FLASH_SIZE) break;
        if (nvm_write_pos + aligned > NVM_RAM_SIZE) break;

        memcpy(&nvm_ram[nvm_write_pos], &flash[pos], total);
        nvm_write_pos += aligned;
        pos += aligned;
    }
}

static void nvm_flush_to_flash(void)
{
    if (nvm_write_pos == 0) return;

    nvm_flash_erase();
    nvm_flash_write(NVM_FLASH_ADDR, nvm_ram, nvm_write_pos);
}

#else

/* Non-WBA55: RAM-only (no flash persistence) */
static void nvm_load_from_flash(void) { memset(nvm_ram, 0xFF, NVM_RAM_SIZE); nvm_write_pos = 0; }
static void nvm_flush_to_flash(void) { }

#endif

/* ---- Public API ---- */

void NVM_Init(void *buffer, uint32_t offset, uint32_t size)
{
    (void)buffer; (void)offset; (void)size;
    nvm_load_from_flash();
    nvm_initialized = 1;
    nvm_read_cursor = 0;
}

int NVM_Add(uint8_t type, const uint8_t *data, uint16_t size,
            const uint8_t *extra_data, uint16_t extra_size)
{
    if (!nvm_initialized) return -1;

    uint16_t total_data = size + extra_size;
    uint16_t record_size = sizeof(nvm_record_header_t) + total_data;
    uint16_t aligned = (record_size + 7) & ~7;

    if (nvm_write_pos + aligned > NVM_RAM_SIZE) {
        /* NVM full — erase and start fresh */
        memset(nvm_ram, 0xFF, NVM_RAM_SIZE);
        nvm_write_pos = 0;
    }

    /* Write header */
    nvm_record_header_t *hdr = (nvm_record_header_t *)&nvm_ram[nvm_write_pos];
    hdr->magic = NVM_RECORD_MAGIC;
    hdr->type = type;
    hdr->size = total_data;

    /* Write data */
    memcpy(&nvm_ram[nvm_write_pos + sizeof(nvm_record_header_t)], data, size);
    if (extra_data && extra_size > 0) {
        memcpy(&nvm_ram[nvm_write_pos + sizeof(nvm_record_header_t) + size],
               extra_data, extra_size);
    }

    nvm_write_pos += aligned;

    /* Flush to flash for persistence */
    nvm_flush_to_flash();

    return 0;
}

int NVM_Get(int mode, uint8_t type, uint16_t offset, uint8_t *data, uint16_t size)
{
    if (!nvm_initialized) return -3;  /* EOF */

    if (mode == 0) {  /* NVM_FIRST */
        nvm_read_cursor = 0;
    }

    /* Search from cursor */
    while (nvm_read_cursor < nvm_write_pos) {
        nvm_record_header_t *hdr = (nvm_record_header_t *)&nvm_ram[nvm_read_cursor];
        if (hdr->magic != NVM_RECORD_MAGIC) return -3;

        uint16_t record_start = nvm_read_cursor + sizeof(nvm_record_header_t);
        uint16_t aligned = (sizeof(nvm_record_header_t) + hdr->size + 7) & ~7;
        nvm_read_cursor += aligned;

        if (hdr->type == type || type == 0xFF) {
            uint16_t avail = (offset < hdr->size) ? (hdr->size - offset) : 0;
            uint16_t copy = (size < avail) ? size : avail;
            if (data && copy > 0) {
                memcpy(data, &nvm_ram[record_start + offset], copy);
            }
            return (int)copy;
        }
    }

    return -3;  /* EOF */
}

int NVM_Compare(uint16_t offset, const uint8_t *data, uint16_t size)
{
    if (!nvm_initialized) return 1;
    if (offset + size > nvm_write_pos) return 1;
    return memcmp(&nvm_ram[offset], data, size);
}

void NVM_Discard(int mode)
{
    (void)mode;
    if (nvm_initialized) {
        memset(nvm_ram, 0xFF, NVM_RAM_SIZE);
        nvm_write_pos = 0;
        nvm_read_cursor = 0;
        nvm_flush_to_flash();
    }
}
