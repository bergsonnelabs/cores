#include "core.h"
#include "tiles.h"

int main(void)
{
    core_init();

    /* Bridge I2C1 to tile driver layer */
    tiles_hal_core_cfg_t core_hal_i2c1_cfg = {
        .i2c = &core_i2c1,
        .buses = TILES_BUS_I2C,
    };
    tiles_hal_core_init(&core_hal_i2c1, &core_hal_i2c1_cfg);

    /* Bridge I2C3 to tile driver layer */
    tiles_hal_core_cfg_t core_hal_i2c3_cfg = {
        .i2c = &core_i2c3,
        .buses = TILES_BUS_I2C,
    };
    tiles_hal_core_init(&core_hal_i2c3, &core_hal_i2c3_cfg);

    /* Find and initialize tiles */
    tile_sense_i_9_init(&core_hal_i2c3, 0, &tile_sense_i_9_0);
    tile_power_l_1t_init(&core_hal_i2c3, 0, &tile_power_l_1t_0);
    tile_drive_p_init(&core_hal_i2c1, 0, &tile_drive_p_0);
    tile_drive_p_init(&core_hal_i2c3, 1, &tile_drive_p_1);

    while (1) {
    }
}
