/**
 * core_adc.h — Pad-level ADC reads
 *
 * The primary ADC API. All functions use tile pad numbers.
 * Channels, instances, and clocks are resolved automatically
 * from the project configuration.
 *
 * @tessera category adc label=Core.ADC icon=◐
 *
 * @tessera coverage
 *   id:    adc
 *   name:  ADC — analog-to-digital conversion
 *   page:  /docs/sdk/adc
 *   blurb: Pad-numbered ADC reads in raw counts and calibrated
 *          millivolts, plus internal channels (die temperature, VDD).
 *          Tier 2 dispatches to the coregen-emitted default instance
 *          (`core_adc1`); Tier 1 (the explicit-handle API) covers
 *          channel registration, sampling-speed control, and DMA
 *          continuous mode for advanced callers.
 */

#ifndef CORE_ADC_H
#define CORE_ADC_H

#include "tal_adc.h"

/** Core-level ADC handle. Alias for hal_adc_t — use this in application code. */
typedef hal_adc_t core_adc_t;

/** Default ADC instance — emitted by coregen into core_init.c when the
 * project declares any ADC pad. Today this is always `core_adc1`; multi-ADC
 * (Core.H) lands when tile JSON grows per-pad peripheral tagging. */
extern core_adc_t core_adc1;

/* ============================================================
 * Sampling speed aliases
 * ============================================================ */

#define SAMP_FAST      HAL_ADC_SAMP_FAST
#define SAMP_MED       HAL_ADC_SAMP_MED
#define SAMP_SLOW      HAL_ADC_SAMP_SLOW
#define SAMP_VERY_SLOW HAL_ADC_SAMP_VERY_SLOW

/* ============================================================
 * Resolution aliases
 * ============================================================ */

#define ADC_6BIT   HAL_ADC_RES_6BIT
#define ADC_8BIT   HAL_ADC_RES_8BIT
#define ADC_10BIT  HAL_ADC_RES_10BIT
#define ADC_12BIT  HAL_ADC_RES_12BIT

/* ============================================================
 * Init
 * ============================================================ */

/** Initialise the ADC at the given resolution. */
static inline hal_status_t core_adc_init(core_adc_t *adc, uint32_t resolution)
{
    return tal_adc_init(adc, (hal_adc_res_t)resolution);
}

/* ============================================================
 * Channel registration
 * ============================================================ */

/** Register a pad as an ADC input with the given sampling speed. */
static inline hal_status_t core_adc_add(core_adc_t *adc, uint8_t pad,
                                         uint32_t samp)
{
    return tal_adc_add_pad(adc, pad, (hal_adc_samp_t)samp);
}

/* ============================================================
 * Blocking reads
 * ============================================================ */

/** Single-shot read — returns raw ADC count. */
static inline uint16_t core_adc_read(core_adc_t *adc, uint8_t pad)
{
    return tal_adc_read_pad(adc, pad);
}

/** Single-shot read — returns calibrated millivolts. */
static inline uint32_t core_adc_read_mv(core_adc_t *adc, uint8_t pad)
{
    return tal_adc_read_pad_mv(adc, pad);
}

/* ============================================================
 * Pad-oriented wrappers (default-instance convention)
 * ============================================================
 *
 * These dispatch to the coregen-emitted default handle (`core_adc1`) so
 * the caller never has to pick an ADC peripheral. config.json drives the
 * handle into existence via `build_adc_config()` + `core_pads_init()`;
 * if no ADC pad is configured, calling these wrappers fails to link
 * — the correct outcome.
 */

/**
 * Read a pad as raw ADC counts (0–4095 at 12-bit resolution).
 *
 * @tessera expose category=adc name=read returns=int
 * @tessera twin noop
 * @param pad [1..64] Tile pad number configured as an ADC input in config.json.
 */
static inline int core_adc_read_pad(uint8_t pad)
{
    return (int)tal_adc_read_pad(&core_adc1, pad);
}

/**
 * Read a pad as calibrated millivolts (uses VREFINT for per-chip accuracy).
 *
 * @tessera expose category=adc name=read_mv returns=int
 * @tessera twin noop
 * @param pad [1..64] Tile pad number configured as an ADC input in config.json.
 */
static inline int core_adc_read_mv_pad(uint8_t pad)
{
    return (int)tal_adc_read_pad_mv(&core_adc1, pad);
}

/**
 * Die temperature in tenths of deg C (e.g. 253 = 25.3 C).
 * Dispatches to the default ADC instance.
 *
 * @tessera expose category=adc name=temp_decidegc returns=int
 * @tessera twin noop
 */
static inline int core_adc_temp_decidegc(void)
{
    return (int)tal_adc_read_temp_decidegc(&core_adc1);
}

/**
 * VDD supply voltage in millivolts (via VREFINT).
 * Dispatches to the default ADC instance.
 *
 * @tessera expose category=adc name=vdd_mv returns=int
 * @tessera twin noop
 */
static inline int core_adc_vdd_mv(void)
{
    return (int)tal_adc_read_vdda_mv(&core_adc1);
}

/* ============================================================
 * Internal channels
 * ============================================================ */

/** Die temperature in tenths of deg C (e.g. 253 = 25.3 C). */
static inline int32_t core_adc_temp(core_adc_t *adc)
{
    return tal_adc_read_temp_decidegc(adc);
}

/** Actual VDD supply voltage in millivolts via VREFINT. */
static inline uint32_t core_adc_vdd(core_adc_t *adc)
{
    return tal_adc_read_vdda_mv(adc);
}

/* ============================================================
 * DMA / continuous mode
 * ============================================================ */

/** Start continuous conversion with DMA circular buffer.
 *  @param cb   Called on half-transfer and transfer-complete; may be NULL
 *  @param ctx  User context passed to callback; may be NULL
 */
static inline hal_status_t core_adc_start_dma(core_adc_t *adc, uint16_t *buf,
                                               uint16_t len,
                                               hal_callback_t cb, void *ctx)
{
    return tal_adc_start_dma(adc, buf, len, cb, ctx);
}

/** Stop continuous DMA conversion. */
static inline void core_adc_stop_dma(core_adc_t *adc)
{
    tal_adc_stop_dma(adc);
}

/** Read most recent DMA result for a pad. */
static inline uint16_t core_adc_dma_read(core_adc_t *adc, uint8_t pad)
{
    return tal_adc_dma_read_pad(adc, pad);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=2 value=H title="Twin doesn't model analog inputs"
//   The simulator logs core_adc_read_pad / read_mv_pad / temp_decidegc /
//   vdd_mv host calls but always returns 0 (no named handler in
//   runtime.ts). DSL programs that branch on an analog read can't be
//   exercised end-to-end in the IDE — needs a per-pad input slider
//   wired into a tile-sim handler that returns the simulated count.
//
// @tessera unsupported tier=2 value=M title="No DSL access to sampling speed / resolution"
//   Tier 2 calls the default instance at its compile-time resolution
//   (12-bit) and per-pad sampling speed set during init. SAMP_FAST /
//   MED / SLOW / VERY_SLOW and ADC_6/8/10/12BIT aliases are reachable
//   only via the explicit-handle Tier 1 API.
//
// @tessera unsupported tier=2 value=M title="No multi-instance ADC dispatch"
//   Tier 2 wrappers always hit core_adc1. Cores with multiple ADCs
//   (Core.H has ADC1 + ADC2) need per-pad peripheral tagging in tile
//   JSON before coregen can emit the dispatch. Until then DSL programs
//   are limited to whichever pads coregen routed to ADC1.
//
// @tessera unsupported tier=1 value=M title="No oversampling / averaging helper"
//   STM32 ADCs can hardware-oversample 2×–256× for extra effective
//   resolution. The HAL exposes the registers but core_adc has no
//   convenience wrapper — callers reach into tal_adc / hal_adc to set
//   OVSR/OVSS bits manually.
//
// @tessera unsupported tier=1 value=L title="No external-trigger / injected channels"
//   ADC injected groups + external triggers (TIMx TRGO, EXTI line)
//   are not surfaced. Bring-your-own register writes if you need a
//   timer-synchronized analog sample for control loops.

#endif /* CORE_ADC_H */
