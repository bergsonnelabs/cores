#include "tile_sense_hr.h"

static I2C_HandleTypeDef* tile_handle;


// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_hr_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, MAX86174_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}
