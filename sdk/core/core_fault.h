/**
 * core_fault.h — Fault handler configuration
 *
 * The SDK automatically catches HardFault, MemManage, BusFault, and
 * UsageFault exceptions. On fault, registers are dumped over USB CDC
 * (if initialized) and the LED blinks SOS. No setup needed.
 *
 * Optionally register a callback to run custom logic before the dump.
 */

#ifndef CORE_FAULT_H
#define CORE_FAULT_H

#include "hal_fault.h"

/** Register a fault callback (called before register dump and SOS). */
static inline void core_fault_set_callback(hal_fault_callback_t cb)
{
    hal_fault_set_callback(cb);
}

#endif /* CORE_FAULT_H */
