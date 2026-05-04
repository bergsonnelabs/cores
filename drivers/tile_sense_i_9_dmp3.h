/*
 * tile_sense_i_9_dmp3.h — ICM-20948 DMP3 firmware blob.
 *
 * Source: TDK InvenSense eMD-SmartMotion-ICM20948-1.1.0-MP, distributed
 * by SparkFun in their ICM-20948 Arduino library (MIT-licensed wrapper).
 * The blob itself is TDK's IP, released for use with the ICM-20948 the
 * driver targets. Mirrored here so val + hw tests + production builds
 * can link DMP support without depending on the upstream Arduino package.
 *
 * Upstream:
 *   https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary
 *     src/util/icm20948_img.dmp3a.h (vendored from
 *     src/util/eMD-SmartMotion-ICM20948-1.1.0-MP/icm20948_img.dmp3a.h_14301)
 *
 * Layout: 14301 bytes loaded into the chip's DMP RAM starting at
 * DMP_LOAD_START (0x90 within bank 0). Bank crossings (0x100-byte
 * pages) are handled by the loader — see tile_sense_i_9_dmp_load().
 *
 * Do not edit. Regen by re-vendoring the upstream file.
 */
#ifndef TILE_SENSE_I_9_DMP3_H
#define TILE_SENSE_I_9_DMP3_H

#include <stdint.h>

/** DMP RAM address where the firmware loads. Per InvenSense eMD;
 *  the DMP processor reset vector points here. */
#define ICM20948_DMP_LOAD_START      0x90U

/** Total size of the firmware blob in bytes. */
#define ICM20948_DMP_FIRMWARE_SIZE   14301U

extern const uint8_t icm20948_dmp3_firmware[ICM20948_DMP_FIRMWARE_SIZE];

#endif /* TILE_SENSE_I_9_DMP3_H */
