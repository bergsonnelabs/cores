/**
 * nvm.h -- NVM stub declarations
 */
#ifndef NVM_H
#define NVM_H

#include <stdint.h>

int  NVM_Add(uint8_t type, const uint8_t *data, uint16_t size,
             const uint8_t *extra_data, uint16_t extra_size);
int  NVM_Get(int mode, uint8_t type, uint16_t offset, uint8_t *data, uint16_t size);
int  NVM_Compare(uint16_t offset, const uint8_t *data, uint16_t size);
void NVM_Discard(int mode);

#endif /* NVM_H */
