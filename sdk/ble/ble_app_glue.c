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
#include "auto/ble_gap_aci.h"
#include "core_config.h"

/* ---- SystemCoreClock (CMSIS convention) ---- */

uint32_t SystemCoreClock = SYSCLK_HZ;

/* ---- Connection state (readable from core_ble.c) ---- */

volatile uint8_t  ble_connected;
volatile uint16_t ble_conn_handle;

/* User callbacks (set via core_ble API) */
void (*ble_on_connect_cb)(void);
void (*ble_on_disconnect_cb)(void);

/* ---- GAP command response release ---- */

static void gap_cmd_resp_release(void)
{
    UTIL_SEQ_SetEvt(1U << CFG_IDLEEVT_PROC_GAP_COMPLETE);
}

/* ---- SVCCTL_App_Notification ---- */

SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *p_Pckt)
{
    hci_event_pckt *p_event = (hci_event_pckt *)((hci_uart_pckt *)p_Pckt)->data;

    switch (p_event->evt)
    {
    case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
    {
        ble_connected = 0;
        ble_conn_handle = 0xFFFF;
        gap_cmd_resp_release();
        if (ble_on_disconnect_cb) ble_on_disconnect_cb();
        break;
    }

    case HCI_LE_META_EVT_CODE:
    {
        evt_le_meta_event *p_meta = (evt_le_meta_event *)p_event->data;

        switch (p_meta->subevent)
        {
        case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
        {
            hci_le_connection_complete_event_rp0 *p_conn =
                (hci_le_connection_complete_event_rp0 *)p_meta->data;
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
    {
        evt_blecore_aci *p_aci = (evt_blecore_aci *)p_event->data;
        switch (p_aci->ecode)
        {
        case 0x0401:  /* ACI_GAP_PAIRING_COMPLETE */
            /* Pairing finished — Just Works completes automatically */
            break;
        case 0x0405:  /* ACI_GAP_BOND_LOST */
            /* Bond lost — allow re-pairing */
            aci_gap_allow_rebond(ble_conn_handle);
            break;
        default:
            break;
        }
        break;
    }

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
