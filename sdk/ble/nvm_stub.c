/**
 * nvm_stub.c — Minimal RAM-backed NVM for BLE security database
 *
 * The BLE stack's security database (SDB) uses NVM to store bonding
 * data. Without a working NVM, the SDB loops on NVM_Get during
 * connection attempts. This provides a simple RAM-backed implementation
 * that satisfies the SDB's read/write/compare interface.
 */

#include <stdint.h>
#include <string.h>
#include "nvm.h"

/* RAM buffer for NVM emulation */
#define NVM_SIZE  4096
static uint8_t nvm_buffer[NVM_SIZE];
static uint8_t nvm_initialized;

/* Simple record header */
#define NVM_RECORD_MAGIC  0xBE
typedef struct {
    uint8_t  magic;
    uint8_t  type;
    uint16_t size;
} nvm_record_header_t;

/* Read cursor for NVM_FIRST / NVM_NEXT */
static uint16_t nvm_read_cursor;

void NVM_Init(void *buffer, uint32_t offset, uint32_t size)
{
    (void)buffer; (void)offset; (void)size;
    memset(nvm_buffer, 0xFF, NVM_SIZE);
    nvm_initialized = 1;
    nvm_read_cursor = 0;
}

int NVM_Add(uint8_t type, const uint8_t *data, uint16_t size,
            const uint8_t *extra_data, uint16_t extra_size)
{
    if (!nvm_initialized) return -1;

    /* Find free space */
    uint16_t pos = 0;
    while (pos < NVM_SIZE) {
        nvm_record_header_t *hdr = (nvm_record_header_t *)&nvm_buffer[pos];
        if (hdr->magic != NVM_RECORD_MAGIC) break;
        pos += sizeof(nvm_record_header_t) + hdr->size;
    }

    uint16_t total = size + extra_size;
    if (pos + sizeof(nvm_record_header_t) + total > NVM_SIZE) return -1;

    /* Write record */
    nvm_record_header_t *hdr = (nvm_record_header_t *)&nvm_buffer[pos];
    hdr->magic = NVM_RECORD_MAGIC;
    hdr->type = type;
    hdr->size = total;
    memcpy(&nvm_buffer[pos + sizeof(nvm_record_header_t)], data, size);
    if (extra_data && extra_size > 0) {
        memcpy(&nvm_buffer[pos + sizeof(nvm_record_header_t) + size], extra_data, extra_size);
    }

    return 0;
}

int NVM_Get(int mode, uint8_t type, uint16_t offset, uint8_t *data, uint16_t size)
{
    if (!nvm_initialized) return -3;  /* EOF */

    if (mode == 0) {  /* NVM_FIRST */
        nvm_read_cursor = 0;
    }

    /* Search from cursor */
    while (nvm_read_cursor < NVM_SIZE) {
        nvm_record_header_t *hdr = (nvm_record_header_t *)&nvm_buffer[nvm_read_cursor];
        if (hdr->magic != NVM_RECORD_MAGIC) return -3;  /* EOF — no more records */

        uint16_t record_start = nvm_read_cursor + sizeof(nvm_record_header_t);
        nvm_read_cursor = record_start + hdr->size;  /* advance cursor past this record */

        if (hdr->type == type || type == 0xFF) {
            /* Match — copy data from offset */
            uint16_t avail = (offset < hdr->size) ? (hdr->size - offset) : 0;
            uint16_t copy = (size < avail) ? size : avail;
            if (data && copy > 0) {
                memcpy(data, &nvm_buffer[record_start + offset], copy);
            }
            return (int)copy;
        }
    }

    return -3;  /* EOF */
}

int NVM_Compare(uint16_t offset, const uint8_t *data, uint16_t size)
{
    if (!nvm_initialized) return 1;
    if (offset + size > NVM_SIZE) return 1;
    return memcmp(&nvm_buffer[offset], data, size);
}

void NVM_Discard(int mode)
{
    (void)mode;
    if (nvm_initialized) {
        memset(nvm_buffer, 0xFF, NVM_SIZE);
        nvm_read_cursor = 0;
    }
}
