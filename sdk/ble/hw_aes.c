/**
 * hw_aes.c — AES-ECB encryption using STM32WBA55 AES hardware
 *
 * Matches ST's baes_ecb.c + hw_aes.c byte ordering exactly:
 * - Key loaded in direct order (HW_AES_REV mode)
 * - Input/output word-order swapped (BAES_SWAP)
 * - DATATYPE=00 (32-bit, no byte swap within words)
 */

#include <stdint.h>
#include <string.h>
#include "baes.h"
#include "ll_common.h"
#include "ll_rcc.h"

/* AES peripheral (non-secure, AHB2 domain) */
#define AES_BASE_ADDR  (PERIPH_BASE + 0x020C0000UL)  /* 0x420C0000 */

#define AES_CR_REG    REG32(AES_BASE_ADDR + 0x00UL)
#define AES_SR_REG    REG32(AES_BASE_ADDR + 0x04UL)
#define AES_DINR_REG  REG32(AES_BASE_ADDR + 0x08UL)
#define AES_DOUTR_REG REG32(AES_BASE_ADDR + 0x0CUL)
#define AES_KEYR0_REG REG32(AES_BASE_ADDR + 0x10UL)
#define AES_KEYR1_REG REG32(AES_BASE_ADDR + 0x14UL)
#define AES_KEYR2_REG REG32(AES_BASE_ADDR + 0x18UL)
#define AES_KEYR3_REG REG32(AES_BASE_ADDR + 0x1CUL)
#define AES_ICR_REG   REG32(AES_BASE_ADDR + 0x08UL)  /* same offset as DINR on WBA55? */

/* Actually ICR is at different offset. Let me check. */
/* On STM32WBA: CR=0x00, SR=0x04, DINR=0x08, DOUTR=0x0C, KEYR0-3=0x10-0x1C
 * ICR is at 0x20 according to CMSIS AES_TypeDef layout */
#undef AES_ICR_REG
#define AES_ICR_REG   REG32(AES_BASE_ADDR + 0x20UL)

/* CR bits */
#define AES_CR_EN       (1UL << 0)

/* SR bits */
#define AES_SR_CCF      (1UL << 0)
#define AES_SR_KEYVALID (1UL << 7)

/* AHB2ENR bit for AES */
#define LL_AHB2_AES     (1UL << 16)

volatile uint32_t _aes_call_count __attribute__((used));

void BAES_Reset(void)
{
    ll_rcc_ahb2_clk_enable(LL_AHB2_AES);
    AES_CR_REG = 0;
}

void BAES_EcbCrypt(const uint8_t *key, const uint8_t *input,
                   uint8_t *output, int encrypt)
{
    (void)encrypt;
    _aes_call_count++;

    ll_rcc_ahb2_clk_enable(LL_AHB2_AES);

    /* Disable AES, reset CR */
    AES_CR_REG = 0;

    /* Load key in direct order (matches HW_AES_REV mode in ST hw_aes.c) */
    uint32_t k[4];
    memcpy(k, key, 16);
    AES_KEYR0_REG = k[0];
    AES_KEYR1_REG = k[1];
    AES_KEYR2_REG = k[2];
    AES_KEYR3_REG = k[3];

    /* Wait for KEYVALID */
    while (!(AES_SR_REG & AES_SR_KEYVALID))
        ;

    /* Enable AES: ECB encrypt, DATATYPE=00 (32-bit, no swap) */
    AES_CR_REG = AES_CR_EN;

    /* Load input with word-order swap (BAES_SWAP: [0,1,2,3] → [3,2,1,0]) */
    uint32_t in[4];
    memcpy(in, input, 16);
    AES_DINR_REG = in[3];
    AES_DINR_REG = in[2];
    AES_DINR_REG = in[1];
    AES_DINR_REG = in[0];

    /* Wait for CCF */
    while (!(AES_SR_REG & AES_SR_CCF))
        ;

    /* Read output and reverse word order (BAES_SWAP back) */
    uint32_t out[4];
    out[3] = AES_DOUTR_REG;
    out[2] = AES_DOUTR_REG;
    out[1] = AES_DOUTR_REG;
    out[0] = AES_DOUTR_REG;
    memcpy(output, out, 16);

    /* Disable AES (clears CCF) */
    AES_CR_REG = 0;
}

/* CMAC and CCM stubs — only needed for Secure Connections pairing */

void BAES_CmacSetKey(const uint8_t *key) { (void)key; }

void BAES_CmacCompute(const uint8_t *input, uint32_t input_length,
                      uint8_t *output_tag)
{
    (void)input; (void)input_length;
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
