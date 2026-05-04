#include "tile_power_p_n.h"

static I2C_HandleTypeDef* power_p_n_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_power_p_n_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, NPM2100_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_power_p_n_init(I2C_HandleTypeDef* hi2c)
{
	power_p_n_handle = hi2c;

}
