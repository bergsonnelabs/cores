#include "tile_sense_i_6d.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
uint8_t lsm6dsv320x_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, LSM6DSV320X_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void lsm6dsv320x_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, LSM6DSV320X_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_i_6d_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

	// validate connectivity by reading the contents of the WHO_AM_I register
	if(lsm6dsv320x_read(LSM6DSV320X_REG_WHO_AM_I) != LSM6DSV320X_REG_WHO_AM_I_DEFAULT){
		return 0;
	}

	// configuration
	lsm6dsv320x_write(LSM6DSV320X_REG_CTRL1, 0b00001100);

	return 1;

}

// these are just test functions... likely not here to stay
int16_t tile_sense_i_6d_get_accel_x(void){
    return (int16_t) ((((uint16_t)lsm6dsv320x_read(LSM6DSV320X_REG_OUTX_H_A))<<8) + lsm6dsv320x_read(LSM6DSV320X_REG_OUTX_L_A));
}

double tile_sense_i_6d_get_temp(void){
	return 25.0 + ((((uint16_t)lsm6dsv320x_read(LSM6DSV320X_REG_TEMP_H))<<8) + lsm6dsv320x_read(LSM6DSV320X_REG_TEMP_L))/256;
}


