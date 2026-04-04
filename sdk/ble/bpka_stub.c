/**
 * bpka_stub.c — PKA stubs (not needed for advertising)
 */

#include <stdint.h>
#include "bpka.h"

void BPKA_Reset(void) {}

int BPKA_StartP256Key(const uint32_t *local_private_key)
{
    (void)local_private_key;
    return -1;  /* not supported */
}

void BPKA_ReadP256Key(uint32_t *local_public_key) { (void)local_public_key; }

int BPKA_StartDhKey(const uint32_t *local_private_key,
                    const uint32_t *remote_public_key)
{
    (void)local_private_key; (void)remote_public_key;
    return -1;  /* not supported */
}

int BPKA_ReadDhKey(uint32_t *dh_key)
{
    (void)dh_key;
    return -1;  /* not supported */
}

int BPKA_Process(void) { return 0; }
void BPKA_BG_Process(void) {}
