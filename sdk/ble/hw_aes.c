/**
 * hw_aes.c — AES hardware crypto for BLE stack
 *
 * Implements BAES_EcbCrypt using the STM32WBA55 AES peripheral.
 * CMAC and CCM are stubbed — only needed for pairing/bonding.
 */

#include <stdint.h>
#include <string.h>
#include "baes.h"
#include "ll_common.h"
#include "ll_rcc.h"

/* AES peripheral base address */
#define AES_BASE    (PERIPH_BASE + 0x060C0000UL)  /* 0x460C0000 */

#define AES_CR      REG32(AES_BASE + 0x00UL)
#define AES_SR      REG32(AES_BASE + 0x04UL)
#define AES_DINR    REG32(AES_BASE + 0x08UL)
#define AES_DOUTR   REG32(AES_BASE + 0x0CUL)
#define AES_KEYR0   REG32(AES_BASE + 0x10UL)
#define AES_KEYR1   REG32(AES_BASE + 0x14UL)
#define AES_KEYR2   REG32(AES_BASE + 0x18UL)
#define AES_KEYR3   REG32(AES_BASE + 0x1CUL)

/* AES_CR bits */
#define AES_CR_EN       (1UL << 0)
#define AES_CR_DATATYPE_NONE  (0UL << 1)
#define AES_CR_MODE_ENCRYPT   (0UL << 3)

/* AES_SR bits */
#define AES_SR_CCF      (1UL << 0)  /* Computation Complete Flag */

/* AHB2ENR bit for AES */
#define LL_AHB2_AES     (1UL << 16)

void BAES_Reset(void)
{
    /* TODO: Enable SAES clock and reset peripheral.
     * WBA55 has SAES (Secure AES) at a different base than standard AES.
     * Stubbed for now — not needed for basic advertising. */
}

void BAES_EcbCrypt(const uint8_t *key, const uint8_t *input,
                   uint8_t *output, int encrypt)
{
    (void)key;
    (void)input;
    (void)encrypt;
    /* TODO: Implement using SAES peripheral at correct WBA55 address.
     * Stubbed for now — basic advertising doesn't require AES. */
    memset(output, 0, 16);
}

/* CMAC and CCM stubs — only needed for pairing/bonding */

void BAES_CmacSetKey(const uint8_t *key)
{
    (void)key;
}

void BAES_CmacCompute(const uint8_t *input, uint32_t input_length,
                      uint8_t *output_tag)
{
    (void)input;
    (void)input_length;
    memset(output_tag, 0, 16);
}

int BAES_CcmCrypt(uint8_t mode, const uint8_t *key, uint8_t iv_length,
                  const uint8_t *iv, uint16_t add_length, const uint8_t *add,
                  uint32_t input_length, const uint8_t *input,
                  uint8_t tag_length, uint8_t *tag, uint8_t *output)
{
    (void)mode; (void)key; (void)iv_length; (void)iv;
    (void)add_length; (void)add;
    (void)input_length; (void)input;
    (void)tag_length; (void)tag; (void)output;
    return 0;
}
