#include "tile_sense_i_6p.h"

static I2C_HandleTypeDef* tile_handle;

uint8_t icm42688p_read(uint8_t reg);
void icm42688p_write(uint8_t reg, uint8_t value);

uint8_t tile_sense_i_6p_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

//	tile_write(LP5811_REG_CHIPEN,1);
	if(icm42688p_read(ICM42688P_REG_WHO_AM_I) != ICM42688P_REG_WHO_AM_I_DEFAULT){
		return 0;
	}

	icm42688p_write(ICM42688P_REG_PWR_MGMT0, 0x0F); // turn on the gyro and accel

	return 1;
}

void tile_sense_i_6p_get_raw_6dof(int16_t* buffer){
	HAL_I2C_Mem_Read(tile_handle, ICM42688P_I2C_ADDR<<1, ICM42688P_REG_ACCEL_DATA_X1, 1, (uint8_t *)buffer, 12, 1000);
	for(int i=0;i<6;i++){
		buffer[i] = (int16_t*)__builtin_bswap16(buffer[i]);
	}
}

double tile_sense_i_6p_get_temp(void){
    return 25.0 + ((((uint16_t)icm42688p_read(ICM42688P_REG_TEMP_DATA1))<<8) + icm42688p_read(ICM42688P_REG_TEMP_DATA0))/132.48;
}

uint8_t icm42688p_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, ICM42688P_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void icm42688p_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, ICM42688P_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}



