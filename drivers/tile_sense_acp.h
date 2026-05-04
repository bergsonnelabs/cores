#ifndef INC_TILE_SENSE_ACP_H_
#define INC_TILE_SENSE_ACP_H_

#include "main.h"
#include "stdint.h"

#define TMD3725_I2C_ADDR				0x39

#define TMD3725_REG_ENABLE				0x80
#define TMD3725_REG_ATIME				0x81
#define TMD3725_REG_PCFG1				0x8F
#define TMD3725_REG_CFG1				0x90
#define TMD3725_REG_ID					0x92
#define TMD3725_REG_CDATAL				0x94
#define TMD3725_REG_CDATAH				0x95
#define TMD3725_REG_RDATAL				0x96
#define TMD3725_REG_PDATA				0x9C

#define TMD3725_REG_ID_DEFAULT			0xE4


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_acp_check(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_acp_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	READ CLEAR-CHANNEL DATA

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint16_t tile_sense_acp_get_cdata(void);

uint8_t tile_sense_acp_get_pdata(void);

void tile_sense_acp_get_rgb(int16_t* buffer);

#endif
