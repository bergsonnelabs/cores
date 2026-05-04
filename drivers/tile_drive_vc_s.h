#ifndef INC_TILE_DRIVE_VC_S_H_
#define INC_TILE_DRIVE_VC_S_H_

#include "main.h"
#include "stdint.h"

#define DRV201_I2C_ADDR						0x0E

#define DRV201_REG_CONTROL					0x02

#define DRV201_REG_CONTROL_DEFAULT			0x02
#define DRV201_REG_VCM_CURRENT_MSB			0x03
#define DRV201_REG_VCM_CURRENT_LSB			0x04
#define DRV201_REG_STATUS					0x05
#define DRV201_REG_MODE						0x06
#define DRV201_REG_VCM_FREQ					0x07

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_vc_s_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_vc_s_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	SET OUTPUT CURRENT AND GET STATUS

	parameters
	-------------------------------------------------------
	current		10bit, 0.1mA per LSB = 0, 0.1, ..., 102.3mA

	returns
	-------------------------------------------------------
	status		status register

**********************************************************/
uint8_t tile_drive_vc_s_output(uint16_t current);


#endif
