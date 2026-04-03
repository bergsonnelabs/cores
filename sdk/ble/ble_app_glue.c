/**
 * ble_app_glue.c — Application-level callbacks and stubs for BLE stack
 *
 * Provides symbols that the BLE middleware and binary libs expect
 * the application to define. Will be extended in Phase B3/B4.
 */

#include <stdint.h>
#include <stddef.h>
#include "stm32_seq.h"
#include "stm32_mm.h"
#include "advanced_memory_manager.h"
#include "app_conf.h"
#include "svc_ctl.h"
#include "core_config.h"

/* ---- SystemCoreClock (CMSIS convention) ---- */

uint32_t SystemCoreClock = SYSCLK_HZ;

/* ---- AMM callbacks ---- */

static uint32_t amm_buffer[CFG_AMM_POOL_SIZE];

void AMM_RegisterBasicMemoryManager(AMM_BasicMemoryManagerFunctions_t *const p_fns)
{
    p_fns->Init(amm_buffer, sizeof(amm_buffer));
}

void AMM_ProcessRequest(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_AMM, CFG_SEQ_PRIO_0);
}

/* ---- SVCCTL callback ---- */

SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *pckt)
{
    (void)pckt;
    /* Will be implemented in B4 for event routing */
    return SVCCTL_UserEvtFlowEnable;
}

/* ---- Link layer debug signal stubs ---- */

void LINKLAYER_DEBUG_SIGNAL_SET(uint8_t signal)  { (void)signal; }
void LINKLAYER_DEBUG_SIGNAL_RESET(uint8_t signal) { (void)signal; }
void LINKLAYER_DEBUG_SIGNAL_TOGGLE(uint8_t signal) { (void)signal; }
