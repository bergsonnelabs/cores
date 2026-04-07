/**
 * h-tier1 — Tier 1 feature verification on Core.H
 *
 * Tests: backup registers, hardware RNG, RTC alarms.
 * Output: USB CDC serial (connect with `screen /dev/tty.usbmodem* 115200`)
 *
 * Expected output:
 *   [BACKUP] Write 0xCAFEBABE to register 0... read back: 0xCAFEBABE OK
 *   [BACKUP] Write 0xDEADBEEF to register 31... read back: 0xDEADBEEF OK
 *   [BACKUP] Boot count: N (incremented each reset)
 *   [RNG] Generating 8 random values:
 *   [RNG]   0x12345678 0xABCDEF01 ... (8 values, all different)
 *   [RNG] Error status: OK
 *   [RTC] Init (LSI)... set time to 12:00:00
 *   [RTC] Setting alarm for 12:00:05 (5 seconds from now)
 *   [RTC] Waiting for alarm... 12:00:01 ... 12:00:05 FIRED!
 *   [RTC] Alarm test: PASS
 *   [ADC] Temperature: XX.X °C
 *   [DONE] All tests complete.
 */

#include "core.h"
#include "core_usb.h"
#include "core_backup.h"
#include "core_rng.h"
#include "core_rtc.h"
#include "core_adc.h"

/* Wait for USB CDC to connect (DTR set by host terminal) */
static void wait_for_usb(void)
{
    while (!core_usb_connected()) {
        LED_TOGGLE();
        core_delay_ms(200);
    }
    LED_ON();
    core_delay_ms(500);  /* Let host terminal settle */
}

/* ---- Test: Backup Registers ---- */

static int test_backup(void)
{
    int pass = 1;

    /* Test register 0 */
    core_backup_write(0, 0xCAFEBABE);
    uint32_t v0 = core_backup_read(0);
    core_usb_printf("[BACKUP] Write 0xCAFEBABE to reg 0... read: 0x%08lX %s\r\n",
                    v0, v0 == 0xCAFEBABE ? "OK" : "FAIL");
    if (v0 != 0xCAFEBABE) pass = 0;

    /* Test register 31 (max on H5) */
    core_backup_write(31, 0xDEADBEEF);
    uint32_t v31 = core_backup_read(31);
    core_usb_printf("[BACKUP] Write 0xDEADBEEF to reg 31... read: 0x%08lX %s\r\n",
                    v31, v31 == 0xDEADBEEF ? "OK" : "FAIL");
    if (v31 != 0xDEADBEEF) pass = 0;

    /* Boot counter in register 2 (persists through resets) */
    uint32_t boots = core_backup_read(2) + 1;
    core_backup_write(2, boots);
    core_usb_printf("[BACKUP] Boot count: %lu\r\n", boots);

    return pass;
}

/* ---- Test: Hardware RNG ---- */

static int test_rng(void)
{
    core_rng_init();

    uint32_t vals[8];
    uint32_t filled = core_rng_fill(vals, 8);

    core_usb_printf("[RNG] Generated %lu values:\r\n[RNG]  ", filled);
    for (uint32_t i = 0; i < filled; i++) {
        core_usb_printf(" 0x%08lX", vals[i]);
    }
    core_usb_printf("\r\n");

    /* Check for errors */
    int err = core_rng_error();
    core_usb_printf("[RNG] Error status: %s\r\n", err ? "ERROR" : "OK");

    /* Verify values are different (basic entropy check) */
    int all_same = 1;
    for (uint32_t i = 1; i < filled; i++) {
        if (vals[i] != vals[0]) { all_same = 0; break; }
    }
    if (all_same && filled > 1) {
        core_usb_printf("[RNG] WARNING: all values identical — entropy failure?\r\n");
    }

    core_rng_deinit();
    return (filled == 8 && !err && !all_same) ? 1 : 0;
}

/* ---- Test: RTC Alarm A ---- */

static int test_rtc_alarm(void)
{
    core_rtc_init();

    /* Set time to 12:00:00 */
    core_rtc_set_time(12, 0, 0);
    core_usb_printf("[RTC] Init (LSI)... set time to 12:00:00\r\n");

    /* Set alarm for 12:00:05 (5 seconds from now) */
    core_rtc_set_alarm(12, 0, 5);
    core_usb_printf("[RTC] Alarm set for 12:00:05\r\n");

    /* Poll for alarm, printing time each second */
    int fired = 0;
    for (int i = 0; i < 10; i++) {
        core_delay_ms(1000);

        uint8_t h, m, s;
        core_rtc_get_time(&h, &m, &s);
        core_usb_printf("[RTC] %02d:%02d:%02d", h, m, s);

        if (core_rtc_alarm_fired()) {
            core_usb_printf(" — ALARM FIRED!\r\n");
            fired = 1;
            break;
        }
        core_usb_printf("\r\n");
    }

    core_rtc_clear_alarm();

    if (!fired) {
        core_usb_printf("[RTC] Alarm did not fire within 10 seconds\r\n");
    }

    return fired;
}

/* ---- Test: ADC Temperature (bonus, already verified) ---- */

static void test_adc_temp(void)
{
    core_adc_t adc;
    core_adc_init(&adc, ADC_12BIT);

    int32_t temp = core_adc_temp(&adc);
    core_usb_printf("[ADC] Temperature: %ld.%ld C\r\n", temp / 10, temp % 10);
}

/* ---- Main ---- */

int main(void)
{
    core_init();
    core_led_init();
    core_usb_init();

    wait_for_usb();

    core_usb_printf("\r\n=== Core.H Tier 1 Verification ===\r\n\r\n");

    int backup_ok = test_backup();
    core_usb_printf("\r\n");

    int rng_ok = test_rng();
    core_usb_printf("\r\n");

    int alarm_ok = test_rtc_alarm();
    core_usb_printf("\r\n");

    test_adc_temp();

    core_usb_printf("\r\n=== Results ===\r\n");
    core_usb_printf("  Backup registers: %s\r\n", backup_ok ? "PASS" : "FAIL");
    core_usb_printf("  Hardware RNG:     %s\r\n", rng_ok ? "PASS" : "FAIL");
    core_usb_printf("  RTC Alarm A:      %s\r\n", alarm_ok ? "PASS" : "FAIL");
    core_usb_printf("=== Done ===\r\n");

    /* Heartbeat */
    while (1) {
        LED_TOGGLE();
        core_delay_ms(1000);
    }
}
