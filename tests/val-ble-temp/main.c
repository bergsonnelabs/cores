/**
 * val-ble-temp -- Validation: BLE GATT services + ADC temperature
 *
 * Core.ST.W5, clock=default, BLE enabled
 * Pad 5 = ADC2
 *
 * Exercises: core_init, core_ble_set_services, core_ble_init,
 *            core_ble_advertise, core_ble_process, core_adc_t/init/read,
 *            core_ble_add_service, core_ble_add_char (6 args!),
 *            core_ble_set_value, core_ble_notify,
 *            core_ble_on_connect(cb, ctx), core_ble_on_disconnect(cb, ctx)
 *            write_cb signature: void(const uint8_t*, uint16_t, void *ctx)
 */

#include "core.h"
#include "core_ble.h"
#include "core_adc.h"

static core_ble_svc_t  svc;
static core_ble_char_t temp_char;
static core_ble_char_t ctrl_char;
static volatile uint8_t notify_enabled;

/* Write callback -- signature: void(const uint8_t *data, uint16_t len, void *ctx) */
static void on_ctrl_write(const uint8_t *data, uint16_t len, void *ctx)
{
    (void)ctx;
    if (len >= 1) {
        notify_enabled = data[0];
    }
}

static void on_connect(void *ctx)
{
    (void)ctx;
    notify_enabled = 1;
}

static void on_disconnect(void *ctx)
{
    (void)ctx;
    notify_enabled = 0;
}

static void build_services(void)
{
    svc = core_ble_add_service("Temperature");

    /* core_ble_add_char takes 6 args: svc, name, access, type, on_write, ctx */
    temp_char = core_ble_add_char(svc, "Temp",
                                  CORE_BLE_READ | CORE_BLE_NOTIFY,
                                  CORE_BLE_UINT16,
                                  NULL, NULL);

    ctrl_char = core_ble_add_char(svc, "Control",
                                  CORE_BLE_RW,
                                  CORE_BLE_UINT8,
                                  on_ctrl_write, NULL);
}

int main(void)
{
    core_init();

    /* ADC for pad 5 */
    core_adc_t adc;
    core_adc_init(&adc, ADC_12BIT);
    core_adc_add(&adc, 5, SAMP_SLOW);

    /* BLE lifecycle */
    core_ble_set_services(build_services);
    core_ble_init();

    /* Connection callbacks -- each takes (cb, ctx) */
    core_ble_on_connect(on_connect, NULL);
    core_ble_on_disconnect(on_disconnect, NULL);

    core_ble_advertise("ValTemp");

    while (1) {
        core_ble_process();

        if (core_ble_connected() && notify_enabled) {
            uint16_t raw = core_adc_read(&adc, 5);
            core_ble_set_value(temp_char, &raw, sizeof(raw));
            core_ble_notify(temp_char);
        }

        core_delay_ms(100);
    }
}
