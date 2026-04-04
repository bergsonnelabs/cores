/**
 * ble_app_glue.c — Application-level callbacks for BLE stack
 *
 * Handles HCI events (connection, disconnection, PHY updates),
 * AMM callbacks, and debug signal stubs.
 */

#include <stdint.h>
#include <stddef.h>
#include "stm32_seq.h"
#include "stm32_mm.h"
#include "advanced_memory_manager.h"
#include "app_conf.h"
#include "svc_ctl.h"
#include "ble_std.h"
#include "auto/ble_types.h"
#include "auto/ble_l2cap_aci.h"
#include "core_config.h"

/* ---- SystemCoreClock (CMSIS convention) ---- */

uint32_t SystemCoreClock = SYSCLK_HZ;

/* ---- Connection state (readable from core_ble.c) ---- */

volatile uint8_t  ble_connected;
volatile uint16_t ble_conn_handle;

/* User callbacks (set via core_ble API) */
void (*ble_on_connect_cb)(void);
void (*ble_on_disconnect_cb)(void);

/* Debug log — readable via CubeProgrammer */
#define BLE_EVT_LOG_SIZE 32
volatile uint32_t ble_evt_log[BLE_EVT_LOG_SIZE] __attribute__((used));
volatile uint32_t ble_evt_log_idx;

/* ---- GAP command response release ---- */

static void gap_cmd_resp_release(void)
{
    UTIL_SEQ_SetEvt(1U << CFG_IDLEEVT_PROC_GAP_COMPLETE);
}

/* ---- SVCCTL_App_Notification ---- */

static void evt_log(uint32_t val)
{
    uint32_t idx = ble_evt_log_idx;
    if (idx < BLE_EVT_LOG_SIZE) {
        ble_evt_log[idx] = val;
        ble_evt_log_idx = idx + 1;
    }
}

SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *p_Pckt)
{
    hci_event_pckt *p_event = (hci_event_pckt *)((hci_uart_pckt *)p_Pckt)->data;

    /* Log every event code that arrives */
    evt_log(0xE0000000 | p_event->evt);

    switch (p_event->evt)
    {
    case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
    {
        hci_disconnection_complete_event_rp0 *p_dc =
            (hci_disconnection_complete_event_rp0 *)p_event->data;
        evt_log(0xDC000000 | (p_dc->Reason << 8) | p_dc->Connection_Handle);
        ble_connected = 0;
        ble_conn_handle = 0xFFFF;
        gap_cmd_resp_release();
        if (ble_on_disconnect_cb) ble_on_disconnect_cb();
        break;
    }

    case HCI_LE_META_EVT_CODE:
    {
        evt_le_meta_event *p_meta = (evt_le_meta_event *)p_event->data;
        evt_log(0xAE000000 | p_meta->subevent);

        switch (p_meta->subevent)
        {
        case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
        {
            hci_le_connection_complete_event_rp0 *p_conn =
                (hci_le_connection_complete_event_rp0 *)p_meta->data;
            evt_log(0xCC000000 | (p_conn->Status << 8) | p_conn->Connection_Handle);
            if (p_conn->Status == 0) {
                ble_connected = 1;
                ble_conn_handle = p_conn->Connection_Handle;
                if (ble_on_connect_cb) ble_on_connect_cb();
            }
            break;
        }

        case HCI_LE_ENHANCED_CONNECTION_COMPLETE_SUBEVT_CODE:
        {
            hci_le_enhanced_connection_complete_event_rp0 *p_conn =
                (hci_le_enhanced_connection_complete_event_rp0 *)p_meta->data;
            evt_log(0xEC000000 | (p_conn->Status << 8) | p_conn->Connection_Handle);
            if (p_conn->Status == 0) {
                ble_connected = 1;
                ble_conn_handle = p_conn->Connection_Handle;
                if (ble_on_connect_cb) ble_on_connect_cb();
            }
            break;
        }

        case HCI_LE_PHY_UPDATE_COMPLETE_SUBEVT_CODE:
            gap_cmd_resp_release();
            break;

        case HCI_LE_CONNECTION_UPDATE_COMPLETE_SUBEVT_CODE:
            break;

        default:
            break;
        }
        break;
    }

    case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
        break;

    default:
        break;
    }

    return SVCCTL_UserEvtFlowEnable;
}

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

/* ---- Link layer debug signal stubs ---- */

void LINKLAYER_DEBUG_SIGNAL_SET(uint8_t signal)  { (void)signal; }
void LINKLAYER_DEBUG_SIGNAL_RESET(uint8_t signal) { (void)signal; }
void LINKLAYER_DEBUG_SIGNAL_TOGGLE(uint8_t signal) { (void)signal; }
