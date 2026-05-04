#ifndef INC_TILE_POWER_L_1N_
#define INC_TILE_POWER_L_1N_

#include "main.h"
#include "stdint.h"

// LED 0 (red) - error condition
// LED 1 (orange) - charging
// LED 2 (green) - host

#define NPM1300_I2C_ADDR				0x6B

#define NPM1300_REG_TASKSWRESET			0x0001
#define NPM1300_REG_EVENTSBCHARGER1SET 	0x000A

#define NPM1300_REG_BCHGENABLESET		0x0304
#define NPM1300_REG_BCHGENABLECLR		0x0305
#define NPM1300_REG_BCHGDISABLESET		0x0306
#define NPM1300_REG_BCHGISETMSB			0x0308
#define NPM1300_REG_BCHGVTERM			0x030C
#define NPM1300_REG_BCHGVTERMR			0x030D
#define NPM1300_REG_BCHGCHARGESTATUS 	0x0334
#define NPM1300_REG_BCHGERRREASON		0x0336

#define NPM1300_REG_TASKVBATMEASURE		0x0500
#define NPM1300_REG_ADCCONFIG			0x0509
#define NPM1300_REG_ADCNTCRSEL			0x050A
#define NPM1300_REG_ADCDELTIMCONF		0x050D
#define NPM1300_REG_ADCVBATRESULTMSB	0x0511

#define NPM1300_REG_LEDDRV0MODESEL		0x0A00
#define NPM1300_REG_LEDDRV1MODESEL		0x0A01
#define NPM1300_REG_LEDDRV2MODESEL		0x0A02
#define NPM1300_REG_LEDDRV0SET			0x0A03
#define NPM1300_REG_LEDDRV0CLR			0x0A04
#define NPM1300_REG_LEDDRV1SET			0x0A05
#define NPM1300_REG_LEDDRV1CLR			0x0A06
#define NPM1300_REG_LEDDRV2SET			0x0A07
#define NPM1300_REG_LEDDRV2CLR			0x0A08

#define NPM1300_REG_SHPHLDCONFIG		0x0B04

#define NPM1300_REG_SCRATCH0			0x0E01
#define NPM1300_REG_SCRATCH1			0x0E02
#define NPM1300_REG_RSTCAUSE			0x0E03
#define NPM1300_REG_CHARGERERRREASON	0x0E04
#define NPM1300_REG_CHARGERERRSENSOR	0x0E05


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_power_l_1n_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_power_l_1n_init(I2C_HandleTypeDef* hi2c);

uint16_t tile_power_l_1n_get_status(void);

uint16_t tile_power_l_1n_get_vbat(void);

#endif
