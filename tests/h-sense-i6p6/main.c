/**
 * Read register map around WHO_AM_I to check alignment
 */
#include "core.h"
#include "core_i2c.h"

#define RESULT_BASE  0x2001FF80UL
#define R  ((volatile uint32_t *)RESULT_BASE)

/* Direct read_reg that bypasses probe issues */
static uint8_t i2c_read_reg(uint8_t addr, uint8_t reg)
{
    I2C_TypeDef *i2c = I2C1;
    i2c->ICR = 0x3F38;

    /* Write phase: register address */
    i2c->CR2 = ((uint32_t)addr << 1) | (1UL << 16) | (1UL << 13);
    volatile uint32_t t = 100000;
    while (!(i2c->ISR & (1UL << 1))) { if (--t == 0) return 0xFE; }
    i2c->TXDR = reg;
    t = 100000;
    while (!(i2c->ISR & (1UL << 6))) { if (--t == 0) return 0xFD; }

    /* Read phase */
    i2c->CR2 = ((uint32_t)addr << 1) | (1UL << 16) | (1UL << 10) | (1UL << 25) | (1UL << 13);
    t = 100000;
    while (!(i2c->ISR & (1UL << 2))) { if (--t == 0) return 0xFC; }
    uint8_t val = (uint8_t)i2c->RXDR;
    t = 100000;
    while (!(i2c->ISR & (1UL << 5))) { if (--t == 0) break; }
    i2c->ICR = 0x3F38;
    return val;
}

int main(void)
{
    core_init();
    R[0] = 0xBE61000E;
    core_delay_ms(500);

    /* Read regs 0x73 through 0x77 from 0x69 */
    R[1] = i2c_read_reg(0x69, 0x73);  /* reg before WHO_AM_I */
    R[2] = i2c_read_reg(0x69, 0x74);
    R[3] = i2c_read_reg(0x69, 0x75);  /* WHO_AM_I */
    R[4] = i2c_read_reg(0x69, 0x76);  /* BANK_SEL */
    R[5] = i2c_read_reg(0x69, 0x77);

    /* Also read from 0x68 in case */
    R[6] = i2c_read_reg(0x68, 0x75);

    /* Read some known registers */
    R[7]  = i2c_read_reg(0x69, 0x4E);  /* PWR_MGMT0 */
    R[8]  = i2c_read_reg(0x69, 0x4F);  /* GYRO_CONFIG0 */
    R[9]  = i2c_read_reg(0x69, 0x50);  /* ACCEL_CONFIG0 */
    R[10] = i2c_read_reg(0x69, 0x11);  /* DEVICE_CONFIG */

    R[15] = 0xD04ED04E;

    while (1) core_delay_ms(1000);
}
