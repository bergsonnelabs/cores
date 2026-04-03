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
    ll_rcc_ahb2_clk_enable(LL_AHB2_AES);
    AES_CR = 0;
}

void BAES_EcbCrypt(const uint8_t *key, const uint8_t *input,
                   uint8_t *output, int encrypt)
{
    (void)encrypt;

    ll_rcc_ahb2_clk_enable(LL_AHB2_AES);

    /* Disable AES before configuration */
    AES_CR = 0;

    /* Load 128-bit key (little-endian word order) */
    uint32_t k[4];
    memcpy(k, key, 16);
    AES_KEYR3 = k[0];
    AES_KEYR2 = k[1];
    AES_KEYR1 = k[2];
    AES_KEYR0 = k[3];

    /* Configure: encrypt, no swap, ECB mode */
    AES_CR = AES_CR_DATATYPE_NONE | AES_CR_MODE_ENCRYPT | AES_CR_EN;

    /* Load 128-bit input */
    uint32_t in[4];
    memcpy(in, input, 16);
    AES_DINR = in[0];
    AES_DINR = in[1];
    AES_DINR = in[2];
    AES_DINR = in[3];

    /* Wait for completion */
    uint32_t timeout = 100000;
    while (!(AES_SR & AES_SR_CCF) && --timeout)
        ;

    /* Read output */
    uint32_t out[4];
    out[0] = AES_DOUTR;
    out[1] = AES_DOUTR;
    out[2] = AES_DOUTR;
    out[3] = AES_DOUTR;
    memcpy(output, out, 16);

    /* Disable */
    AES_CR = 0;
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
