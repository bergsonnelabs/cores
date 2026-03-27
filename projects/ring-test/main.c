/**
 * Ring Test — Multi-level I2C driver integration test
 *
 * Uses the Ring assembly (Core.W + Sense.I.9 + Power.L.1T + dual Drive.P)
 * to test the full Cores SDK → HAL → Kiln driver stack.
 *
 * LED patterns (PB12):
 *   Level 1: 1 blink per I2C1 device found (expect 3), pause,
 *            1 blink per I2C3 device found (expect 1)
 *   Level 2: 3 fast blinks = WHO_AM_I verified (0xEA)
 *   Level 3: Steady 500ms blink = Kiln Sense.I.9 driver initialized
 *   Level 4: LED tracks motion (fast blink = moving, slow = still)
 *   SOS: bus error or driver failure
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

volatile uint8_t  dbg_i2c1_found[16];   /* I2C1 device addresses found */
volatile uint8_t  dbg_i2c1_count = 0;
volatile uint8_t  dbg_i2c3_found[16];
volatile uint8_t  dbg_i2c3_count = 0;
volatile uint8_t  dbg_whoami = 0;
volatile uint8_t  dbg_level = 0;
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

/* ---- I2C bus instances ---- */

static hal_i2c_t i2c1_handle;
static hal_i2c_t i2c3_handle;

/* ---- Main ---- */

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);

    /* LED */
    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);
    LED_OFF();

    /* Brief startup flash */
    blink_n(2, 50, 50);
    ll_delay_ms(500);

    /* ============================================================
     * Level 1: I2C Bus Alive
     * ============================================================ */
    dbg_level = 1;

    /* Enable I2C peripheral clocks */
#if defined(STM32WBA55xx)
    ll_rcc_apb1_clk_enable(LL_APB1_I2C1);
    /* I2C3 is on APB7 for WBA55 */
    SET_BITS(REG32(RCC_BASE + 0xA8UL), LL_APB7_I2C3);  /* APB7ENR */
#endif

    /* Configure I2C pins (AF4 for I2C1 on WBA55) */
    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);

    /* I2C1: PA15=SCL (AF4), PB3=SDA (AF4) */
    ll_gpio_config_af(GPIOA, 15, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);
    ll_gpio_config_af(GPIOB,  3, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);

    /* I2C3: PA6=SCL (AF4), PA7=SDA (AF4) */
    ll_gpio_config_af(GPIOA, 6, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);
    ll_gpio_config_af(GPIOA, 7, 4, LL_GPIO_OTYPE_OD, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_UP);

    /* Init I2C HAL */
    hal_i2c_config_t i2c_cfg = {
        .timing = LL_I2C_TIMING_400K_32MHZ,
        .timeout_ms = 100,
    };
    if (hal_i2c_init(&i2c1_handle, I2C1, &i2c_cfg) != HAL_OK) sos();
    if (hal_i2c_init(&i2c3_handle, I2C3, &i2c_cfg) != HAL_OK) sos();

    /* Scan I2C1 */
    hal_i2c_scan(&i2c1_handle, (uint8_t *)dbg_i2c1_found, (uint8_t *)&dbg_i2c1_count, 16);
    /* Scan I2C3 */
    hal_i2c_scan(&i2c3_handle, (uint8_t *)dbg_i2c3_found, (uint8_t *)&dbg_i2c3_count, 16);

    /* LED: blink count = I2C1 devices, pause, I2C3 devices */
    blink_n(dbg_i2c1_count, 300, 200);
    ll_delay_ms(1000);
    blink_n(dbg_i2c3_count, 300, 200);
    ll_delay_ms(1000);

    if (dbg_i2c1_count == 0) sos();  /* No devices = bus problem */

    /* ============================================================
     * Level 2: WHO_AM_I verification
     * ============================================================ */
    dbg_level = 2;

    uint8_t who = 0;
    if (hal_i2c_read_byte(&i2c1_handle, 0x69, 0x00, &who) != HAL_OK) {
        blink_n(1, 1000, 1000);  /* 1 long = read failed */
        sos();
    }
    dbg_whoami = who;

    if (who == 0xEA) {
        blink_n(3, 100, 100);  /* 3 fast = WHO_AM_I correct! */
    } else {
        blink_n(5, 500, 500);  /* 5 slow = wrong ID */
    }
    ll_delay_ms(1000);

    /* ============================================================
     * Level 3: Kiln Driver Init
     * ============================================================ */
    dbg_level = 3;

    /* Set up Kiln HAL bridge for I2C1 */
    tiles_hal_t kiln_hal;
    tiles_hal_core_cfg_t kiln_cfg = {
        .i2c = &i2c1_handle,
        .buses = TILES_BUS_I2C,
    };
    tiles_hal_core_init(&kiln_hal, &kiln_cfg);

    /* Initialize Sense.I.9 driver */
    tile_t imu;
    tile_sense_i_9_init(&kiln_hal, 0, &imu);
    dbg_driver_state = imu.state;

    if (imu.state == TILE_STATE_READY) {
        blink_n(4, 100, 100);  /* 4 fast = driver ready! */
    } else {
        blink_n(2, 1000, 500);  /* 2 long = driver failed */
        sos();
    }
    ll_delay_ms(1000);

    /* ============================================================
     * Level 4: Live Data
     * ============================================================ */
    dbg_level = 4;

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

        /* Motion detection: magnitude of accel vector */
        int32_t mag = (int32_t)accel[0] * accel[0]
                    + (int32_t)accel[1] * accel[1]
                    + (int32_t)accel[2] * accel[2];

        if (mag > 300000000) {
            /* Strong motion — fast blink */
            LED_TOGGLE();
            ll_delay_ms(50);
        } else {
            /* Mostly still — slow blink */
            LED_TOGGLE();
            ll_delay_ms(500);
        }
    }
}
