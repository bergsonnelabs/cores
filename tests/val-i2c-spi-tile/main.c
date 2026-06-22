/**
 * val-i2c-spi-tile -- Validation: I2C + SPI buses
 *
 * Core.ST.W5, clock=max
 * Pad 10 = I2C1.CLK, Pad 11 = I2C1.DAT
 * Pad 9 = SPI1.CLK, Pad 8 = SPI1.CS
 *
 * Exercises: core_init, core_i2c_init (h, I2C1, I2C_400K),
 *            core_spi_init, core_spi_set_cs, core_spi_transfer
 *
 * Note: tile driver integration (TILES_ENABLED) tested separately
 * in projects/ble-test which uses tile_handles.h from coregen.
 */

#include "core.h"
#include "core_i2c.h"
#include "core_spi.h"

int main(void)
{
    core_init();
    core_led_init();

    /* I2C1 at 400 kHz — speed-based init (no pclk param) */
    core_i2c_t i2c;
    core_i2c_init(&i2c, I2C1, I2C_400K);

    /* Probe for a device */
    int found = core_i2c_probe(&i2c, 0x68);
    (void)found;

    /* I2C read/write */
    uint8_t who;
    core_i2c_read_byte(&i2c, 0x68, 0x75, &who);
    (void)who;

    /* SPI1 with software CS on pad 8 */
    hal_spi_t spi;
    hal_spi_config_t spi_cfg = {
        .prescaler = LL_SPI_PRESCALER_8,
        .cpol = 0,
        .cpha = 0,
    };
    core_spi_init(&spi, SPI1, &spi_cfg);
    core_spi_set_cs(&spi, 8);

    /* Basic SPI transfer */
    core_spi_select(&spi);
    uint8_t rx = core_spi_transfer(&spi, 0x9F);
    core_spi_deselect(&spi);
    (void)rx;

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);
    }
}
