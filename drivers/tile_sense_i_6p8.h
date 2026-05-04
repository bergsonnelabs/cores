#ifndef INC_TILE_SENSE_I_6P_H_
#define INC_TILE_SENSE_I_6P_H_

#include "main.h"
#include "stdint.h"

#define ICM42688P_I2C_ADDR				0b1101001

#define ICM42688P_REG_TEMP_DATA1		0x1D
#define ICM42688P_REG_TEMP_DATA0		0x1E
#define ICM42688P_REG_ACCEL_DATA_X1		0x1F
#define ICM42688P_REG_ACCEL_DATA_X0		0x20
#define ICM42688P_REG_PWR_MGMT0			0x4E
#define ICM42688P_REG_GYRO_CONFIG0		0x4F
#define ICM42688P_REG_ACCEL_CONFIG0		0x50
#define ICM42688P_REG_WHO_AM_I			0x75

#define ICM42688P_REG_WHO_AM_I_DEFAULT	0x47

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_i_6p_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	GET RAW ACCEL & GYRO DATA

	parameters
	-------------------------------------------------------
	buffer		pointer to a 6-element int16_t array to be filled as [AX AY AZ GX GY GZ]

	returns
	-------------------------------------------------------
	n/a

**********************************************************/
void tile_sense_i_6p_get_raw_6dof(int16_t* buffer);

/**********************************************************

	GET RAW TEMP DATA

	parameters
	-------------------------------------------------------
	n/a

	returns
	-------------------------------------------------------
	temp		calculated temperature in degrees C as a 32-bit double

**********************************************************/
double tile_sense_i_6p_get_temp(void);

#endif
