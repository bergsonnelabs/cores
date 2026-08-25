/**
 * ble-contract — the GATT is GENERATED from the `ble.contract` block in
 * config.json. There is no hand-written build_services() here; coregen emits
 * ble_contract.{h,c} and core_init() registers the builder.
 *
 * Covers: SIG + custom services, scalar + byte characteristics, a writable
 * characteristic whose weak handler is overridden below, and a trailing
 * service carrying a `note`.
 *
 * Compile/link validation. Flash it and it advertises as BLE_DEVICE_NAME with
 * the four characteristics, each named via its 0x2901 descriptor.
 */
#include "core.h"
#include "core_ble.h"
#include "ble_contract.h"

static uint8_t s_status[7];

/* Overrides the generated weak no-op — same symbol, different TU.
 *
 * Brightness is declared uint16 in the contract, so coregen decodes the
 * payload at that width and hands over the value as an int (the width a
 * Studio DSL handler can express); a short write is dropped before it gets
 * here. Command is `bytes`, which has no declared width, so it still takes the
 * raw buffer. Both shapes are exercised on purpose. */
void ble_brightness_on_write(int value)
{
    /* Read + write, so publish the accepted value straight back: a client that
     * reads after writing sees what actually took effect. */
    ble_brightness_set(value);
}

void ble_command_on_write(const uint8_t *data, uint16_t len)
{
    if (len >= 1) {
        s_status[0] = 1;         /* format version */
        s_status[1] = data[0];   /* echo the opcode */
        ble_status_set(s_status, sizeof s_status);
    }
}

int main(void)
{
    core_init();                 /* registers ble_contract_build() */
    core_led_init();
    core_ble_init();
    core_ble_advertise(BLE_DEVICE_NAME);   /* generated from ble.name */

    uint32_t uptime = 0;
    while (1) {
        core_ble_process();

        ble_uptime_set(uptime++);
        ble_battery_level_set((uint8_t)(uptime & 0x7Fu));
        ble_level_set((uint16_t)(uptime * 3u));

        LED_ON();
        core_delay_ms(2);
        LED_OFF();
        core_delay_ms(248);
    }
}
