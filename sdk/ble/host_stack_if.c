/**
 * host_stack_if.c — Host stack interface for BLE
 *
 * Adapted from ST's reference implementation.
 * Handles BLE host stack processing requests from the link layer.
 *
 * Original: STM32_WPAN/Target/host_stack_if.c
 * Copyright (c) 2025 STMicroelectronics. Licensed under ST terms.
 */

#include "host_stack_if.h"
#include "app_conf.h"
#include "ll_sys.h"
#include "auto/ble_raw_api.h"
#include "stm32_seq.h"

/* Missed HCI event flag — set by the BLE stack when an event is missed */
extern uint8_t missed_hci_event_flag;

/* Trigger BLE Host stack process after calling any aci/hci functions */
#define BLE_WRAP_POSTPROC BleStackCB_Process()

/**
 * Host stack processing request from Link Layer.
 */
void HostStack_Process(void)
{
    BleStackCB_Process();
}

/**
 * BLE Host stack processing callback.
 * Triggers the BLE host background task via the sequencer.
 */
void BleStackCB_Process(void)
{
    if (missed_hci_event_flag)
    {
        missed_hci_event_flag = 0;
        HCI_HARDWARE_ERROR_EVENT(0x03);
    }
    UTIL_SEQ_SetTask(1U << CFG_TASK_BLE_HOST, CFG_SEQ_PRIO_0);
}
