#include "tile_power_l_1n.h"

static I2C_HandleTypeDef* power_l1_n_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_power_l_1n_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, NPM1300_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_power_l_1n_init(I2C_HandleTypeDef* hi2c)
{
	power_l1_n_handle = hi2c;

	uint8_t TX_Buffer[1];

	// DISABLE CHARGING TO ALLOW ADJUSTMENT
	// NPM1300_REG_BCHGENABLECLR
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	//	1	ENABLEFULLCHGCOOL	0		1		1 = disable full charging of cool battery
	//	0	ENABLECHARGING		0		1		1 = disable charging
	TX_Buffer[0] = 0b00000011;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGENABLECLR, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);

	// DELAY VBAT READINGS BY 200ms
	// NPM1300_REG_ADCDELTIMCONF
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
//	TX_Buffer[0] = 100;
//	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_ADCDELTIMCONF, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);

	// 	NPM1300_REG_ADCCONFIG
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	//	1	VBATBURSTENABLE		0		0		0 = single; 1 = 4 consecutive
	//	0	VBATAUTOENABLE		0		1		0 = triggered; 1 = 1Hz continuous
	TX_Buffer[0] = 0b00000001;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_ADCCONFIG, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_TASKVBATMEASURE, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);

	// NPM1300_REG_ADCNTCRSEL
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	//	1:0		thermistor res.	01		00		00 = no resistor, 01 = 10k, 10 = 47k, 11 = 100k
	TX_Buffer[0] = 0;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_ADCNTCRSEL, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);

	// 	NPM1300_REG_BCHGDISABLESET
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	//	1	DISABLENTC			0		1		1 = ignore NTC thermistor
	//	0	DISABLERECHARGE		0		0		1 = disable recharging
	TX_Buffer[0] = 0b00000010;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGDISABLESET, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);

	// MPM1300_REG_BCHGISETMSB
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	// value = floor(ma/4)
	TX_Buffer[0] = 25;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGISETMSB, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);

	// NPM1300_REG_BCHGVTERM & NPM1300_REG_BCHGVTERMR
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	// 	7:0						0x02	0x08	0=3.5, 1=3.55, 3.6, 3.65, 4.0, 4.05, 4.1, 4.15, 4.2, 4.25, 4.3, 4.35, 4.4, 4.45
	TX_Buffer[0] = 8;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGVTERM, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGVTERMR, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);


	// NPM1300_REG_BCHGENABLESET
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	//	1	ENABLEFULLCHGCOOL	0		1		1 = enable full charging of cool battery
	//	0	ENABLECHARGING		0		1		1 = enable charging
	TX_Buffer[0] = 0b00000011;
	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGENABLESET, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);


	return 1;
}

uint16_t tile_power_l_1n_get_status(void)
{
	uint8_t RX_Buffer[1] = {0};
	uint16_t result = 0;
	HAL_I2C_Mem_Read(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGERRREASON, I2C_MEMADD_SIZE_16BIT, (uint8_t *)RX_Buffer, 1, 1000);
	result = ((uint16_t)RX_Buffer[0])<<8;
	HAL_I2C_Mem_Read(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_BCHGCHARGESTATUS, I2C_MEMADD_SIZE_16BIT, (uint8_t *)RX_Buffer, 1, 1000);
	result = result + RX_Buffer[0];
	return result;
	// NPM1300_REG_BCHGCHARGESTATUS
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	// 	0	battery detected
	//	1	charge completed
	//	2	trickle charge
	//	3	constant current
	//	4	constant voltage
	//	5	recharge needed
	//	6	stopped due to die overtemp
	//	7	spplement mode active

	// 	0x03 = detected & complete
	//	0x05 = trickle charging
	//	0x09 = constant current
	//	0x11 = constant voltage

}

uint16_t tile_power_l_1n_get_vbat(void)
{

	uint8_t TX_Buffer[1];
	uint8_t RX_Buffer[1] = {0};

//	TX_Buffer[0] = 1;
//	HAL_I2C_Mem_Write(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_LEDDRV2SET, I2C_MEMADD_SIZE_16BIT, (uint8_t *)TX_Buffer, 1, 1000);


	HAL_I2C_Mem_Read(power_l1_n_handle, NPM1300_I2C_ADDR<<1, NPM1300_REG_ADCVBATRESULTMSB, I2C_MEMADD_SIZE_16BIT, (uint8_t *)RX_Buffer, 1, 1000);
	uint16_t result = 5 * (((uint16_t)RX_Buffer[0]) << 2);
	return result;//(uint16_t) RX_Buffer[0];

}
