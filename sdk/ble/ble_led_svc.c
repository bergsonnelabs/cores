/**
 * ble_led_svc.c — Simple GATT service with a writable LED characteristic
 *
 * Service UUID:  0000AA00-8E22-4541-9D4C-21EDAE82ED19
 * Char UUID:     0000AA01-8E22-4541-9D4C-21EDAE82ED19
 *
 * Write 0x01 to turn LED on, 0x00 to turn LED off.
 * Read returns current LED state.
 */

#include <stdint.h>
#include "svc_ctl.h"
#include "ble_defs.h"
#include "ble_std.h"
#include "auto/ble_gatt_aci.h"
#include "auto/ble_vs_codes.h"
#include "auto/ble_types.h"

/* Service and characteristic handles */
static uint16_t led_svc_handle;
static uint16_t led_char_handle;

/* Current LED state */
static uint8_t led_state;

/* User callback for LED state changes */
static void (*led_write_cb)(uint8_t value);

/* UUID helper */
#define COPY_UUID_128(dst, b15,b14,b13,b12,b11,b10,b9,b8,b7,b6,b5,b4,b3,b2,b1,b0) \
    do { dst[0]=b0;dst[1]=b1;dst[2]=b2;dst[3]=b3;dst[4]=b4;dst[5]=b5; \
         dst[6]=b6;dst[7]=b7;dst[8]=b8;dst[9]=b9;dst[10]=b10;dst[11]=b11; \
         dst[12]=b12;dst[13]=b13;dst[14]=b14;dst[15]=b15; } while(0)

/* ---- Event handler for attribute writes ---- */

static SVCCTL_EvtAckStatus_t LED_EventHandler(void *p_Event)
{
    hci_event_pckt *p_evt = (hci_event_pckt *)((hci_uart_pckt *)p_Event)->data;

    if (p_evt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
        return SVCCTL_EvtNotAck;

    evt_blecore_aci *p_aci = (evt_blecore_aci *)p_evt->data;

    if (p_aci->ecode == ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE)
    {
        aci_gatt_attribute_modified_event_rp0 *p_mod =
            (aci_gatt_attribute_modified_event_rp0 *)p_aci->data;

        /* Check if this is our LED characteristic (value is at handle + 1) */
        if (p_mod->Attr_Handle == led_char_handle + 1)
        {
            if (p_mod->Attr_Data_Length >= 1)
            {
                led_state = p_mod->Attr_Data[0];
                if (led_write_cb) led_write_cb(led_state);
            }
            return SVCCTL_EvtAckFlowEnable;
        }
    }

    return SVCCTL_EvtNotAck;
}

/* ---- Public API ---- */

void ble_led_svc_init(void (*on_write)(uint8_t value))
{
    uint8_t uuid[16];
    tBleStatus ret;

    led_write_cb = on_write;

    /* Register event handler with service controller */
    SVCCTL_RegisterSvcHandler(LED_EventHandler);

    /* Add the LED service (1 service + 2 char attrs + 0 descriptors = 3) */
    COPY_UUID_128(uuid, 0x00,0x00,0xAA,0x00, 0x8E,0x22,0x45,0x41,
                        0x9D,0x4C,0x21,0xED, 0xAE,0x82,0xED,0x19);
    ret = aci_gatt_add_service(UUID_TYPE_128,
                               (Service_UUID_t *)uuid,
                               PRIMARY_SERVICE,
                               3,  /* max attribute records */
                               &led_svc_handle);
    (void)ret;

    /* Add the LED characteristic — readable + writable */
    COPY_UUID_128(uuid, 0x00,0x00,0xAA,0x01, 0x8E,0x22,0x45,0x41,
                        0x9D,0x4C,0x21,0xED, 0xAE,0x82,0xED,0x19);
    ret = aci_gatt_add_char(led_svc_handle,
                            UUID_TYPE_128,
                            (Char_UUID_t *)uuid,
                            1,                              /* value length */
                            CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
                            ATTR_PERMISSION_NONE,
                            GATT_NOTIFY_ATTRIBUTE_WRITE,    /* notify app on write */
                            10,                             /* encryption key size (unused) */
                            0,                              /* fixed length: no */
                            &led_char_handle);
    (void)ret;
}
