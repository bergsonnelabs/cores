/**
 * h-tier1 — Tier 1 feature verification on Core.H (SWD readback)
 *
 * Tests: backup registers, hardware RNG, RTC alarms.
 * Results written to SRAM at 0x20000000 for SWD readback.
 *
 * Readback:
 *   STM32_Programmer_CLI -c port=SWD mode=UR -r32 0x20000000 64
 *
 * Expected (all words in hex):
 *   [0] 0xRESULT01  — magic (test started)
 *   [1] backup test: 1=pass, 0=fail
 *   [2] backup readback reg 0
 *   [3] backup readback reg 31
 *   [4] backup boot count
 *   [5] RNG test: 1=pass, 0=fail
 *   [6] RNG value 0
 *   [7] RNG value 1
 *   [8] RNG value 2
 *   [9] RNG value 3
 *   [10] RNG values generated count
 *   [11] RNG error status (0=OK)
 *   [12] alarm test: 1=pass, 0=fail
 *   [13] RTC seconds at alarm fire (should be ~5)
 *   [14] ADC temperature (deci-degrees C)
 *   [15] 0xDONEDONE — magic (test complete)
 */

#include "core.h"
#include "core_backup.h"
#include "core_rng.h"
#include "core_rtc.h"
#include "core_adc.h"

/* Results array at top of SRAM — readable via SWD.
 * H5 SRAM1 ends at 0x20020000; place results at 0x2001FFC0 (16 words). */
#define RESULT_BASE  0x2001FFC0UL
#define R  ((volatile uint32_t *)RESULT_BASE)

int main(void)
{
    core_init();

    R[0] = 0xBE610001;  /* magic: test started */

    /* ---- Backup registers ---- */
    {
        core_backup_write(0, 0xCAFEBABE);
        uint32_t v0 = core_backup_read(0);

        core_backup_write(31, 0xDEADBEEF);
        uint32_t v31 = core_backup_read(31);

        /* Boot counter in register 2 */
        uint32_t boots = core_backup_read(2) + 1;
        core_backup_write(2, boots);

        R[1] = (v0 == 0xCAFEBABE && v31 == 0xDEADBEEF) ? 1 : 0;
        R[2] = v0;
        R[3] = v31;
        R[4] = boots;
    }

    /* ---- Hardware RNG ---- */
    {
        core_rng_init();

        uint32_t vals[4];
        uint32_t filled = core_rng_fill(vals, 4);
        int err = core_rng_error();

        /* Check values are different */
        int all_same = 1;
        for (uint32_t i = 1; i < filled; i++) {
            if (vals[i] != vals[0]) { all_same = 0; break; }
        }

        R[5] = (filled == 4 && !err && !all_same) ? 1 : 0;
        R[6] = (filled > 0) ? vals[0] : 0;
        R[7] = (filled > 1) ? vals[1] : 0;
        R[8] = (filled > 2) ? vals[2] : 0;
        R[9] = (filled > 3) ? vals[3] : 0;
        R[10] = filled;
        R[11] = (uint32_t)err;

        core_rng_deinit();
    }

    /* ---- RTC Alarm A ---- */
    {
        core_rtc_init();
        core_rtc_set_time(12, 0, 0);

        /* Alarm at 12:00:05 */
        core_rtc_set_alarm(12, 0, 5);

        int fired = 0;
        uint8_t h, m, s;
        for (int i = 0; i < 10; i++) {
            core_delay_ms(1000);
            if (core_rtc_alarm_fired()) {
                fired = 1;
                break;
            }
        }
        core_rtc_get_time(&h, &m, &s);
        core_rtc_clear_alarm();

        R[12] = fired ? 1 : 0;
        R[13] = (uint32_t)s;
    }

    /* ---- ADC Temperature ---- */
    {
        core_adc_t adc;
        core_adc_init(&adc, ADC_12BIT);
        int32_t temp = core_adc_temp(&adc);
        R[14] = (uint32_t)temp;
    }

    R[15] = 0xD04ED04E;  /* magic: test complete (DONEDONE) */

    /* Spin — results readable via SWD */
    while (1) {
        core_delay_ms(1000);
    }
}
