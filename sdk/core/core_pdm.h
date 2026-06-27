/**
 * core_pdm.h — PDM bitstream → PCM decimation
 *
 * A digital MEMS microphone (e.g. the SPG01 on the MIC tile) emits a 1-bit
 * sigma-delta PDM stream clocked at a high rate (e.g. 2.048 MHz). The SAI
 * captures that raw bitstream into RAM via DMA; this module turns it into
 * signed 16-bit PCM at an audio rate by low-pass filtering and downsampling.
 *
 * The filter is a CIC (cascaded integrator-comb) decimator — the classic,
 * multiply-free choice for PDM: N integrator stages at the bit rate, a /R
 * downsample, then N comb stages at the output rate, followed by a one-pole
 * DC blocker (a PDM idle pattern carries a large DC term). Integer-only.
 *
 *   output_rate = pdm_clock / R          (R = decimation factor)
 *   e.g. 2.048 MHz / 128 = 16 kHz voice; / 64 = 32 kHz.
 *
 * Streaming: feed it successive DMA half-buffers; filter state carries across
 * calls so block boundaries are seamless. Not interrupt-reentrant — run one
 * instance from one context (typically the SAI DMA half/complete callback).
 *
 * @studio coverage
 *   id:    pdm
 *   name:  PDM — microphone decimation
 *   blurb: Tier 1 helper. Converts a raw 1-bit PDM microphone bitstream
 *          (captured by the SAI) into signed-16 PCM via an integer CIC
 *          decimator + DC blocker. No DSL surface — audio capture is
 *          escape-to-C territory; the MIC tile driver wraps it.
 */
#ifndef CORE_PDM_H
#define CORE_PDM_H

#include <stdint.h>

/** Max CIC order supported (stack/state sizing). 4 is a good voice default. */
#define CORE_PDM_MAX_ORDER 6

/**
 * CIC decimator state. Zero-initialise via core_pdm_init(); do not poke
 * the fields directly. One instance per microphone stream.
 */
typedef struct {
    int32_t  integ[CORE_PDM_MAX_ORDER]; /**< integrator accumulators (bit rate) */
    int32_t  comb[CORE_PDM_MAX_ORDER];  /**< comb delay registers (output rate) */
    uint16_t decimation;                /**< R: input bits per output sample */
    uint16_t phase;                      /**< bits since last output (0..R-1) */
    uint8_t  order;                      /**< N: number of CIC stages */
    uint8_t  out_shift;                  /**< right-shift to map CIC gain into int16 */
    uint8_t  lsb_first;                  /**< 1 = take PDM bits LSB-first per byte */
    int32_t  dc_y;                       /**< DC-blocker output accumulator (Q8) */
    int32_t  dc_x1;                      /**< DC-blocker previous input */
} core_pdm_cic_t;

/**
 * @brief  Initialise a CIC decimator.
 *
 * @param  st          State to initialise (cleared).
 * @param  order       CIC order N (1..CORE_PDM_MAX_ORDER; 4 = good voice default).
 * @param  decimation  R = pdm_clock / output_rate (e.g. 128 for 2.048 MHz→16 kHz).
 * @param  gain_shift  Extra digital gain as a LEFT shift on the output (0 = unity
 *                     CIC scaling). Use to lift a quiet mic; clips are saturated.
 * @param  lsb_first   1 if the SAI packs PDM bits LSB-first within each byte,
 *                     0 for MSB-first. (Match your SAI capture config.)
 */
void core_pdm_init(core_pdm_cic_t *st, uint8_t order, uint16_t decimation,
                   uint8_t gain_shift, uint8_t lsb_first);

/**
 * @brief  Decimate a block of packed PDM bits into PCM samples.
 *
 * Processes every bit in `pdm` (8 bits/byte) and emits one int16 PCM sample
 * per `decimation` bits. State carries across calls, so streaming successive
 * DMA buffers is seamless.
 *
 * @param  st        Decimator state.
 * @param  pdm       Packed PDM bytes from the SAI DMA buffer.
 * @param  nbytes    Number of PDM bytes to process.
 * @param  out       Output PCM buffer; must hold at least
 *                   (phase + nbytes*8) / decimation samples.
 * @return number of PCM samples written to `out`.
 */
uint32_t core_pdm_process(core_pdm_cic_t *st, const uint8_t *pdm, uint32_t nbytes,
                          int16_t *out);

#endif /* CORE_PDM_H */
