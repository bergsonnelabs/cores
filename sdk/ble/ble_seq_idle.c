/**
 * ble_seq_idle.c — Sequencer idle hooks for BLE
 */

#include <stdint.h>
#include "stm32_seq.h"
#include "ll_rcc.h"

extern int ll_sys_dp_slp_exit(void);

/**
 * Called by UTIL_SEQ_Run when no tasks are pending.
 */
void UTIL_SEQ_Idle(void)
{
    /* Spin — no WFI. Keeps SWD accessible and minimizes radio latency. */
}

/**
 * Called before UTIL_SEQ_Idle.
 */
void UTIL_SEQ_PreIdle(void)
{
}

/**
 * Called after UTIL_SEQ_Idle (CPU woke up).
 * Must re-enable radio AHB5 clock and notify link layer.
 */
void UTIL_SEQ_PostIdle(void)
{
    /* Re-enable AHB5 radio clock (may have been gated during sleep) */
    ll_rcc_ahb5_clk_enable(LL_AHB5_RADIO);

    /* Notify link layer that deep sleep is over */
    ll_sys_dp_slp_exit();
}

/**
 * Called by UTIL_SEQ_WaitEvt while waiting for an event.
 */
void UTIL_SEQ_EvtIdle(uint32_t TaskId_bm, uint32_t EvtWaited_bm)
{
    (void)EvtWaited_bm;
    UTIL_SEQ_Run(~TaskId_bm);
}
