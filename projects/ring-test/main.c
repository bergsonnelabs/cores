/**
 * Ring Test — Sense.I.9 driver validation
 *
 * Focused test: find the ICM-20948 at 0x69 on I2C1, read WHO_AM_I,
 * init the Kiln driver, read live data.
 *
 * LED:
 *   1 blink  = I2C1 init OK
 *   2 blinks = WHO_AM_I = 0xEA (correct)
 *   3 blinks = Kiln driver READY
 *   Heartbeat = reading live data (fast = motion, slow = still)
 *   SOS = error
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"
#include "ll_i2c.h"
#include "hal_common.h"
#include "hal_i2c.h"

/* Kiln driver includes */
#include "tiles_hal.h"
#include "tiles_hal_core.h"
#include "tile_sense_i_9.h"

/* ---- Debug variables (readable via SWD) ---- */

volatile uint8_t  dbg_whoami = 0;
volatile uint8_t  dbg_probe_result = 0xFF;
volatile uint8_t  dbg_read_result = 0xFF;
volatile int      dbg_probe_44 = -99;
volatile int      dbg_probe_68 = -99;
volatile int      dbg_probe_69 = -99;
volatile uint8_t  dbg_driver_state = 0;
volatile int16_t  dbg_accel[3];
volatile int16_t  dbg_gyro[3];

/* ---- LED helpers ---- */

static void blink_n(int n, int on_ms, int off_ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  ll_delay_ms(on_ms);
        LED_OFF(); ll_delay_ms(off_ms);
    }
}

static void sos(void)
{
    while (1) {
        blink_n(3, 100, 100);
        blink_n(3, 400, 400);
        blink_n(3, 100, 100);
        ll_delay_ms(2000);
    }
}

/* ---- Main ---- */

static hal_i2c_t i2c1_handle;
static hal_i2c_t i2c3_handle;

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);

    /* LED */
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);
    LED_OFF();

    /* ============================================================
     * Step 1: Init I2C1
     * ============================================================ */

    /* I2C1 clock */
    ll_rcc_apb1_clk_enable(LL_APB1_I2C1);

    /* I2C1 pins: PA15=SCL, PB3=SDA — both AF4, open-drain, pull-up */
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);
    ll_gpio_config_af(GPIOA, 15, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);
    ll_gpio_config_af(GPIOB,  3, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);

    hal_i2c_config_t i2c_cfg = {
        .timing = LL_I2C_TIMING_100K_16MHZ,
        .timeout_ms = 100,
    };
    if (hal_i2c_init(&i2c1_handle, I2C1, &i2c_cfg) != HAL_OK) sos();

    /* I2C3 clock (APB7 on WBA55) + pins */
#if defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xA8UL), LL_APB7_I2C3);
#endif
    ll_gpio_config_af(GPIOA, 6, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);
    ll_gpio_config_af(GPIOA, 7, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);
    if (hal_i2c_init(&i2c3_handle, I2C3, &i2c_cfg) != HAL_OK) sos();

    /* Set up Kiln HAL bridge */
    tiles_hal_t kiln_hal;
    tiles_hal_core_cfg_t kiln_cfg = {
        .i2c = &i2c1_handle,
        .buses = TILES_BUS_I2C,
    };
    tiles_hal_core_init(&kiln_hal, &kiln_cfg);

    blink_n(1, 300, 500);  /* 1 blink = I2C1 init OK */
    ll_delay_ms(1000);

    /* ============================================================
     * Step 2: Probe specific addresses — blink per ACK
     *   0x44 = Drive.P (control)
     *   0x68 = IMU (AD0 low)
     *   0x69 = IMU (AD0 high/float)
     * LED: 1 blink per address that ACKs, long pause between
     * ============================================================ */

    dbg_probe_result = 0;

    /* I2C1: Drive.P (0x44)
     * I2C3: Sense.I.9 (0x69), Power.L.1T, Drive.P (0x44) */

    /* Probe I2C1: Drive.P */
    dbg_probe_44 = hal_i2c_probe(&i2c1_handle, 0x44);
    if (dbg_probe_44 == HAL_OK) {
        dbg_probe_result |= 0x01;
        blink_n(1, 300, 500);
    }
    ll_delay_ms(1000);

    /* Probe I2C3: IMU */
    dbg_probe_69 = hal_i2c_probe(&i2c3_handle, 0x69);
    if (dbg_probe_69 == HAL_OK) {
        dbg_probe_result |= 0x04;
        blink_n(1, 300, 500);
    }
    ll_delay_ms(1000);

    if (!(dbg_probe_result & 0x04)) {
        /* Try alt address */
        dbg_probe_68 = hal_i2c_probe(&i2c3_handle, 0x68);
        if (dbg_probe_68 == HAL_OK) {
            dbg_probe_result |= 0x02;
            blink_n(1, 300, 500);
        }
        ll_delay_ms(1000);
    }

    /* Read WHO_AM_I from IMU on I2C3 */
    uint8_t imu_addr = (dbg_probe_result & 0x04) ? 0x69 :
                       (dbg_probe_result & 0x02) ? 0x68 : 0;
    if (imu_addr) {
        uint8_t who = 0;
        hal_i2c_read_byte(&i2c3_handle, imu_addr, 0x00, &who);
        dbg_whoami = who;
        blink_n(2, 300, 500);  /* 2 blinks = WHO_AM_I read */
    }
    ll_delay_ms(1000);

    if (imu_addr == 0) sos();

    /* ============================================================
     * Step 3: Kiln driver init on I2C3
     * ============================================================ */

    /* Kiln HAL for I2C3 (where the IMU lives) */
    tiles_hal_t kiln_hal_i2c3;
    tiles_hal_core_cfg_t kiln_cfg_i2c3 = {
        .i2c = &i2c3_handle,
        .buses = TILES_BUS_I2C,
    };
    tiles_hal_core_init(&kiln_hal_i2c3, &kiln_cfg_i2c3);

    tile_t imu;
    {
        uint8_t instance = (imu_addr == 0x69) ? 0 : 1;
        tile_sense_i_9_init(&kiln_hal_i2c3, instance, &imu);
        dbg_driver_state = imu.state;

        if (imu.state == TILE_STATE_READY) {
            blink_n(3, 300, 500);  /* 3 blinks = driver ready! */
        } else {
            blink_n(6, 100, 100);  /* 6 fast = driver failed */
            sos();
        }
    }
    ll_delay_ms(500);

    /* ============================================================
     * Step 4: Live data — LED tracks motion
     * ============================================================ */

    while (1) {
        int16_t accel[3], gyro[3];
        tile_sense_i_9_get_raw_accels(&imu, accel);
        tile_sense_i_9_get_raw_gyros(&imu, gyro);

        dbg_accel[0] = accel[0];
        dbg_accel[1] = accel[1];
        dbg_accel[2] = accel[2];
        dbg_gyro[0] = gyro[0];
        dbg_gyro[1] = gyro[1];
        dbg_gyro[2] = gyro[2];

        int32_t mag = (int32_t)accel[0] * accel[0]
                    + (int32_t)accel[1] * accel[1]
                    + (int32_t)accel[2] * accel[2];

        if (mag > 300000000) {
            LED_TOGGLE();
            ll_delay_ms(50);
        } else {
            LED_TOGGLE();
            ll_delay_ms(500);
        }
    }
}
