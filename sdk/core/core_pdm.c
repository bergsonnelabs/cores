/**
 * core_pdm.c — PDM bitstream → PCM decimation (CIC + DC blocker).
 *
 * CIC relies on two's-complement wraparound in the integrator/comb registers:
 * correct as long as the 32-bit word width covers the filter's bit growth,
 * order * ceil(log2(R)) <= ~30. core_pdm_init enforces sane order/R.
 */
#include "core_pdm.h"

/* ceil(log2(r)) for r >= 1 (0 for r<=1). */
static uint8_t ceil_log2(uint16_t r)
{
    if (r <= 1) return 0;
    uint8_t n = 0;
    uint16_t v = (uint16_t)(r - 1);
    while (v) { v >>= 1; n++; }
    return n;
}

static int16_t sat16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

void core_pdm_init(core_pdm_cic_t *st, uint8_t order, uint16_t decimation,
                   uint8_t gain_shift, uint8_t lsb_first)
{
    for (int i = 0; i < CORE_PDM_MAX_ORDER; i++) { st->integ[i] = 0; st->comb[i] = 0; }
    if (order < 1) order = 1;
    if (order > CORE_PDM_MAX_ORDER) order = CORE_PDM_MAX_ORDER;
    if (decimation < 1) decimation = 1;

    st->order      = order;
    st->decimation = decimation;
    st->phase      = 0;
    st->lsb_first  = lsb_first ? 1u : 0u;
    st->dc_y       = 0;
    st->dc_x1      = 0;

    /* CIC DC gain is R^N; shift it down toward int16 full-scale, then apply
     * the caller's extra digital gain (a smaller right-shift). Clamp >= 0. */
    int growth = (int)order * (int)ceil_log2(decimation);
    int shift  = growth - 15 - (int)gain_shift;
    if (shift < 0) shift = 0;
    st->out_shift = (uint8_t)shift;
}

uint32_t core_pdm_process(core_pdm_cic_t *st, const uint8_t *pdm, uint32_t nbytes,
                          int16_t *out)
{
    const uint8_t  N     = st->order;
    const uint8_t  shift = st->out_shift;
    const uint16_t R     = st->decimation;
    uint32_t n = 0;

    for (uint32_t bidx = 0; bidx < nbytes; bidx++) {
        uint8_t byte = pdm[bidx];
        for (uint8_t k = 0; k < 8; k++) {
            uint8_t b = st->lsb_first ? (uint8_t)((byte >> k) & 1u)
                                      : (uint8_t)((byte >> (7 - k)) & 1u);
            int32_t x = b ? 1 : -1;   /* 1-bit density → +/-1 */

            /* Integrator cascade — every input bit. */
            int32_t v = x;
            for (uint8_t i = 0; i < N; i++) { st->integ[i] += v; v = st->integ[i]; }

            if (++st->phase >= R) {
                st->phase = 0;
                /* Comb cascade — at the output rate. */
                int32_t c = v;
                for (uint8_t i = 0; i < N; i++) {
                    int32_t t = c - st->comb[i];
                    st->comb[i] = c;
                    c = t;
                }
                int32_t s = c >> shift;

                /* One-pole DC blocker: y[n] = (x-x[n-1]) + a*y[n-1], a = 255/256.
                 * y kept in Q8 (dc_y). Removes the PDM idle DC term. */
                int32_t Y = (int32_t)((s - st->dc_x1) << 8) + st->dc_y - (st->dc_y >> 8);
                st->dc_y  = Y;
                st->dc_x1 = s;
                out[n++]  = sat16(Y >> 8);
            }
        }
    }
    return n;
}
