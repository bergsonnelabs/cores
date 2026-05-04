/**
 * core_fault.h — Fault handler configuration
 *
 * The SDK automatically catches HardFault, MemManage, BusFault, and
 * UsageFault exceptions. On fault, registers are dumped over USB CDC
 * (if initialized) and the LED blinks SOS. No setup needed.
 *
 * Optionally register a callback to run custom logic before the dump.
 *
 * @tessera coverage
 *   id:    fault
 *   name:  Fault — exception handlers
 *   blurb: Tier 1 only. The SDK installs default HardFault / MemManage /
 *          BusFault / UsageFault handlers that dump registers over USB
 *          CDC and SOS-blink the LED. The single API here lets escape-
 *          to-C users hook in custom pre-dump logic (e.g., log to NVM
 *          before the reset). DSL programs aren't expected to handle
 *          their own faults.
 */

#ifndef CORE_FAULT_H
#define CORE_FAULT_H

#include "hal_fault.h"

/** Register a fault callback (called before register dump and SOS). */
static inline void core_fault_set_callback(hal_fault_callback_t cb)
{
    hal_fault_set_callback(cb);
}

/* ---- Coverage gaps (consumed by the SDK Coverage Table) ---- */

// @tessera unsupported tier=1 value=M title="No structured fault report capture"
//   The default handler dumps to USB CDC at runtime — there's no
//   in-flash crash log that survives reset for post-mortem analysis.
//   A small ring buffer in backup or NVM (with the captured PC / LR
//   / xPSR / fault status registers) would make field debugging
//   tractable.

#endif /* CORE_FAULT_H */
