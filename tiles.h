/**
 * @file   tiles.h
 * @brief  Tile driver framework — the only include users need.
 *
 * Provides the tile instance handle (tile_t), lifecycle states,
 * error reporting, and convenience functions. Includes tiles_pal.h
 * automatically, so users get the full framework with one include.
 *
 * Header-only — no companion .c file required.
 *
 * Usage:
 * @code
 *   #include "tiles.h"
 *   #include "tile_sense_i_9.h"
 *
 *   tiles_pal_stm32_cfg_t cfg = { .i2c = &hi2c1, .buses = TILES_BUS_I2C };
 *   tiles_pal_t hal;
 *   tiles_pal_stm32_init(&hal, &cfg);
 *   hal.on_error = my_error_handler;  // optional
 *
 *   tile_t imu;
 *   tile_sense_i_9_init(&hal, 0, &imu);
 *   if (tile_is_ready(&imu)) {
 *       tile_sense_i_9_get_raw_accels(&imu, accel);
 *   }
 * @endcode
 */

#ifndef INC_TILES_H_
#define INC_TILES_H_

#include "tiles_pal.h"

/* ------------------------------------------------------------------ */
/*  Library version                                                   */
/* ------------------------------------------------------------------ */

#define TILES_VERSION_MAJOR  1
#define TILES_VERSION_MINOR  0
#define TILES_VERSION_PATCH  0
#define TILES_VERSION_STRING "1.0.0"

/**
 * @brief  Assert at compile time that tiles.h meets a minimum version.
 *
 * Place this near the top of every driver header, after includes:
 * @code
 *   #include "tiles.h"
 *   TILES_CHECK_VERSION(1, 0);
 * @endcode
 *
 * @param  req_major  Minimum required major version
 * @param  req_minor  Minimum required minor version
 */
#define TILES_CHECK_VERSION(req_major, req_minor) \
    _Static_assert( \
        (TILES_VERSION_MAJOR > (req_major)) || \
        (TILES_VERSION_MAJOR == (req_major) && \
         TILES_VERSION_MINOR >= (req_minor)), \
        "tiles.h version too old for this driver")

/* ------------------------------------------------------------------ */
/*  Lifecycle states                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief  Tile lifecycle states.
 */
typedef enum {
    TILE_STATE_NONE     = 0,   /**< Uninitialized                         */
    TILE_STATE_FOUND    = 1,   /**< ACKs on bus, not yet configured        */
    TILE_STATE_READY    = 2,   /**< Fully initialized and operational      */
    TILE_STATE_SLEEPING = 3,   /**< Low-power / standby mode               */
    TILE_STATE_ERROR    = 4,   /**< Fault detected (ID mismatch, bus err)  */
} tile_state_t;

/* ------------------------------------------------------------------ */
/*  Callback type                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief  User callback — runs from tiles_process() in main-loop context.
 *
 * @param  ctx  User-supplied context pointer (passed at registration)
 */
typedef void (*tile_callback_fn)(void* ctx);

/* ------------------------------------------------------------------ */
/*  Event flags                                                       */
/* ------------------------------------------------------------------ */

#define TILE_FLAG_DATA_READY   (1 << 0)  /**< Sensor has new data         */
#define TILE_FLAG_DMA_COMPLETE (1 << 1)  /**< Async transfer finished     */
#define TILE_FLAG_ERROR        (1 << 2)  /**< Fault detected              */

/* ------------------------------------------------------------------ */
/*  Instance handle                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief  Tile instance handle.
 *
 * Populated by each driver's init() function. Pass a pointer to all
 * subsequent driver calls. Declare as a global or long-lived local.
 *
 * @code
 *   tile_t imu;
 *   tile_sense_i_6p_init(&hal, 0, &imu);
 * @endcode
 */
typedef struct tile {
    tiles_pal_t*     hal;       /**< Platform HAL pointer                       */
    uint8_t          id;        /**< Device identifier (I2C addr, SPI CS, etc.) */
    tile_state_t     state;     /**< Current lifecycle state                    */
    uint8_t          flags;     /**< Event flags — set by ISRs, cleared by
                                     tiles_process(). Reserved for future use.  */
    tile_callback_fn callback;  /**< User callback — dispatched by
                                     tiles_process(). Reserved for future use.  */
    void*            cb_ctx;    /**< User context for callback.
                                     Reserved for future use.                   */
} tile_t;

/* ------------------------------------------------------------------ */
/*  Error reporting                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief  Error reporting macro used inside drivers.
 *
 * Routes errors through the HAL's on_error callback. If the callback
 * is NULL (the default), this is a no-op — silent and zero-cost.
 * Set hal.on_error to your preferred transport (printf, UART, BLE, etc.).
 *
 * For zero-overhead builds on tight flash budgets, define TILE_ON_ERROR
 * to ((void)0) before including this header to eliminate the check entirely.
 */
#ifndef TILE_ON_ERROR
#define TILE_ON_ERROR(tile, msg) \
    do { if ((tile)->hal && (tile)->hal->on_error) \
             (tile)->hal->on_error(*(tile), (msg)); } while(0)
#endif

/* ------------------------------------------------------------------ */
/*  Convenience functions                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief  Get the current lifecycle state of a tile instance.
 *
 * Use this for switch/case handling of multiple states. For the
 * common "is it ready?" check, use tile_is_ready() instead.
 *
 * @param  inst  Instance handle
 * @return Current tile_state_t value
 *
 * @code
 *   switch (tile_state(&imu)) {
 *       case TILE_STATE_READY:    break;
 *       case TILE_STATE_SLEEPING: tile_sense_i_9_wake(&imu); break;
 *       case TILE_STATE_ERROR:    handle_error(); break;
 *       default:                  break;
 *   }
 * @endcode
 */
static inline tile_state_t tile_state(tile_t* tile)
{
    return tile->state;
}

/**
 * @brief  Check if a tile instance is ready for use.
 *
 * Convenience wrapper — equivalent to tile_state(tile) == TILE_STATE_READY.
 *
 * @param  tile  Pointer to instance handle
 * @return 1 if state is TILE_STATE_READY, 0 otherwise
 */
static inline uint8_t tile_is_ready(tile_t* tile)
{
    return (tile->state == TILE_STATE_READY) ? 1 : 0;
}

#endif /* INC_TILES_H_ */
