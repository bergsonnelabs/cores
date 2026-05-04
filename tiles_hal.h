/**
 * @file   tiles_hal.h
 * @brief  Platform abstraction layer for tile drivers.
 *
 * Provides a hardware-independent interface for bus communication (I2C,
 * SPI, QSPI), timing, and optional error reporting. Each tile driver
 * accepts a tiles_hal_t pointer instead of a platform-specific handle,
 * making drivers portable across MCU families and frameworks.
 *
 * To port to a new platform, create a cfg struct and init function
 * that populates the tiles_hal_t function pointers for your target's
 * bus and delay primitives. See the HAL/ directory for ready-to-use
 * implementations (Arduino, ESP-IDF, STM32).
 *
 * Usage:
 * @code
 *   tiles_hal_stm32_cfg_t cfg = {
 *       .i2c = &hi2c1,
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_hal_t hal;
 *   tiles_hal_stm32_init(&hal, &cfg);
 *
 *   tile_t imu = tile_sense_i_9_init(&hal, 0);
 * @endcode
 */

#ifndef INC_TILES_HAL_H_
#define INC_TILES_HAL_H_

#include <stdint.h>

/* Forward declaration — tile_t is defined in tiles.h */
struct tile;

/* ------------------------------------------------------------------ */
/*  Bus type flags                                                    */
/* ------------------------------------------------------------------ */

#define TILES_BUS_I2C   (1 << 0)   /**< I2C bus available   */
#define TILES_BUS_SPI   (1 << 1)   /**< SPI bus available   */
#define TILES_BUS_QSPI  (1 << 2)   /**< QSPI bus available  */
#define TILES_BUS_I3C   (1 << 3)   /**< I3C bus available   */

/* ------------------------------------------------------------------ */
/*  Platform abstraction handle                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief  Platform abstraction handle.
 *
 * Wraps opaque platform handles with portable function pointers for
 * register-level bus access, millisecond delays, and optional error
 * reporting. Populate only the buses your project uses — leave unused
 * pointers NULL.
 */
typedef struct {
    /* --- I2C --- */

    /**
     * @brief  Read one or more bytes from a device register via I2C.
     *
     * @param  handle   Opaque platform handle (stored in tiles_hal_t.handle)
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  reg      Register address (8-bit or 16-bit; if > 0xFF, two
     *                  address bytes are sent MSB-first on the wire)
     * @param  data     Output buffer
     * @param  len      Number of bytes to read
     * @return 0 on success, non-zero on error
     */
    int (*i2c_read)(void* handle, uint8_t addr, uint16_t reg,
                    uint8_t* data, uint16_t len);

    /**
     * @brief  Write one or more bytes to a device register via I2C.
     *
     * @param  handle   Opaque platform handle (stored in tiles_hal_t.handle)
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  reg      Register address (8-bit or 16-bit; if > 0xFF, two
     *                  address bytes are sent MSB-first on the wire)
     * @param  data     Input buffer
     * @param  len      Number of bytes to write
     * @return 0 on success, non-zero on error
     */
    int (*i2c_write)(void* handle, uint8_t addr, uint16_t reg,
                     const uint8_t* data, uint16_t len);

    /**
     * @brief  Check whether a device acknowledges its address on I2C.
     *
     * @param  handle   Opaque platform handle
     * @param  addr     7-bit I2C device address (unshifted)
     * @return 0 if device is present, non-zero otherwise
     */
    int (*i2c_is_ready)(void* handle, uint8_t addr);

    /**
     * @brief  Raw I2C write — no register address, just data bytes.
     *
     * For devices that use command-based protocols (e.g., Sensirion STC31-C)
     * or config-byte patterns (e.g., MAX11644 ADC) where there is no
     * register address in the transaction.
     *
     * @param  handle   Opaque platform handle
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  data     Bytes to send after the address
     * @param  len      Number of bytes
     * @return 0 on success, non-zero on error.  NULL if not supported.
     */
    int (*i2c_write_raw)(void* handle, uint8_t addr,
                         const uint8_t* data, uint16_t len);

    /**
     * @brief  Raw I2C read — no register address, just receive data.
     *
     * @param  handle   Opaque platform handle
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  data     Output buffer
     * @param  len      Number of bytes to read
     * @return 0 on success, non-zero on error.  NULL if not supported.
     */
    int (*i2c_read_raw)(void* handle, uint8_t addr,
                        uint8_t* data, uint16_t len);

    /* --- SPI --- */

    /**
     * @brief  Read one or more bytes from a device register via SPI.
     *
     * @param  handle   Opaque platform handle
     * @param  cs       Chip-select identifier (pin number or index)
     * @param  reg      Register address to read from (read bit applied by impl)
     * @param  data     Output buffer
     * @param  len      Number of bytes to read
     * @return 0 on success, non-zero on error
     */
    int (*spi_read)(void* handle, uint8_t cs, uint8_t reg,
                    uint8_t* data, uint16_t len);

    /**
     * @brief  Write one or more bytes to a device register via SPI.
     *
     * @param  handle   Opaque platform handle
     * @param  cs       Chip-select identifier (pin number or index)
     * @param  reg      Register address to write to
     * @param  data     Input buffer
     * @param  len      Number of bytes to write
     * @return 0 on success, non-zero on error
     */
    int (*spi_write)(void* handle, uint8_t cs, uint8_t reg,
                     const uint8_t* data, uint16_t len);

    /* --- GPIO interrupt --- */

    /**
     * @brief  Enable an edge-triggered interrupt on a GPIO pin.
     *
     * Used by drivers for INT, RDY, ALERT, DRDY output pins on tiles.
     * The callback runs in ISR context — keep it short (set a flag).
     *
     * @param  handle  Opaque platform handle
     * @param  pin     Platform pin identifier (e.g., Core pad number)
     * @param  edge    0 = falling, 1 = rising, 2 = both edges
     * @param  cb      Callback to fire on edge event
     * @param  ctx     User context passed to callback
     * @return 0 on success, non-zero on error.  NULL if not supported.
     */
    int (*gpio_irq_enable)(void* handle, uint8_t pin, uint8_t edge,
                           void (*cb)(void* ctx), void* ctx);

    /**
     * @brief  Disable a previously enabled GPIO interrupt.
     *
     * @param  handle  Opaque platform handle
     * @param  pin     Same pin passed to gpio_irq_enable
     *                 NULL if not supported.
     */
    void (*gpio_irq_disable)(void* handle, uint8_t pin);

    /* Edge constants for gpio_irq_enable */
    #define TILES_GPIO_EDGE_FALLING  0
    #define TILES_GPIO_EDGE_RISING   1
    #define TILES_GPIO_EDGE_BOTH     2

    /* --- Shared --- */

    /**
     * @brief  Blocking delay in milliseconds.
     *
     * @param  ms  Number of milliseconds to wait
     */
    void (*delay_ms)(uint32_t ms);

    /**
     * @brief  Optional error callback — set to NULL to silence.
     *
     * When a driver detects an error (wrong state, bus failure, etc.),
     * it calls this if non-NULL. Route to printf, UART, BLE, etc.
     *
     * @param  inst  The tile instance that triggered the error
     * @param  msg   Human-readable description
     */
    void (*on_error)(struct tile t, const char* msg);

    /** @brief  Opaque platform handle passed to I2C callbacks. */
    void* handle;

    /** @brief  Active bus flags (TILES_BUS_I2C, TILES_BUS_SPI, etc.). */
    uint8_t buses;
} tiles_hal_t;

#endif /* INC_TILES_HAL_H_ */
