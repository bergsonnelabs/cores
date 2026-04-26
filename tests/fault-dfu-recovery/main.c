/**
 * fault-dfu-recovery — Test that a hard fault reboots into DFU
 *
 * Procedure:
 *   1. Flash via: make flash-dfu
 *   2. Open a serial monitor (screen /dev/tty.usbmodem* 115200)
 *   3. Wait ~3 seconds — the RGBW LED blinks green twice, then the
 *      firmware deliberately dereferences a null pointer
 *   4. Expected: LED flashes red (fault handler SOS), then the board
 *      reboots into DFU (USB re-enumerates as 0483:DF11)
 *   5. Verify recovery: run make flash-dfu again — it should succeed
 */

#include "core.h"
#include "core_usb.h"
#include "tile_handles.h"

/* Volatile so the compiler doesn't optimize away the null deref */
static volatile uint32_t *bad_ptr = (volatile uint32_t *)0x00000000UL;

static void delay(volatile int count)
{
    while (count-- > 0);
}

int main(void)
{
    core_init();
    core_usb_init();

    /* Init the RGBW LED on I2C1 */
    tile_display_rgbw_init(core_tiles_pal(&core_i2c1), 0, &tile_display_rgbw_i2c1_0, NULL);

    /* Give USB time to enumerate so the serial monitor can connect */
    delay(2000000);

    core_usb_printf("fault-dfu-recovery: starting\r\n");
    core_usb_printf("Blinking green twice, then crashing...\r\n");

    /* Blink green twice */
    for (int i = 0; i < 2; i++) {
        tile_display_rgbw_set(&tile_display_rgbw_i2c1_0, 0, 64, 0, 0);
        delay(300000);
        tile_display_rgbw_off(&tile_display_rgbw_i2c1_0);
        delay(300000);
    }

    /* Brief pause so the message gets through */
    delay(500000);

    core_usb_printf("Triggering null pointer dereference NOW\r\n");

    /* This write to address 0 triggers a HardFault on Cortex-M4.
     * The fault handler should blink SOS once, then reboot into DFU. */
    *bad_ptr = 0xDEAD;

    /* Should never reach here */
    while (1);
}
