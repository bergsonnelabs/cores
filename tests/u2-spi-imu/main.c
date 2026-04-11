/**
 * SPI debug — verify the peripheral is actually clocking.
 */
#include "core.h"
#include "core_usb.h"
#include "core_spi.h"
#include "ll_gpio.h"

/* CS = PB0 */
#define CS_LOW()   ll_gpio_clear(GPIOB, (1UL << 0))
#define CS_HIGH()  ll_gpio_set(GPIOB, (1UL << 0))

int main(void)
{
    core_init();
    core_usb_init();
    core_delay_ms(500);

    CS_HIGH();

    while (!core_usb_connected()) core_delay_ms(100);
    core_delay_ms(500);

    core_usb_printf("\r\n=== SPI Debug ===\r\n");

    /* Dump SPI1 config */
    uint32_t cr1 = *(volatile uint32_t *)(0x40013000);
    uint32_t cr2 = *(volatile uint32_t *)(0x40013004);
    uint32_t sr  = *(volatile uint32_t *)(0x40013008);
    core_usb_printf("SPI1 CR1=0x%08lX CR2=0x%08lX SR=0x%08lX\r\n", cr1, cr2, sr);
    core_usb_printf("  SPE=%d MSTR=%d CPOL=%d CPHA=%d BR=%d\r\n",
        (cr1 >> 6) & 1, (cr1 >> 2) & 1, (cr1 >> 1) & 1, cr1 & 1, (cr1 >> 3) & 7);
    core_usb_printf("  DS=%d FRXTH=%d\r\n", (cr2 >> 8) & 0xF, (cr2 >> 12) & 1);

    /* Try HAL SPI transfer */
    hal_spi_set_cs(&core_spi1, GPIOB, 0);

    core_usb_printf("\r\nHAL transfer test:\r\n");
    hal_spi_select(&core_spi1);
    uint8_t r1 = hal_spi_transfer(&core_spi1, 0x75 | 0x80);  /* Read WHO_AM_I */
    uint8_t r2 = hal_spi_transfer(&core_spi1, 0x00);
    hal_spi_deselect(&core_spi1);
    core_usb_printf("  TX: 0xF5,0x00 -> RX: 0x%02X,0x%02X\r\n", r1, r2);

    /* Manual CS + raw DR access */
    core_usb_printf("\r\nRaw register test:\r\n");
    CS_LOW();
    volatile uint32_t *spi_dr = (volatile uint32_t *)(0x4001300C);
    volatile uint32_t *spi_sr = (volatile uint32_t *)(0x40013008);

    /* Send 0xF5 (read WHO_AM_I) */
    while (!(*spi_sr & (1 << 1))) ;  /* TXE */
    *(volatile uint8_t *)spi_dr = 0xF5;
    while (!(*spi_sr & (1 << 0))) ;  /* RXNE */
    r1 = *(volatile uint8_t *)spi_dr;

    /* Send 0x00 (dummy to clock data in) */
    while (!(*spi_sr & (1 << 1))) ;
    *(volatile uint8_t *)spi_dr = 0x00;
    while (!(*spi_sr & (1 << 0))) ;
    r2 = *(volatile uint8_t *)spi_dr;

    CS_HIGH();
    core_usb_printf("  TX: 0xF5,0x00 -> RX: 0x%02X,0x%02X\r\n", r1, r2);

    /* GPIO state check */
    core_usb_printf("\r\nGPIO state:\r\n");
    core_usb_printf("  PA1(CLK) mode=%lu af=%lu\r\n",
        (GPIOA->MODER >> 2) & 3, (GPIOA->AFR[0] >> 4) & 0xF);
    core_usb_printf("  PA7(MOSI) mode=%lu af=%lu\r\n",
        (GPIOA->MODER >> 14) & 3, (GPIOA->AFR[0] >> 28) & 0xF);
    core_usb_printf("  PA6(MISO) mode=%lu af=%lu\r\n",
        (GPIOA->MODER >> 12) & 3, (GPIOA->AFR[0] >> 24) & 0xF);
    core_usb_printf("  PB0(CS) mode=%lu out=%lu\r\n",
        (GPIOB->MODER >> 0) & 3, (GPIOB->ODR >> 0) & 1);

    core_usb_printf("\r\nDone.\r\n");
    while (1) core_delay_ms(1000);
}
