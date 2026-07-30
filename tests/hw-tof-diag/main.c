/**
 * hw-tof-diag — why does Sense.TOF read 0 from Studio firmware but not from
 * a hand-written test?
 *
 * hw-tof-motor streams good distances. Studio-generated firmware, same board
 * and same driver calls, streams zeros. The two differ in exactly three ways:
 *
 *   hw-tof-motor (works)          Studio (zeros)
 *   ---------------------------   ------------------------------
 *   core_delay_ms(1500) pre-init  no settle delay before tile init
 *   if (tile_is_ready(&tof)) …    no readiness check at all
 *   no watchdog                   core_watchdog_start(5000)
 *
 * Hypothesis: tile_sense_tof_init()'s first step waits for the TMF8806
 * bootloader to reach sleep (ENABLE == 0x00). Straight out of core_init() the
 * sensor hasn't settled, that poll times out, and the tile lands in
 * TILE_STATE_ERROR. Studio never checks, so it happily calls start() and reads
 * a dead device — zeros, with nothing to say why. The 1.5 s delay in the
 * working test would have been masking this the whole time.
 *
 * This test runs BOTH sequences on the same boot and prints the difference:
 *
 *   Phase A — Studio's sequence: init immediately, no delay, no gate.
 *   Phase B — re-init after a settle delay, the hw-tof-motor way.
 *
 * If A reports not-ready and B reports ready, the bug is the missing settle
 * (and the missing check that would have surfaced it). If BOTH are ready and
 * both read 0, the sequence is innocent and the problem is elsewhere — the raw
 * register dump then says where.
 *
 * Raw registers are read through the HAL directly, NOT through the driver, so
 * the readings stand independent of any driver logic.
 */

#include "core.h"
#include "core_led.h"
#include "core_usb.h"
#include "core_watchdog.h"
#include "core_tiles.h"
#include "tile_sense_tof.h"

static tile_t tof;

/* Raw register reads — HAL-direct, bypassing the driver entirely. */
static uint8_t raw_reg(tiles_pal_t *hal, uint8_t reg)
{
    uint8_t v = 0;
    hal->i2c_read(hal->handle, TMF8806_I2C_ADDR, reg, &v, 1);
    return v;
}

static void dump_state(tiles_pal_t *hal, const char *tag)
{
    core_usb_printf("  [%s] ID=0x%02X ENABLE=0x%02X APPID=0x%02X INT=0x%02X\r\n",
                    tag,
                    raw_reg(hal, TMF8806_REG_ID),
                    raw_reg(hal, TMF8806_REG_ENABLE),
                    raw_reg(hal, TMF8806_REG_APPID),
                    raw_reg(hal, TMF8806_REG_INT_STATUS));
}

/* The 7-byte result block (0x1D..0x23), read raw. */
static void dump_result_block(tiles_pal_t *hal)
{
    uint8_t b[7] = {0};
    hal->i2c_read(hal->handle, TMF8806_I2C_ADDR, TMF8806_REG_STATUS, b, 7);
    core_usb_printf("  raw 0x1D..0x23: %02X %02X %02X %02X %02X %02X %02X"
                    "  -> dist=%u status=%u reliability=%u seq=%u\r\n",
                    b[0], b[1], b[2], b[3], b[4], b[5], b[6],
                    (unsigned)(((uint16_t)b[6] << 8) | b[5]),
                    b[0], (unsigned)(b[4] & 0x3F), b[3]);
}

int main(void)
{
    core_init();
    core_led_heartbeat(1000, 100);
    core_usb_init();
    core_delay_ms(6000);   /* CDC eats early prints; wide window so a host can attach */

    core_usb_printf("\r\n=== hw-tof-diag: Studio sequence vs settled sequence ===\r\n");

    tiles_pal_t *hal = core_tiles_pal(&core_i2c1);

    /* ---- Phase A: exactly what Studio generates ---- */
    core_usb_printf("A) init with NO settle delay (Studio's sequence)\r\n");
    dump_state(hal, "pre-init");
    tile_sense_tof_init(hal, 0, &tof, NULL);
    core_usb_printf("  tile_is_ready = %u   (0 => init failed; Studio never checks)\r\n",
                    tile_is_ready(&tof) ? 1u : 0u);
    dump_state(hal, "post-init");

    /* ---- Phase B: the hw-tof-motor sequence ---- */
    core_usb_printf("B) settle 1500 ms, then re-init\r\n");
    core_delay_ms(1500);
    tile_sense_tof_init(hal, 0, &tof, NULL);
    core_usb_printf("  tile_is_ready = %u\r\n", tile_is_ready(&tof) ? 1u : 0u);
    dump_state(hal, "post-init");

    if (tile_is_ready(&tof)) {
        tile_sense_tof_start(&tof);
        core_usb_printf("  started\r\n");
    } else {
        core_usb_printf("  NOT READY — starting anyway, like Studio does\r\n");
        tile_sense_tof_start(&tof);
    }

    /* Give the first measurement a chance (default period is 30 ms). */
    core_delay_ms(200);
    dump_state(hal, "post-start");

    core_usb_printf("--- streaming: ready / driver distance / raw block ---\r\n");
    for (;;) {
        core_watchdog_feed();   /* harmless if unarmed */
        core_usb_printf("ready=%u driver_mm=%u\r\n",
                        tile_sense_tof_result_ready(&tof) ? 1u : 0u,
                        tile_sense_tof_get_distance_mm(&tof));
        dump_result_block(hal);
        core_delay_ms(500);
    }
}
