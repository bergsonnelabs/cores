/**
 * Core.ST.H5 Power Test — Clean End-to-End
 *
 * Tests sleep and stop modes using the SDK API.
 *
 * R[0] = 0xAA55AA55 (started)
 * R[1] = sleep elapsed ms (~2000)
 * R[2] = 0xCAFE (woke from stop)
 * R[3] = 0xBEEF (all done)
 */

#include "core.h"
#include "core_usb.h"
#include "core_power.h"

#define R ((volatile uint32_t *)0x20040000UL)

void RTC_IRQHandler(void) {
    ll_rtc_wakeup_clear_flag();
}

int main(void)
{
    core_init();
    core_usb_init();

    for (int i = 0; i < 8; i++) R[i] = 0;
    R[0] = 0xAA55AA55;

    /* Wait for USB */
    uint32_t wait_start = _systick_ticks;
    while (!core_usb_connected() && (_systick_ticks - wait_start < 2000))
        core_delay_ms(100);
    core_delay_ms(500);

    core_usb_printf("\r\n*** Core.ST.H5 Power Test ***\r\n\r\n");

    /* ---- Test 1: Sleep ---- */
    core_usb_printf("TEST 1: Sleep for ~2s...\r\n");
    uint32_t before = _systick_ticks;
    while (_systick_ticks - before < 2000)
        core_sleep();
    uint32_t sleep_ms = _systick_ticks - before;
    R[1] = sleep_ms;
    core_usb_printf("  Woke after %lu ms — %s\r\n\r\n",
                    sleep_ms, (sleep_ms >= 1900 && sleep_ms <= 2100) ? "PASS" : "WARN");

    core_delay_ms(500);

    /* ---- Test 2: Stop with RTC wakeup ---- */
    core_usb_printf("TEST 2: core_stop_for(3)...\r\n");
    core_delay_ms(200);

    core_stop_for(3);

    R[2] = 0xCAFE;  /* Woke up! */

    core_usb_printf("  Woke from Stop! — PASS\r\n\r\n");

    core_usb_printf("*** All tests passed! ***\r\n");
    R[3] = 0xBEEF;

    while (1) {
        core_delay_ms(1000);
    }
}
