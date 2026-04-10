/**
 * val-sense-tof -- Compile-only validation for the Sense.TOF tile driver.
 *
 * Exercises every public API function in tile_sense_tof.h to verify
 * compilation. Does not require hardware -- all results are cast to void.
 *
 * Core.U.1 (Core-U-1-a), clock=max, I2C3 at 400 kHz
 */

#include "core.h"
#include "core_tiles.h"
#include "tile_sense_tof.h"

int main(void)
{
    core_init();

    tiles_pal_t *hal = core_tiles_pal(&core_i2c3);
    tile_t tof;

    /* ---- Lifecycle ---- */

    uint8_t found = tile_sense_tof_find(hal, 0);
    (void)found;

    /* Init with defaults */
    tile_sense_tof_init(hal, 0, &tof, NULL);

    /* Init with config */
    sense_tof_cfg_t cfg = {
        .mode       = SENSE_TOF_RANGE_2500MM,
        .period_ms  = 30,
        .kilo_iters = 900,
        .threshold  = 6,
    };
    tile_sense_tof_init(hal, 0, &tof, &cfg);

    /* ---- Power management ---- */

    tile_sense_tof_sleep(&tof);
    tile_sense_tof_wake(&tof);
    tile_sense_tof_reset(&tof);

    /* Re-init after reset */
    tile_sense_tof_init(hal, 0, &tof, NULL);

    /* ---- Measurement control ---- */

    tile_sense_tof_start(&tof);
    tile_sense_tof_stop(&tof);

    /* Single-shot measurement */
    sense_tof_result_t result;
    uint8_t ok = tile_sense_tof_measure_single(&tof, &result, 500);
    (void)ok;

    /* Single-shot with NULL result */
    ok = tile_sense_tof_measure_single(&tof, NULL, 500);
    (void)ok;

    /* ---- Result reading ---- */

    uint16_t dist = tile_sense_tof_get_distance_mm(&tof);
    (void)dist;

    sense_tof_result_t res;
    tile_sense_tof_get_result(&tof, &res);
    (void)res.distance_mm;
    (void)res.status;
    (void)res.reliability;
    (void)res.temperature;
    (void)res.result_number;

    uint8_t ready = tile_sense_tof_result_ready(&tof);
    (void)ready;

    /* ---- Calibration ---- */

    uint8_t cal_ok = tile_sense_tof_factory_calibrate(&tof, 5000);
    (void)cal_ok;

    uint8_t cal_data[TMF8806_CALIB_DATA_LEN];
    tile_sense_tof_get_calibration(&tof, cal_data);
    tile_sense_tof_set_calibration(&tof, cal_data);

    /* ---- Info ---- */

    sense_tof_version_t ver;
    tile_sense_tof_get_app_version(&tof, &ver);
    (void)ver.major;
    (void)ver.minor;
    (void)ver.patch;

    uint8_t serial[4];
    uint8_t serial_ok = tile_sense_tof_get_serial_number(&tof, serial);
    (void)serial_ok;

    /* ---- Runtime configuration ---- */

    tile_sense_tof_set_distance_mode(&tof, SENSE_TOF_SHORT_RANGE);
    tile_sense_tof_set_distance_mode(&tof, SENSE_TOF_RANGE_5000MM);
    tile_sense_tof_set_distance_mode(&tof, SENSE_TOF_RANGE_2500MM);
    tile_sense_tof_set_period(&tof, 0x00);
    tile_sense_tof_set_period(&tof, 0xFE);
    tile_sense_tof_set_period(&tof, 0x1E);

    /* ---- Algorithm state save/restore ---- */

    uint8_t alg_state[TMF8806_STATE_DATA_LEN];
    tile_sense_tof_save_state(&tof, alg_state);
    tile_sense_tof_restore_state(&tof, alg_state);

    /* ---- State checks ---- */

    uint8_t is_ready = tile_is_ready(&tof);
    (void)is_ready;

    tile_state_t state = tile_state(&tof);
    (void)state;

    while (1) {
        core_delay_ms(1000);
    }
}
