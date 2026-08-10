/**
 * @file   tile_sense_cam_p.h
 * @brief  Ultra-low-power global-shutter camera driver for Sense.CAM.P (rev a).
 *
 * Embeds the PixArt PAG7920J3, a 320 x 240 monochrome global-shutter camera
 * module with an integrated lens in a 2.94 x 2.44 x 2.11 mm cube. Registers
 * are configured over I2C; image data comes out over SPI. The tile does not
 * break out the 8-bit parallel bus, so SPI is the only image path.
 *
 * Key specifications:
 *   - Array:      320 x 240 monochrome, 3.0 um pixels, global shutter
 *   - Frame rate: up to 180 fps, 8-bit RAW
 *   - Optics:     71.34 deg H x 53.72 deg V, 64 dB dynamic range
 *   - Power:      0.76 mW @ QVGA 15 fps, 8.54 mW @ 180 fps, <100 uW motion detect
 *   - Supply:     VDDMA 3.14-3.45 V (<=50 mV ripple), VDDIO 1.14-3.45 V
 *
 * Datasheet: https://www.bergsonne.io/tiles/sense/cam-p
 * IC datasheet: PixArt PAG7920J3 (V0.8, L1033EN)
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   static uint8_t frame[160 * 120];
 *   tile_t cam;
 *
 *   sense_cam_p_cfg_t cfg = { .spi_cs = 9, .resolution = SENSE_CAM_P_RES_160x120 };
 *   tile_sense_cam_p_init(core_tiles_pal(&core_i2c1), 0, &cam, &cfg);
 *   if (tile_is_ready(&cam))
 *       tile_sense_cam_p_capture(&cam, frame, sizeof(frame));
 * @endcode
 *
 * @studio tile label=Sense.CAM.P icon=◉
 *
 * @par Three things that will bite anyone reimplementing this
 *
 * 1. The vendor init tables MUST be applied verbatim. They set R_TG_En at
 *    write 156 of section 8.1.3.4 and again at write 21 of 8.1.3.5, i.e. the
 *    sensor starts 24 writes before configuration ends. Deferring the start
 *    to tidy that up also relocates the undocumented 0xB0 = 0x01 (the final
 *    line of 8.1.3.5) from after the start to before it, which silently
 *    breaks the SPI mode switch. The part then accepts all 179 writes,
 *    reports R_TG_En = 1, passes a config read-back across all four banks,
 *    and never produces an image. Do not reorder these tables.
 *
 * 2. SPI register access requires R_TG_En = 1. CheckID reads 0x0000 whenever
 *    the sensor is stopped, so the SPI link must be brought up and verified
 *    while it is running, before any mode change.
 *
 * 3. Readout must be frame-locked. The output buffer is 4800 bytes and a
 *    frame is delivered as several of them; starting mid-frame yields an
 *    image rotated by whole buffers. Wait for Frame_Start (CPU_INT0_Status
 *    bit 1) before taking the first one.
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * @studio unsupported severity=common category="Motion detection"
 *   The PAG7920J3 has a proprietary on-chip motion-detection mode running at
 *   80x60 under 100 uW, which is the tile's headline always-on feature.
 *   Section 8.1.3.8 gives its init table. Driver-deferred, not a hardware
 *   gap, and the highest-value thing to add next.
 *
 * @studio unsupported severity=common category="Manual exposure and gain"
 *   Auto-exposure and auto-gain run by default and hold mean brightness
 *   constant. Section 9.3 defines manual control, which is needed for any
 *   machine-vision use wanting repeatable radiometry. Driver-deferred.
 *
 * @studio unsupported severity=advanced category="Trigger mode"
 *   Section 8.4 allows one-frame-on-demand capture, with frame rate set by
 *   the host's trigger interval. Note it does NOT relax the per-buffer
 *   readout deadline — the frame is still emitted in one output cycle at the
 *   sensor's own cadence. Driver-deferred.
 *
 * @studio unsupported severity=advanced category="Windowing, mirror and flip"
 *   Section 10 covers WOI, mirror/flip, skip and average. Only the fixed
 *   320x240 and 160x120 (2x skip) modes are exposed. Driver-deferred.
 *
 * @studio unsupported severity=niche category="LED strobe control"
 *   The sensor can drive an external LED in sync with exposure via GPIO1
 *   (R_expo_LED_* registers). The tile routes GPIO1 to a pad, so this is
 *   driver-deferred rather than hardware-gated.
 *
 * @studio unsupported severity=niche category="Multi-sensor frame sync"
 *   Section 8.7 supports synchronising several sensors to a shared frame
 *   clock. Driver-deferred.
 *
 * @studio unsupported severity=niche category="External clock input"
 *   Section 5.5 allows clocking the sensor from GPIO1 instead of its internal
 *   oscillator (1/6/12/24/30 MHz). Driver-deferred; the internal clock is
 *   used unconditionally.
 *
 * @studio unsupported severity=niche category="8-bit parallel image output"
 *   HARDWARE-GATED. The parallel bus needs PXD0-PXD7 plus PXCLK/HSYNC/VSYNC,
 *   and the tile's 10-pad T44 package breaks out only the four SPI signals.
 *   Not reachable on this tile at any driver revision.
 */

#ifndef INC_TILE_SENSE_CAM_P_H_
#define INC_TILE_SENSE_CAM_P_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_SENSE_CAM_P_VERSION_MAJOR  0
#define TILE_SENSE_CAM_P_VERSION_MINOR  1
#define TILE_SENSE_CAM_P_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Sense.CAM.P.
 *
 * | Instance | ID   | GPIO2 (pad 3) | Notes                        |
 * |----------|------|---------------|------------------------------|
 * | 0        | 0x35 | floating      | Tile default — leave unwired |
 * | 1        | 0x25 | tied to VDDIO |                              |
 * | 2        | 0x40 | tied to GND   |                              |
 *
 * The sensor answers at TWO addresses at once: the strapped unique ID above
 * plus the strap-independent broadcast ID 0x60. The strap is latched 30 ms
 * after power-on and cannot change afterwards, so driving pad 3 moves the
 * address. A software reset does NOT re-latch it.
 */
#define PAG7920_I2C_ADDR_FLOAT      0x35  /**< GPIO2 floating (tile default). */
#define PAG7920_I2C_ADDR_HIGH       0x25  /**< GPIO2 tied to VDDIO. */
#define PAG7920_I2C_ADDR_GND        0x40  /**< GPIO2 tied to GND. */
#define PAG7920_I2C_ADDR_BROADCAST  0x60  /**< Always live, strap-independent. */

/* -------------------------------------------------------------- */
/* Register map (I2C side, banked)                                 */
/* -------------------------------------------------------------- */

#define PAG7920_REG_BANK        0xEF  /**< Bank select, non-bank, WRITE-ONLY. */
#define PAG7920_BANK_0          0x00  /**< Banks 0/1/2/4 exist; 3 does not. */
#define PAG7920_REG_PARTID_L    0x00  /**< Bank 0: PartID[7:0]  = 0x20. */
#define PAG7920_REG_PARTID_H    0x01  /**< Bank 0: PartID[15:8] = 0x79. */
#define PAG7920_REG_TG_EN       0x30  /**< Bank 0: 1 = run continuous. */
#define PAG7920_REG_TRIGGER_EN  0x31  /**< Bank 0: 1 = trigger mode. */
#define PAG7920_REG_TRIGGER     0xEA  /**< Bank 0: write 1 to capture a frame. */
#define PAG7920_REG_SOFT_RESET  0xEE  /**< Bank 0: write 0xFF to reset. */
#define PAG7920_REG_UPDATE      0xEB  /**< Bank 0: write 0x80 to commit. */
#define PAG7920_PART_ID         0x7920 /**< Expected PartID. */

/* -------------------------------------------------------------- */
/* Register map (SPI side — only five are reachable, Table 29)     */
/* -------------------------------------------------------------- */

#define PAG7920_SPI_RD_BIT      0x80  /**< Bit 7 of byte 0: 1 = read. Addr is 7-bit. */
#define PAG7920_SPI_IMG_RD_EN   0x02  /**< 1 = grant access to the output buffer. */
#define PAG7920_SPI_INT_STATUS  0x03  /**< Interrupt flags, write-to-clear. */
#define PAG7920_SPI_CHECKID_L   0x06  /**< 0x5A */
#define PAG7920_SPI_CHECKID_H   0x07  /**< 0xA5 */
#define PAG7920_SPI_IMG_DATA    0x40  /**< Image burst source. */
#define PAG7920_SPI_CHECK_ID    0xA55A /**< Expected CheckID; 0 unless running. */

#define PAG7920_INT_FB_RDY      (1u << 0)  /**< Output buffer ready. */
#define PAG7920_INT_FRAME_START (1u << 1)  /**< New frame beginning. */
#define PAG7920_INT_FB_OVF      (1u << 2)  /**< Buffer overflowed; frame void. */

/** @brief  Output buffer size. A frame is delivered in units of this. */
#define PAG7920_BUFFER_BYTES    4800

/* -------------------------------------------------------------- */
/* Configuration                                                   */
/* -------------------------------------------------------------- */

/** @brief  Capture resolution. */
typedef enum {
    SENSE_CAM_P_RES_160x120 = 0,  /**< 19200 B/frame, analog 2x skip. */
    SENSE_CAM_P_RES_320x240 = 1,  /**< 76800 B/frame, full array. */
} sense_cam_p_res_t;

/**
 * @brief  Driver configuration.
 *
 * @note  spi_cs is the platform's chip-select identifier for the tile's
 *        SPI.CS pad (tile pad 8), passed straight to the PAL's SPI calls.
 *        It is separate from tile->id, which holds the I2C address.
 */
typedef struct {
    uint8_t           spi_cs;      /**< CS identifier for the PAL SPI calls. */
    sense_cam_p_res_t resolution;  /**< Capture resolution. */
} sense_cam_p_cfg_t;

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a PAG7920J3 is present on the I2C bus.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @return 1 if the device ACKs, 0 otherwise
 */
uint8_t tile_sense_cam_p_find(tiles_pal_t* hal, uint8_t instance);

/**
 * @brief  Initialize the camera for SPI image output.
 *
 * Verifies PartID, applies the vendor init tables verbatim for the requested
 * resolution, then confirms the SPI link by reading CheckID. Pass cfg=NULL
 * for defaults (spi_cs 0, 160x120).
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 *
 * @note   Blocks for ~250 ms. Requires both an I2C bus and an SPI bus on the
 *         PAL. Single-instance: the SPI CS and resolution are driver-global.
 */
void tile_sense_cam_p_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                           const sense_cam_p_cfg_t* cfg);

/**
 * @brief  Read the 16-bit part identifier over I2C.
 *
 * @studio expose category=tile name=part_id section=lifecycle
 * @return 0x7920 for a healthy PAG7920J3, 0 on a bus error.
 */
uint16_t tile_sense_cam_p_part_id(tile_t* tile);

/**
 * @brief  Read the SPI-side identifier, confirming the SPI link is usable.
 *
 * @studio expose category=tile name=spi_id section=lifecycle
 * @note   Returns 0x0000 whenever the sensor is stopped — the SPI register
 *         interface is only alive while R_TG_En = 1.
 * @return 0xA55A when the SPI link is up, 0 otherwise.
 */
uint16_t tile_sense_cam_p_spi_id(tile_t* tile);

/**
 * @brief  Frame width in pixels for the configured resolution.
 *
 * @studio expose category=tile name=width section=config
 * @return 160 or 320.
 */
uint16_t tile_sense_cam_p_width(tile_t* tile);

/**
 * @brief  Frame height in pixels for the configured resolution.
 *
 * @studio expose category=tile name=height section=config
 * @return 120 or 240.
 */
uint16_t tile_sense_cam_p_height(tile_t* tile);

/**
 * @brief  Number of bytes one frame occupies (width x height, 8-bit RAW).
 *
 * @studio expose category=tile name=frame_bytes section=config
 * @return 19200 or 76800.
 */
uint32_t tile_sense_cam_p_frame_bytes(tile_t* tile);

/**
 * @brief  Capture one frame into a caller-supplied buffer.
 *
 * Frame-locks on Frame_Start, then reads the frame in 4800-byte buffers.
 * The caller owns the memory: 19200 bytes at 160x120, 76800 at 320x240.
 *
 * Each buffer must be serviced within roughly 3.4 ms at default timing, or
 * FB_Ovf trips and the frame is void. Anything slow between buffers — a
 * per-byte print, a slow link — will tear the image across several exposures.
 *
 * @studio expose category=tile name=capture section=runtime returns=bool
 * @param  dst  Destination buffer.
 * @param  len  Size of dst; must be >= tile_sense_cam_p_frame_bytes().
 * @return 1 on a complete, non-overflowed frame; 0 otherwise.
 */
uint8_t tile_sense_cam_p_capture(tile_t* tile, uint8_t* dst, uint32_t len);

/**
 * @brief  Trigger a software reset (section 5.6.2) and return to defaults.
 *
 * Preferred over a power cycle for repeatable tests: the tile carries ~15.7 uF
 * of bulk and at the camera's microamp idle draw that rail can take seconds to
 * fall below the power-on-reset threshold, so an unplug/replug may not reset
 * the part at all.
 *
 * @studio expose category=tile name=reset section=lifecycle
 * @note   Leaves the sensor unconfigured; call init again afterwards.
 */
void tile_sense_cam_p_reset(tile_t* tile);

#endif /* INC_TILE_SENSE_CAM_P_H_ */
