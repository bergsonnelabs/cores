/**
 * val-spi-u2 -- SPI tile communication test
 *
 * Core.U.2 (STM32L422) → SPI1 → Sense.I.6P6 (ICM-42686P)
 *
 * Wiring:
 *   Core.U.2 Pad 2  (PA7, AF5)  SPI1.MOSI → Sense.I.6P6 Pad 5  SPI.MOSI
 *   Core.U.2 Pad 10 (PA5, AF5)  SPI1.CLK  → Sense.I.6P6 Pad 4  SPI.CLK
 *   Core.U.2 Pad 18 (PB4, AF5)  SPI1.MISO → Sense.I.6P6 Pad 2  SPI.MISO
 *   Core.U.2 Pad 19 (PA4, GPIO) CS        → Sense.I.6P6 Pad 3  SPI.CS
 *
 * SPI config: Mode 0 (CPOL=0, CPHA=0), 5 MHz (80MHz / 16)
 * Debug output via USB CDC (/dev/tty.usbmodem*)
 */

#include "core.h"
#include "core_spi.h"
#include "core_usb.h"

/* ICM-42686P registers */
#define ICM_WHO_AM_I         0x75
#define ICM_DEVICE_CONFIG    0x11
#define ICM_DRIVE_CONFIG     0x13
#define ICM_PWR_MGMT0       0x4E
#define ICM_ACCEL_CONFIG0    0x50
#define ICM_GYRO_CONFIG0     0x4F
#define ICM_TEMP_H           0x1D
#define ICM_BANK_SEL         0x76
#define ICM_INT_STATUS       0x2D
#define ICM_INTF_CONFIG0     0x4C

#define ICM42686P_WHOAMI     0x44

/* SPI handle */
extern hal_spi_t core_spi1;
#define spi core_spi1

#define dbg(...)  core_usb_printf(__VA_ARGS__)

/* ---- Raw SPI register access ---- */

static uint8_t spi_read_reg(uint8_t reg)
{
    core_spi_select(&spi);
    core_spi_transfer(&spi, 0x80 | reg);
    uint8_t val = core_spi_transfer(&spi, 0x00);
    core_spi_deselect(&spi);
    return val;
}

static void spi_write_reg(uint8_t reg, uint8_t val)
{
    core_spi_select(&spi);
    core_spi_transfer(&spi, 0x7F & reg);
    core_spi_transfer(&spi, val);
    core_spi_deselect(&spi);
}

static void spi_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    core_spi_select(&spi);
    core_spi_transfer(&spi, 0x80 | reg);
    for (uint8_t i = 0; i < len; i++)
        buf[i] = core_spi_transfer(&spi, 0x00);
    core_spi_deselect(&spi);
}

/* ---- Main ---- */

int main(void)
{
    core_init();
    core_led_init();

    /* Wait for USB host */
    LED_ON();
    while (!core_usb_connected())
        core_delay_ms(10);
    LED_OFF();
    core_delay_ms(500);

    dbg("\r\n=== val-spi-u2: SPI tile test ===\r\n");
    dbg("Core.U.2 SPI1 -> Sense.I.6P6 (ICM-42686P)\r\n");
    dbg("Mode 0, 5 MHz | MOSI=pad2 CLK=pad10 MISO=pad18 CS=pad19\r\n\r\n");

    /* CS setup */
    core_spi_set_cs(&spi, 19);
    core_delay_ms(100);

    /* ---- WHO_AM_I ---- */
    uint8_t who = spi_read_reg(ICM_WHO_AM_I);
    dbg("WHO_AM_I = 0x%02X %s\r\n", who,
        who == ICM42686P_WHOAMI ? "(OK)" : "(FAIL)");

    if (who != ICM42686P_WHOAMI) {
        dbg("*** STOPPING ***\r\n");
        while (1) { LED_ON(); core_delay_ms(50); LED_OFF(); core_delay_ms(50); }
    }

    /* ---- Soft reset ---- */
    spi_write_reg(ICM_DEVICE_CONFIG, 0x01);
    core_delay_ms(10);

    /* ---- SPI slew rate (datasheet section 12.3) ---- */
    uint8_t drv = spi_read_reg(ICM_DRIVE_CONFIG);
    dbg("DRIVE_CONFIG before: 0x%02X\r\n", drv);
    drv = (drv & 0xF8) | 0x05;  /* SPI_SLEW_RATE = 5 */
    spi_write_reg(ICM_DRIVE_CONFIG, drv);
    dbg("DRIVE_CONFIG after:  0x%02X\r\n", spi_read_reg(ICM_DRIVE_CONFIG));

    /* ---- Configure sensors ---- */
    spi_write_reg(ICM_BANK_SEL, 0x00);
    spi_write_reg(ICM_ACCEL_CONFIG0, (0x02 << 5) | 0x08);  /* ±8G, 100Hz */
    spi_write_reg(ICM_GYRO_CONFIG0,  (0x02 << 5) | 0x08);  /* ±1000dps, 100Hz */
    spi_write_reg(ICM_PWR_MGMT0, 0x0F);                     /* Accel+Gyro LN */
    core_delay_ms(200);

    uint8_t pwr = spi_read_reg(ICM_PWR_MGMT0);
    uint8_t ist = spi_read_reg(ICM_INT_STATUS);
    dbg("PWR_MGMT0=0x%02X INT_STATUS=0x%02X\r\n\r\n", pwr, ist);

    /* ---- Stream sensor data ---- */
    uint8_t raw[14];
    for (int sample = 0; ; sample++) {
        spi_read_burst(ICM_TEMP_H, raw, 14);

        int16_t temp_raw = (int16_t)((uint16_t)raw[0] << 8 | raw[1]);
        int16_t ax = (int16_t)((uint16_t)raw[2]  << 8 | raw[3]);
        int16_t ay = (int16_t)((uint16_t)raw[4]  << 8 | raw[5]);
        int16_t az = (int16_t)((uint16_t)raw[6]  << 8 | raw[7]);
        int16_t gx = (int16_t)((uint16_t)raw[8]  << 8 | raw[9]);
        int16_t gy = (int16_t)((uint16_t)raw[10] << 8 | raw[11]);
        int16_t gz = (int16_t)((uint16_t)raw[12] << 8 | raw[13]);
        int temp_c10 = (temp_raw * 100) / 1325 + 250;

        if (sample % 10 == 0) {
            dbg("[%4d] T=%d.%dC  A=(%6d,%6d,%6d)  G=(%6d,%6d,%6d)\r\n",
                sample, temp_c10 / 10, temp_c10 % 10,
                ax, ay, az, gx, gy, gz);
        }
        LED_TOGGLE();
        core_delay_ms(100);
    }
}
