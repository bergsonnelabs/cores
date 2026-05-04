#include "tile_drive_vc_s.h"
#include "main.h"


static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_drive_vc_s_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, DRV201_I2C_ADDR<<1, DRV201_REG_CONTROL, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] == DRV201_REG_CONTROL_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_drive_vc_s_init(I2C_HandleTypeDef* hi2c){
	tile_handle = hi2c;
	return 1;
}

uint8_t tile_drive_vc_s_output(uint16_t current){

	// ----------------------------------------------------------------------
	// REGISTER: VCA_CURRENT_MSB
	// ----------------------------------------------------------------------
	uint8_t RX_Buffer[1];
	uint8_t TX_Buffer[1];

	TX_Buffer[0] = current>>8;
	HAL_I2C_Mem_Write(tile_handle, DRV201_I2C_ADDR<<1, DRV201_REG_VCM_CURRENT_MSB, 1, (uint8_t *)TX_Buffer, 1, 1000);

	TX_Buffer[0] = (uint8_t)(current & 0x00FF);
	HAL_I2C_Mem_Write(tile_handle, DRV201_I2C_ADDR<<1, DRV201_REG_VCM_CURRENT_LSB, 1, (uint8_t *)TX_Buffer, 1, 1000);

	HAL_I2C_Mem_Read(tile_handle, DRV201_I2C_ADDR<<1, DRV201_REG_STATUS, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];

}
