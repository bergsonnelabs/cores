#ifndef INC_TILE_SENSE_HR_H_
#define INC_TILE_SENSE_HR_H_

#include "main.h"
#include "stdint.h"

#define MAX86174_I2C_ADDR				0x6B

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_hr_find(I2C_HandleTypeDef* hi2c);

#endif
