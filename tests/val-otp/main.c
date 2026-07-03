/**
 * val-otp -- Validation: OTP identity storage on Core.ST.W5
 *
 * Core.ST.W5 (STM32WBA55), clock=max
 *
 * Exercises: core_init, core_otp_size, core_otp_slot_count,
 *            core_otp_slot_is_blank, core_otp_read  (NON-DESTRUCTIVE by default).
 *
 * WBA55 has 512 bytes of OTP at 0x0BF90000 in 16-byte quad-word slots. OTP is
 * WRITE-ONCE and IRREVERSIBLE, so this test only READS by default: it reports
 * the geometry, whether slot 0 is blank, and reads slot 0 back — then toggles
 * the LED if the geometry looks right.
 *
 * The program path (core_otp_program_slot) is compiled out behind
 * VAL_OTP_ALLOW_PROGRAM. Enabling it PERMANENTLY burns VAL_OTP_SCRATCH_SLOT on
 * the attached board -- only ever do this on a dedicated throwaway unit, never
 * a keeper.
 */

#include "core.h"
#include "core_otp.h"

#define VAL_OTP_ALLOW_PROGRAM  0    /* 1 = DESTRUCTIVE: burns VAL_OTP_SCRATCH_SLOT */
#define VAL_OTP_SCRATCH_SLOT   31u

int main(void)
{
    core_init();
    core_led_init();

    uint32_t sz    = core_otp_size();        /* 512 on W5 */
    uint32_t slots = core_otp_slot_count();  /* 32  on W5 */
    int slot0_blank = core_otp_slot_is_blank(0);

    /* Read slot 0 back (memory-mapped; always safe even on virgin OTP). */
    uint8_t buf[CORE_OTP_SLOT_SIZE] = { 0 };
    int rd = core_otp_read(0, buf, sizeof(buf));

#if VAL_OTP_ALLOW_PROGRAM
    /* DESTRUCTIVE -- burns one scratch slot with a sample record, then the
     * driver confirms the read-back. Guarded off by default; see the header
     * warning. Only run on a throwaway board. */
    if (core_otp_slot_is_blank(VAL_OTP_SCRATCH_SLOT) == 1) {
        const uint8_t rec[CORE_OTP_SLOT_SIZE] = {
            0x52, 0x4F, 0x54, 0x50,   /* "ROTP" magic          */
            0x01,                     /* format                */
            0x02,                     /* hw_variant            */
            0x00,                     /* actuator (piezo)      */
            0x0C,                     /* piezo_size (0x0C=1204)*/
            0x03, 0x00,               /* caps                  */
            0x00, 0x00, 0x00, 0x00,   /* reserved              */
            0x00, 0x00                /* crc16 (placeholder)   */
        };
        (void)core_otp_program_slot(VAL_OTP_SCRATCH_SLOT, rec);
    }
#endif

    /* Sanity: geometry present and read succeeded -> toggle LED. */
    if (sz == 512u && slots == 32u && rd == 0 && slot0_blank >= 0) {
        LED_TOGGLE();
    }

    while (1) {
        core_delay_ms(1000);
    }
}
