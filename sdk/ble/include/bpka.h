/**
 * bpka.h -- BLE PKA stub declarations
 */
#ifndef BPKA_H
#define BPKA_H

#include <stdint.h>

void BPKA_Init(void);
int  BPKA_IsReady(void);
void BPKA_Reset(void);
void BPKA_BG_Process(void);
int  BPKA_StartP256Key(const uint32_t *local_private_key);
void BPKA_ReadP256Key(uint32_t *local_public_key);
int  BPKA_StartDhKey(const uint32_t *local_private_key, const uint32_t *remote_public_key);
int  BPKA_ReadDhKey(uint32_t *dh_key);

#endif /* BPKA_H */
