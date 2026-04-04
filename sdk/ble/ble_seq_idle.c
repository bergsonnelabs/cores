/**
 * ble_seq_idle.c — Sequencer idle hooks for BLE
 *
 * Overrides weak defaults to ensure interrupts can fire.
 */

#include <stdint.h>
#include "stm32_seq.h"

/**
 * Called by UTIL_SEQ_Run when no tasks are pending.
 * Just return immediately — no WFI. This keeps the main loop
 * spinning fast so radio events are processed with minimum latency.
 * WFI was causing connection supervision timeouts (reason 0x3E).
 */
void UTIL_SEQ_Idle(void)
{
    /* No WFI — spin. Radio events need fast response for connections. */
}

/**
 * Called by UTIL_SEQ_WaitEvt while waiting for an event.
 * Runs the sequencer to process other tasks, then ensures
 * interrupts are briefly enabled between iterations.
 */
void UTIL_SEQ_EvtIdle(uint32_t TaskId_bm, uint32_t EvtWaited_bm)
{
    (void)EvtWaited_bm;

    /* Run other tasks (default behavior) */
    UTIL_SEQ_Run(~TaskId_bm);

    /* Briefly enable interrupts so pending ISRs can fire. */
    __asm volatile ("cpsie i" ::: "memory");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("cpsid i" ::: "memory");
}
