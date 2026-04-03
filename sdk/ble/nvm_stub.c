/**
 * nvm_stub.c — NVM stubs (not needed for advertising)
 */

#include <stdint.h>
#include "nvm.h"

int NVM_Add(uint8_t type, const uint8_t *data, uint16_t size,
            const uint8_t *extra_data, uint16_t extra_size)
{
    (void)type; (void)data; (void)size;
    (void)extra_data; (void)extra_size;
    return 0;  /* success */
}

int NVM_Get(int mode, uint8_t type, uint16_t offset, uint8_t *data, uint16_t size)
{
    (void)mode; (void)type; (void)offset; (void)data; (void)size;
    return 1;  /* not found */
}

int NVM_Compare(uint16_t offset, const uint8_t *data, uint16_t size)
{
    (void)offset; (void)data; (void)size;
    return 1;  /* mismatch */
}

void NVM_Discard(int mode)
{
    (void)mode;
}
