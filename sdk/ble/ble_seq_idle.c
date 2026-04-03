/**
 * ble_seq_idle.c — Sequencer idle hooks for BLE
 *
 * Overrides weak defaults to ensure interrupts can fire.
 * The link layer binary leaves irq_counter imbalanced during
 * HCI command processing, causing PRIMASK=1 deadlock.
 */

#include <stdint.h>
#include "stm32_seq.h"

/* Reset irq_counter imbalance from link layer binary */
extern volatile int32_t irq_counter;
extern volatile uint32_t primask_bit;

/**
 * Called by UTIL_SEQ_Run when no tasks are pending.
 * Must briefly enable interrupts so radio ISR can fire.
 */
void UTIL_SEQ_Idle(void)
{
    __asm volatile ("cpsie i" ::: "memory");
    __asm volatile ("wfi");
    __asm volatile ("cpsid i" ::: "memory");
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

    /* Briefly enable interrupts so pending ISRs can fire.
     * Without this, the radio interrupt never executes and
     * HCI command responses never arrive. */
    __asm volatile ("cpsie i" ::: "memory");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("cpsid i" ::: "memory");
}
