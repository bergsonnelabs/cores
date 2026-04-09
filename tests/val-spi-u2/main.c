/**
 * val-spi-u2 -- First-ever SPI tile communication test
 *
 * Core.U.2 (STM32L422) → SPI1 → Sense.I.6P6 (ICM-42686P)
 *
 * Wiring:
 *   Core.U.2 Pad 2  (PA7, AF5)  SPI1.MOSI → Sense.I.6P6 Pad 5  SPI.MOSI
 *   Core.U.2 Pad 10 (PA5, AF5)  SPI1.CLK  → Sense.I.6P6 Pad 4  SPI.CLK
 *   Core.U.2 Pad 9  (PA6, AF5)  SPI1.MISO → Sense.I.6P6 Pad 2  SPI.MISO
 *   Core.U.2 Pad 11 (PB0, GPIO) CS        → Sense.I.6P6 Pad 3  SPI.CS
 *
 * SPI config: Mode 0 (CPOL=0, CPHA=0), 5 MHz (80MHz / 16)
 * Debug output via USB CDC (/dev/tty.usbmodem*)
 */

#include "core.h"
#include "core_spi.h"
#include "core_usb.h"

/* ICM-42686P registers */
#define ICM_WHO_AM_I         0x75
#define ICM_DEVICE_CONFIG    0x11
#define ICM_PWR_MGMT0       0x4E
#define ICM_ACCEL_CONFIG0    0x50
#define ICM_GYRO_CONFIG0     0x4F
#define ICM_TEMP_H           0x1D
#define ICM_BANK_SEL         0x76
#define ICM_INT_STATUS       0x2D
#define ICM_INTF_CONFIG0     0x4C

#define ICM42686P_WHOAMI     0x44
#define ICM42688P_WHOAMI     0x47

/* SPI handle */
extern hal_spi_t core_spi1;
#define spi core_spi1

/* ---- Raw SPI register access ---- */

static uint8_t spi_read_reg(uint8_t reg)
{
    core_spi_select(&spi);
    core_spi_transfer(&spi, 0x80 | reg);
    uint8_t val = core_spi_transfer(&spi, 0x00);
    core_spi_deselect(&spi);
    return val;
}

static void spi_write_reg(uint8_t reg, uint8_t val)
{
    core_spi_select(&spi);
    core_spi_transfer(&spi, 0x7F & reg);
    core_spi_transfer(&spi, val);
    core_spi_deselect(&spi);
}

static void spi_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    core_spi_select(&spi);
    core_spi_transfer(&spi, 0x80 | reg);
    for (uint8_t i = 0; i < len; i++)
        buf[i] = core_spi_transfer(&spi, 0x00);
    core_spi_deselect(&spi);
}

#define dbg(...)  core_usb_printf(__VA_ARGS__)

int main(void)
{
    core_init();
    core_led_init();

    /* Wait for USB host */
    LED_ON();
    while (!core_usb_connected())
        core_delay_ms(10);
    LED_OFF();
    core_delay_ms(500);

    dbg("\r\n=== val-spi-u2: SPI tile test ===\r\n");
    dbg("Core.U.2 -> SPI1 -> Sense.I.6P6 (ICM-42686P)\r\n");
    dbg("SPI: Mode 0, 5 MHz (80/16)\r\n");
    dbg("Pads: MOSI=2(PA7) CLK=10(PA5) MISO=9(PA6) CS=11(PB0)\r\n\r\n");

    /* ---- CS setup ---- */
    core_spi_set_cs(&spi, 11);
    dbg("CS on pad 11 (PB0), deasserted HIGH\r\n");

    /* ---- GPIO register dump ---- */
    dbg("\r\nGPIO registers:\r\n");
    dbg("  GPIOA MODER = 0x%08lX\r\n", (unsigned long)GPIOA->MODER);
    dbg("  GPIOA AFRL  = 0x%08lX\r\n", (unsigned long)GPIOA->AFR[0]);
    dbg("  GPIOA AFRH  = 0x%08lX\r\n", (unsigned long)GPIOA->AFR[1]);
    dbg("  GPIOB MODER = 0x%08lX\r\n", (unsigned long)GPIOB->MODER);
    dbg("  Decode:\r\n");
    uint32_t moder = GPIOA->MODER;
    uint32_t afrl  = GPIOA->AFR[0];
    dbg("    PA1(CLK):  MODER=%d AF=%d  (want: MODER=2 AF=5)\r\n",
        (int)((moder >> 2) & 3), (int)((afrl >> 4) & 0xF));
    dbg("    PA6(MISO): MODER=%d AF=%d  (want: MODER=2 AF=5)\r\n",
        (int)((moder >> 12) & 3), (int)((afrl >> 24) & 0xF));
    dbg("    PA7(MOSI): MODER=%d AF=%d  (want: MODER=2 AF=5)\r\n",
        (int)((moder >> 14) & 3), (int)((afrl >> 28) & 0xF));
    uint32_t moder_b = GPIOB->MODER;
    dbg("    PB0(CS):   MODER=%d        (want: MODER=1 = output)\r\n",
        (int)((moder_b >> 0) & 3));

    /* ---- GPIO pin state ---- */
    uint32_t idr_a = GPIOA->IDR;
    uint32_t idr_b = GPIOB->IDR;
    dbg("  IDR: PA1/CLK=%d PA6/MISO=%d PA7/MOSI=%d PB0/CS=%d\r\n",
        (int)((idr_a >> 1) & 1), (int)((idr_a >> 6) & 1),
        (int)((idr_a >> 7) & 1), (int)((idr_b >> 0) & 1));

    /* ---- SPI register dump ---- */
    dbg("\r\nSPI1 regs: CR1=0x%08lX CR2=0x%08lX SR=0x%08lX\r\n",
        (unsigned long)SPI1->CR1, (unsigned long)SPI1->CR2,
        (unsigned long)SPI1->SR);

    /* ================================================================
     * BIT-BANG I2C PROBE
     * With CS high (I2C mode), use the same CLK/MOSI wires as
     * SCL/SDA to probe the ICM-42686P at 0x69.
     * This verifies: (a) tile alive, (b) CLK wire works, (c) MOSI wire works.
     * ================================================================ */
    dbg("\r\n--- Bit-bang I2C probe (CS high = I2C mode) ---\r\n");
    {
        /* Ensure CS is HIGH → I2C mode */
        GPIOB->BSRR = (1UL << 0);  /* PB0 = 1 */

        /* Disable SPI so we can use PA5 and PA7 as GPIO */
        SPI1->CR1 &= ~(1UL << 6);  /* Clear SPE */

        /* PA5 (pad 10, SCL): open-drain output, start HIGH */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);  /* output */
        GPIOA->OTYPER |= (1UL << 5);  /* open-drain */
        GPIOA->BSRR = (1UL << 5);  /* PA5 = 1 (release SCL) */

        /* PA7 (pad 2, SDA): open-drain output, start HIGH */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 14)) | (1UL << 14);  /* output */
        GPIOA->OTYPER |= (1UL << 7);  /* open-drain */
        GPIOA->BSRR = (1UL << 7);  /* PA7 = 1 (release SDA) */

        /* Enable pull-ups on both */
        GPIOA->PUPDR = (GPIOA->PUPDR & ~(3UL << 10)) | (1UL << 10);  /* PA5 pull-up */
        GPIOA->PUPDR = (GPIOA->PUPDR & ~(3UL << 14)) | (1UL << 14);  /* PA7 pull-up */

        core_delay_ms(1);

        /* Macros for bit-bang I2C */
        #define I2C_DELAY() do { for (volatile int _d = 0; _d < 40; _d++); } while(0)
        #define SCL_HIGH()  GPIOA->BSRR = (1UL << 5)
        #define SCL_LOW()   GPIOA->BSRR = (1UL << (5+16))
        #define SDA_HIGH()  GPIOA->BSRR = (1UL << 7)
        #define SDA_LOW()   GPIOA->BSRR = (1UL << (7+16))
        #define SDA_READ()  ((GPIOA->IDR >> 7) & 1)

        /* Check bus idle */
        int scl_idle = (GPIOA->IDR >> 5) & 1;
        int sda_idle = SDA_READ();
        dbg("  Bus idle: SCL=%d SDA=%d (want: 1,1)\r\n", scl_idle, sda_idle);

        /* I2C START: SDA goes low while SCL is high */
        SDA_LOW(); I2C_DELAY();
        SCL_LOW(); I2C_DELAY();

        /* Send address byte: 0x69 << 1 | 0 (write) = 0xD2 */
        /* Then also try read: 0x69 << 1 | 1 = 0xD3 */
        uint8_t addr_w = (0x69 << 1) | 0;  /* 0xD2 */
        uint8_t addr_r = (0x69 << 1) | 1;  /* 0xD3 */

        /* Clock out addr_w, MSB first */
        for (int bit = 7; bit >= 0; bit--) {
            if (addr_w & (1 << bit)) SDA_HIGH(); else SDA_LOW();
            I2C_DELAY();
            SCL_HIGH(); I2C_DELAY();
            SCL_LOW(); I2C_DELAY();
        }

        /* Release SDA for ACK, clock ACK bit */
        SDA_HIGH(); I2C_DELAY();
        SCL_HIGH(); I2C_DELAY();
        int ack_w = SDA_READ();  /* 0 = ACK, 1 = NACK */
        SCL_LOW(); I2C_DELAY();

        /* STOP */
        SDA_LOW(); I2C_DELAY();
        SCL_HIGH(); I2C_DELAY();
        SDA_HIGH(); I2C_DELAY();

        dbg("  I2C write probe 0x69: %s (SDA=%d on ACK clock)\r\n",
            ack_w == 0 ? "ACK" : "NACK", ack_w);

        core_delay_ms(1);

        /* Try addr_r too */
        SDA_LOW(); I2C_DELAY();
        SCL_LOW(); I2C_DELAY();
        for (int bit = 7; bit >= 0; bit--) {
            if (addr_r & (1 << bit)) SDA_HIGH(); else SDA_LOW();
            I2C_DELAY();
            SCL_HIGH(); I2C_DELAY();
            SCL_LOW(); I2C_DELAY();
        }
        SDA_HIGH(); I2C_DELAY();
        SCL_HIGH(); I2C_DELAY();
        int ack_r = SDA_READ();
        SCL_LOW(); I2C_DELAY();

        /* If ACK, read one byte (WHO_AM_I at 0x75 — need register write first) */
        uint8_t i2c_who = 0;
        if (ack_w == 0) {
            /* Full sequence: START → addr+W → reg(0x75) → rSTART → addr+R → read byte → NACK → STOP */
            /* START */
            SDA_HIGH(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY();
            SDA_LOW(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();

            /* addr+W */
            for (int bit = 7; bit >= 0; bit--) {
                if (addr_w & (1 << bit)) SDA_HIGH(); else SDA_LOW();
                I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();
            }
            SDA_HIGH(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();

            /* register address: 0x75 */
            uint8_t reg = 0x75;
            for (int bit = 7; bit >= 0; bit--) {
                if (reg & (1 << bit)) SDA_HIGH(); else SDA_LOW();
                I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();
            }
            SDA_HIGH(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();

            /* Repeated START */
            SDA_HIGH(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY();
            SDA_LOW(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();

            /* addr+R */
            for (int bit = 7; bit >= 0; bit--) {
                if (addr_r & (1 << bit)) SDA_HIGH(); else SDA_LOW();
                I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();
            }
            SDA_HIGH(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();

            /* Read one byte */
            SDA_HIGH();  /* Release SDA for slave to drive */
            for (int bit = 7; bit >= 0; bit--) {
                I2C_DELAY(); SCL_HIGH(); I2C_DELAY();
                i2c_who |= (SDA_READ() << bit);
                SCL_LOW(); I2C_DELAY();
            }

            /* NACK (master doesn't ACK last byte) */
            SDA_HIGH(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SCL_LOW(); I2C_DELAY();

            /* STOP */
            SDA_LOW(); I2C_DELAY(); SCL_HIGH(); I2C_DELAY(); SDA_HIGH(); I2C_DELAY();

            dbg("  I2C WHO_AM_I = 0x%02X", i2c_who);
            if (i2c_who == 0x44) dbg(" — ICM-42686-P confirmed ALIVE!\r\n");
            else if (i2c_who == 0x47) dbg(" — ICM-42688 variant, ALIVE!\r\n");
            else dbg(" — unexpected\r\n");
        } else {
            dbg("  I2C read probe 0x69: %s\r\n", ack_r == 0 ? "ACK" : "NACK");
            dbg("  No ACK on I2C — tile may not be powered or pad 10/2 disconnected\r\n");
        }

        /* Restore AF mode for SPI */
        GPIOA->OTYPER &= ~(1UL << 5);   /* PA5 back to push-pull */
        GPIOA->OTYPER &= ~(1UL << 7);   /* PA7 back to push-pull */
        GPIOA->PUPDR &= ~(3UL << 10);   /* PA5 no pull */
        GPIOA->PUPDR &= ~(3UL << 14);   /* PA7 no pull */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 10)) | (2UL << 10);  /* PA5 AF */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 14)) | (2UL << 14);  /* PA7 AF */
        SPI1->CR1 |= (1UL << 6);  /* Re-enable SPE */
    }

    /* ================================================================
     * SPI LOOPBACK TEST
     * Connect MOSI (pad 2, PA7) → MISO (pad 9, PA6) with a wire.
     * The SPI peripheral sends a byte and should receive it back.
     * Press any key to skip if tile is connected instead.
     * ================================================================ */
    dbg("\r\n--- SPI Loopback Test ---\r\n");
    dbg("If MOSI(pad2) is jumpered to MISO(pad9), this tests the SPI peripheral.\r\n");
    dbg("Running loopback with 3 test patterns...\r\n");
    {
        uint8_t patterns[] = {0xA5, 0x5A, 0xFF};
        int pass = 0, fail = 0;
        for (int i = 0; i < 3; i++) {
            /* No CS needed for loopback — just clock data */
            uint8_t rx = hal_spi_transfer(&spi, patterns[i]);
            dbg("  TX=0x%02X  RX=0x%02X  %s\r\n", patterns[i], rx,
                (rx == patterns[i]) ? "PASS" : "FAIL");
            if (rx == patterns[i]) pass++; else fail++;
        }
        if (pass == 3)
            dbg("  >>> LOOPBACK PASSED — SPI peripheral works! <<<\r\n");
        else if (pass > 0)
            dbg("  >>> PARTIAL — %d/3 passed (timing marginal?) <<<\r\n", pass);
        else
            dbg("  >>> LOOPBACK FAILED — check SPI config or no jumper <<<\r\n");
        /* Flush any residual RX data */
        while (SPI1->SR & 0x01)
            (void)*(volatile uint8_t *)&SPI1->DR;
    }

    /* ---- Check SPI overrun ---- */
    dbg("\r\nSPI1 SR OVR check: 0x%02lX (bit6=OVR)\r\n",
        (unsigned long)(SPI1->SR & 0xFF));
    /* Flush RX FIFO */
    while (SPI1->SR & 0x01) {  /* RXNE */
        volatile uint8_t junk = *(volatile uint8_t *)&SPI1->DR;
        (void)junk;
        dbg("  flushed stale RX byte\r\n");
    }
    /* Clear OVR if set: read DR then SR */
    if (SPI1->SR & (1UL << 6)) {
        (void)*(volatile uint8_t *)&SPI1->DR;
        (void)SPI1->SR;
        dbg("  cleared OVR flag\r\n");
    }

    core_delay_ms(100);

    /* ================================================================
     * BIT-BANG SPI TEST
     * Bypass the SPI peripheral entirely — manually toggle GPIO.
     * This proves whether the slave responds to SPI at the electrical level.
     * ================================================================ */
    dbg("\r\n--- Bit-bang SPI test ---\r\n");
    {
        /* Disable SPI peripheral so we can use the pins as GPIO */
        SPI1->CR1 &= ~(1UL << 6);  /* Clear SPE */

        /* Reconfigure MOSI (PA7) and CLK (PA5) as GPIO output, MISO (PA6) as input */
        /* PA5: MODER = 01 (output) */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);
        /* PA7: MODER = 01 (output) */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 14)) | (1UL << 14);
        /* PA6: MODER = 00 (input) */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 12));

        /* === Try Mode 3: CLK idle HIGH === */
        GPIOA->BSRR = (1UL << 5);  /* PA5 = 1 (idle high for Mode 3) */

        /* Read MISO state before CS */
        dbg("  MISO before CS: %d\r\n", (int)((GPIOA->IDR >> 6) & 1));

        /* Assert CS */
        GPIOB->BSRR = (1UL << (0 + 16));  /* PB0 = 0 (CS low) */

        /* Small delay */
        for (volatile int d = 0; d < 100; d++);

        /* Send 0xF5 (read WHO_AM_I = 0x80 | 0x75), receive 1 byte */
        uint8_t tx_byte = 0xF5;  /* 0x80 | 0x75 */
        uint8_t rx_cmd = 0;
        uint8_t rx_data = 0;

        /* Mode 0 bit-bang — sample BOTH PA6 and PA7 to detect MOSI/MISO swap
         * If tile def has SDO/SDI reversed, data will appear on PA7 not PA6 */
        uint8_t rx_cmd_pa7 = 0;  /* read from PA7 (what we call MOSI) */
        uint8_t rx_data_pa7 = 0;

        for (int bit = 7; bit >= 0; bit--) {
            /* Set MOSI on PA7 */
            if (tx_byte & (1 << bit))
                GPIOA->BSRR = (1UL << 7);
            else
                GPIOA->BSRR = (1UL << (7 + 16));

            for (volatile int d = 0; d < 10; d++);

            /* Rising edge */
            GPIOA->BSRR = (1UL << 5);  /* CLK high */
            for (volatile int d = 0; d < 10; d++);
            uint32_t idr = GPIOA->IDR;
            rx_cmd     |= ((idr >> 6) & 1) << bit;  /* PA6 */
            rx_cmd_pa7 |= ((idr >> 7) & 1) << bit;  /* PA7 */

            /* Falling edge */
            GPIOA->BSRR = (1UL << (5 + 16));  /* CLK low */
            for (volatile int d = 0; d < 10; d++);
        }

        /* Data byte — send 0x00 on PA7, read both PA6 and PA7 */
        GPIOA->BSRR = (1UL << (7 + 16));  /* MOSI = 0 */
        for (int bit = 7; bit >= 0; bit--) {
            for (volatile int d = 0; d < 10; d++);

            GPIOA->BSRR = (1UL << 5);  /* CLK high */
            for (volatile int d = 0; d < 10; d++);
            uint32_t idr = GPIOA->IDR;
            rx_data     |= ((idr >> 6) & 1) << bit;  /* PA6 */
            rx_data_pa7 |= ((idr >> 7) & 1) << bit;  /* PA7 */

            GPIOA->BSRR = (1UL << (5 + 16));  /* CLK low */
            for (volatile int d = 0; d < 10; d++);
        }

        /* Deassert CS */
        GPIOB->BSRR = (1UL << 0);  /* PB0 = 1 (CS high) */

        dbg("  PA6 (MISO?): cmd=0x%02X data=0x%02X\r\n", rx_cmd, rx_data);
        dbg("  PA7 (MOSI?): cmd=0x%02X data=0x%02X\r\n", rx_cmd_pa7, rx_data_pa7);
        if (rx_data_pa7 == 0x44 || rx_data_pa7 == 0x47)
            dbg("  >>> MOSI/MISO ARE SWAPPED IN TILE DEF! Data on PA7! <<<\r\n");
        if (rx_data == 0x44)
            dbg("  >>> BIT-BANG WHO_AM_I = 0x44 — WIRING IS GOOD! <<<\r\n");
        else if (rx_data == 0x47)
            dbg("  >>> BIT-BANG WHO_AM_I = 0x47 — WIRING IS GOOD! <<<\r\n");
        else
            dbg("  Bit-bang also failed (0x%02X) — may be wiring/power\r\n", rx_data);

        /* Restore AF mode for SPI peripheral */
        /* PA5: MODER = 10 (AF) */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 10)) | (2UL << 10);
        /* PA6: MODER = 10 (AF) */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 12)) | (2UL << 12);
        /* PA7: MODER = 10 (AF) */
        GPIOA->MODER = (GPIOA->MODER & ~(3UL << 14)) | (2UL << 14);

        /* Re-enable SPI */
        SPI1->CR1 |= (1UL << 6);  /* SPE */
    }

    /* ================================================================
     * SPI MODE SELECTION
     *
     * The ICM-42686P powers up in I2C mode (AP_CS has internal pull-up).
     * To switch to SPI mode, we need to toggle CS. The first transaction
     * after mode switch may return garbage. InvenSense app notes recommend:
     *   1. Assert CS low briefly, then high (force mode switch)
     *   2. Wait
     *   3. First real read (may still be garbage)
     *   4. Second read should be valid
     * ================================================================ */

    dbg("\r\n--- SPI mode selection ---\r\n");

    /* Step 1: bare CS toggle to latch SPI mode */
    dbg("Step 1: CS toggle (no data)\r\n");
    core_spi_select(&spi);
    core_delay_ms(1);
    core_spi_deselect(&spi);
    core_delay_ms(1);

    /* Step 2: dummy register read (result may be garbage) */
    dbg("Step 2: dummy WHO_AM_I read\r\n");
    uint8_t dummy = spi_read_reg(ICM_WHO_AM_I);
    dbg("  dummy = 0x%02X\r\n", dummy);
    core_delay_ms(1);

    /* Step 3: real WHO_AM_I */
    dbg("Step 3: real WHO_AM_I read\r\n");
    uint8_t who = spi_read_reg(ICM_WHO_AM_I);
    dbg("  who   = 0x%02X\r\n", who);
    core_delay_ms(1);

    /* Step 4: one more for good measure */
    uint8_t who2 = spi_read_reg(ICM_WHO_AM_I);
    dbg("  who2  = 0x%02X\r\n", who2);

    /* GPIO state after SPI transactions */
    idr_a = GPIOA->IDR;
    dbg("\r\nGPIO after SPI: GPIOA IDR = 0x%04lX (PA6/MISO=%d)\r\n",
        (unsigned long)(idr_a & 0xFFFF),
        (int)((idr_a >> 6) & 1));

    /* ---- Evaluate results ---- */
    uint8_t best = who;
    if (who2 == ICM42686P_WHOAMI || who2 == ICM42688P_WHOAMI) best = who2;
    if (dummy == ICM42686P_WHOAMI || dummy == ICM42688P_WHOAMI) best = dummy;

    if (best == ICM42686P_WHOAMI || best == ICM42688P_WHOAMI) {
        dbg("\r\n>>> WHO_AM_I PASSED: 0x%02X <<<\r\n", best);
    } else {
        dbg("\r\n>>> WHO_AM_I FAILED <<<\r\n");
        dbg("All attempts returned: dummy=0x%02X, who=0x%02X, who2=0x%02X\r\n",
            dummy, who, who2);

        /* Extra diagnostics */
        dbg("\r\nDiagnostic reads:\r\n");
        for (int i = 0; i < 5; i++) {
            uint8_t r = spi_read_reg(ICM_WHO_AM_I);
            dbg("  WHO_AM_I attempt %d = 0x%02X\r\n", i, r);
            core_delay_ms(10);
        }

        /* Try reading a different register */
        uint8_t intf = spi_read_reg(ICM_INTF_CONFIG0);
        uint8_t pwr  = spi_read_reg(ICM_PWR_MGMT0);
        uint8_t bank = spi_read_reg(ICM_BANK_SEL);
        dbg("  INTF_CONFIG0(0x4C) = 0x%02X (expect 0x30)\r\n", intf);
        dbg("  PWR_MGMT0(0x4E)   = 0x%02X (expect 0x00)\r\n", pwr);
        dbg("  BANK_SEL(0x76)    = 0x%02X (expect 0x00)\r\n", bank);

        /* Try a softreset then read again */
        dbg("\r\nSoft reset...\r\n");
        spi_write_reg(ICM_DEVICE_CONFIG, 0x01);
        core_delay_ms(5);
        uint8_t after_rst = spi_read_reg(ICM_WHO_AM_I);
        dbg("  WHO_AM_I after reset = 0x%02X\r\n", after_rst);

        /* Brute force: try all 0xFF reads to check if line is stuck */
        dbg("\r\nRaw byte test (no register, just clocking MISO):\r\n");
        core_spi_select(&spi);
        dbg("  CS asserted. Clocking 8 bytes of 0x00:\r\n  ");
        for (int i = 0; i < 8; i++) {
            uint8_t r = core_spi_transfer(&spi, 0x00);
            dbg(" 0x%02X", r);
        }
        dbg("\r\n");
        core_spi_deselect(&spi);

        dbg("\r\n*** STOPPING — check wiring, power, SPI mode ***\r\n");
        while (1) {
            LED_ON(); core_delay_ms(50);
            LED_OFF(); core_delay_ms(50);
        }
    }

    /* ---- Phase 2: Soft reset ---- */
    dbg("\r\n--- Soft reset ---\r\n");
    spi_write_reg(ICM_DEVICE_CONFIG, 0x01);
    core_delay_ms(2);
    uint8_t who3 = spi_read_reg(ICM_WHO_AM_I);
    dbg("Post-reset WHO_AM_I = 0x%02X\r\n", who3);

    /* ---- Phase 3: Enable sensors ---- */
    dbg("\r\n--- Sensor data ---\r\n");
    spi_write_reg(ICM_BANK_SEL, 0x00);
    spi_write_reg(ICM_ACCEL_CONFIG0, (0x02 << 5) | 0x08);
    spi_write_reg(ICM_GYRO_CONFIG0,  (0x02 << 5) | 0x08);
    spi_write_reg(ICM_PWR_MGMT0, 0x0F);
    core_delay_ms(50);
    dbg("Accel: +/-8G, 100Hz | Gyro: +/-1000dps, 100Hz\r\n\r\n");

    uint8_t raw[14];
    for (int sample = 0; ; sample++) {
        spi_read_burst(ICM_TEMP_H, raw, 14);

        int16_t temp_raw = (int16_t)((uint16_t)raw[0] << 8 | raw[1]);
        int16_t ax = (int16_t)((uint16_t)raw[2]  << 8 | raw[3]);
        int16_t ay = (int16_t)((uint16_t)raw[4]  << 8 | raw[5]);
        int16_t az = (int16_t)((uint16_t)raw[6]  << 8 | raw[7]);
        int16_t gx = (int16_t)((uint16_t)raw[8]  << 8 | raw[9]);
        int16_t gy = (int16_t)((uint16_t)raw[10] << 8 | raw[11]);
        int16_t gz = (int16_t)((uint16_t)raw[12] << 8 | raw[13]);
        int temp_c10 = (temp_raw * 100) / 1325 + 250;

        if (sample % 10 == 0) {
            dbg("[%4d] T=%d.%dC  A=(%6d,%6d,%6d)  G=(%6d,%6d,%6d)\r\n",
                sample, temp_c10 / 10, temp_c10 % 10,
                ax, ay, az, gx, gy, gz);
        }
        LED_TOGGLE();
        core_delay_ms(100);
    }
}
