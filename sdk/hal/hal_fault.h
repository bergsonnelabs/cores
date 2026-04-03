/**
 * hal_fault.h — Fault handler with register dump
 *
 * Overrides the default HardFault, MemManage, BusFault, and UsageFault
 * handlers. On fault, dumps the stacked register frame over USB CDC
 * (polled, no interrupts needed) and blinks SOS on the LED.
 *
 * Usage:
 *   Just link hal_fault.c into your project — the handlers are
 *   defined with the correct symbol names and override the weak
 *   aliases in the startup assembly.
 *
 *   Optional: call hal_fault_set_callback() to add your own handler
 *   (e.g., log to flash, trigger a reset after delay).
 */

#ifndef HAL_FAULT_H
#define HAL_FAULT_H

#include <stdint.h>

/** Stacked register frame pushed by Cortex-M hardware on exception entry. */
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;    /* Link Register (return address before fault) */
    uint32_t pc;    /* Program Counter (instruction that faulted) */
    uint32_t psr;   /* Program Status Register */
} hal_fault_frame_t;

/** Fault type passed to the callback. */
typedef enum {
    HAL_FAULT_HARD = 0,
    HAL_FAULT_MEMMANAGE,
    HAL_FAULT_BUS,
    HAL_FAULT_USAGE,
} hal_fault_type_t;

/**
 * Optional user callback, called from fault context before SOS.
 * Keep it minimal — no heap, no interrupts, no blocking.
 */
typedef void (*hal_fault_callback_t)(hal_fault_type_t type, const hal_fault_frame_t *frame);

/**
 * Register a fault callback (optional).
 * Called before the register dump and SOS blink.
 */
void hal_fault_set_callback(hal_fault_callback_t cb);

#endif /* HAL_FAULT_H */
