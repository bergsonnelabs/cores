#ifndef INC_TILE_POWER_P_N_
#define INC_TILE_POWER_P_N_

#include "main.h"
#include "stdint.h"

// LED 0 (red) - error condition
// LED 1 (orange) - charging
// LED 2 (green) - host

#define NPM2100_I2C_ADDR				0x74

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_power_p_n_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_power_p_n_init(I2C_HandleTypeDef* hi2c);

#endif
