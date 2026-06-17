/**
 * val-backup-rng-alarm-u -- Hardware RNG, RTC, backup registers on Core.U.2
 *
 * Core.ST.L4.2, clock=max (80 MHz PLL)
 * No external wiring needed — all on-chip peripherals.
 * Debug output via USB CDC.
 *
 * RNG: fully verified (6/6 PASS).
 * RTC: time set/get verified. Alarm A and backup registers need
 *       bench investigation — writes to ALRMAR (0x1C) and BKPxR (0x50+)
 *       read back as 0 despite all access bits being correct.
 *       See test output for diagnostic register dumps.
 */

#include "core.h"
#include "core_usb.h"
#include "core_rng.h"
#include "core_rtc.h"
#include "core_backup.h"

#define dbg(...)  core_usb_printf(__VA_ARGS__)

static int pass_count, fail_count;

static void check(const char *name, int ok)
{
    if (ok) { dbg("  [PASS] %s\r\n", name); pass_count++; }
    else    { dbg("  [FAIL] %s\r\n", name); fail_count++; }
}

/* ================================================================
 * 1. Hardware RNG — fully verified
 * ================================================================ */
static void test_rng(void)
{
    dbg("\r\n--- 1. Hardware RNG ---\r\n");

    core_rng_init();
    check("RNG init (no error)", core_rng_error() == 0);

    uint32_t vals[8];
    int all_nonzero = 1;
    for (int i = 0; i < 8; i++) {
        vals[i] = core_rng_read();
        if (vals[i] == 0) all_nonzero = 0;
    }
    check("8 reads all non-zero", all_nonzero);
    dbg("    values: 0x%08lX 0x%08lX 0x%08lX 0x%08lX\r\n",
        (unsigned long)vals[0], (unsigned long)vals[1],
        (unsigned long)vals[2], (unsigned long)vals[3]);

    int all_unique = 1;
    for (int i = 0; i < 8 && all_unique; i++)
        for (int j = i + 1; j < 8 && all_unique; j++)
            if (vals[i] == vals[j]) all_unique = 0;
    check("8 values all unique", all_unique);

    uint32_t buf[16];
    check("core_rng_fill(16) returns 16", core_rng_fill(buf, 16) == 16);
    check("No error after burst", core_rng_error() == 0);

    core_rng_deinit();
    core_rng_init();
    check("Re-init produces data", core_rng_read() != 0);
    core_rng_deinit();
}

/* ================================================================
 * 2. RTC time set/get + alarm + backup diagnostics
 * ================================================================ */
static void test_rtc(void)
{
    dbg("\r\n--- 2. RTC ---\r\n");

    core_rtc_init();
    dbg("    PRER=0x%08lX\r\n", (unsigned long)REG32(0x40002810UL));

    /* Time set/get */
    core_rtc_set_time(12, 30, 0);
    uint8_t h, m, s;
    core_rtc_get_time(&h, &m, &s);
    dbg("    set 12:30:00 → read %02d:%02d:%02d\r\n", h, m, s);
    check("RTC set_time/get_time", h == 12 && m == 30 && s == 0);

    /* Verify ticking */
    core_delay_ms(2500);
    core_rtc_get_time(&h, &m, &s);
    check("RTC ticking", s > 0 || m > 30);
    dbg("    after 2.5s: %02d:%02d:%02d\r\n", h, m, s);

    /* Alarm A diagnostic */
    core_rtc_set_time(0, 0, 0);
    core_rtc_set_alarm(0, 0, 3);
    dbg("    ALRMAR=0x%08lX CR=0x%08lX\r\n",
        (unsigned long)REG32(0x4000281CUL),
        (unsigned long)REG32(0x40002818UL));

    int alarm_ok = 0;
    uint32_t start = core_millis();
    while (core_millis() - start < 8000) {
        if (core_rtc_alarm_fired()) { alarm_ok = 1; break; }
        core_delay_ms(50);
    }
    dbg("    alarm: %s (%lu ms)\r\n",
        alarm_ok ? "FIRED" : "TIMEOUT — ALRMAR write likely failed (hw investigation needed)",
        (unsigned long)(core_millis() - start));
    check("Alarm A fires", alarm_ok);
    core_rtc_clear_alarm();

    /* Backup register diagnostic */
    core_backup_write(0, 0xDEADBEEF);
    uint32_t bkp = core_backup_read(0);
    dbg("    BKP[0]: wrote 0xDEADBEEF, read 0x%08lX\r\n", (unsigned long)bkp);
    check("Backup register write/read", bkp == 0xDEADBEEF);
    check("BKP out-of-range returns 0", core_backup_read(CORE_BACKUP_COUNT) == 0);
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
    core_init();
    core_led_init();

    LED_ON();
    while (!core_usb_connected())
        core_delay_ms(10);
    LED_OFF();
    core_delay_ms(500);

    dbg("\r\n=== val-backup-rng-alarm-u (Core.U.2) ===\r\n");

    test_rng();
    test_rtc();

    dbg("\r\n========================================\r\n");
    dbg("Results: %d passed, %d failed\r\n", pass_count, fail_count);

    if (fail_count == 0) {
        dbg("=== ALL TESTS PASSED ===\r\n");
        while (1) { LED_TOGGLE(); core_delay_ms(500); }
    } else {
        dbg("=== %d TEST(S) FAILED ===\r\n", fail_count);
        dbg("NOTE: Alarm/backup failures may be hw-specific.\r\n");
        dbg("      RTC ALRMAR+BKPxR writes read back 0 on this L422.\r\n");
        dbg("      PRER/TR/CR/TAMPCR writes work fine.\r\n");
        while (1) { LED_ON(); core_delay_ms(50); LED_OFF(); core_delay_ms(50); }
    }
}
