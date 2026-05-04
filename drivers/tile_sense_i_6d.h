#ifndef INC_TILE_SENSE_I_6D_H_
#define INC_TILE_SENSE_I_6D_H_

#include "main.h"
#include "stdint.h"

#define LSM6DSV320X_I2C_ADDR			0x6B

#define LSM6DSV320X_REG_WHO_AM_I		0x0F
#define LSM6DSV320X_REG_TEMP_L			0x20
#define LSM6DSV320X_REG_TEMP_H			0x21
#define LSM6DSV320X_REG_CTRL1			0x10
#define LSM6DSV320X_REG_OUTX_L_A		0x28
#define LSM6DSV320X_REG_OUTX_H_A		0x29

#define LSM6DSV320X_REG_WHO_AM_I_DEFAULT	0x73

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_i_6d_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	GET RAW TEMP DATA

	parameters
	-------------------------------------------------------
	n/a

	returns
	-------------------------------------------------------
	temp		calculated temperature in degrees C as a 32-bit double

**********************************************************/
double tile_sense_i_6d_get_temp(void);

/**********************************************************

	GET RAW ACCEL-X DATA

	parameters
	-------------------------------------------------------
	n/a

	returns
	-------------------------------------------------------
	a_x			raw signed 16-bit accel-x channel data

**********************************************************/

int16_t tile_sense_i_6d_get_accel_x(void);

#endif
