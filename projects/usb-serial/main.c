/**
 * USB debug test — ALL registers accessed as 16-bit
 *
 * Scope on Pad 8 (CH1) and Pad 6 (CH2):
 *   After USB reset: both HIGH = STAT_RX VALID
 *   CH1 pulse LOW = CTR_RX fired
 *   CH2 goes LOW = SETUP received
 *
 * LED: 1 slow = no resets, 2 = resets no SETUP, 3 = got SETUP!
 */

#include "tile_init.h"
#include "tile_board.h"
#include "tile_config.h"
#include "ll_rcc.h"
#include "ll_gpio.h"
#include "ll_systick.h"
#include "ll_crs.h"
#include "hal_common.h"

/* ---- ALL USB registers as 16-bit ---- */
#define USB  0x40006800UL
#define PMA  0x40006C00UL

#define EP0R    (*(volatile uint16_t *)(USB + 0x00))
#define CNTR    (*(volatile uint16_t *)(USB + 0x40))
#define ISTR    (*(volatile uint16_t *)(USB + 0x44))
#define FNR     (*(volatile uint16_t *)(USB + 0x48))
#define DADDR   (*(volatile uint16_t *)(USB + 0x4C))
#define BTABLE  (*(volatile uint16_t *)(USB + 0x50))
#define BCDR    (*(volatile uint16_t *)(USB + 0x58))

/* Bit definitions */
#define CNTR_FRES    (1U << 0)
#define CNTR_RESETM  (1U << 10)
#define CNTR_CTRM    (1U << 15)
#define ISTR_RESET   (1U << 10)
#define ISTR_CTR     (1U << 15)
#define DADDR_EF     (1U << 7)
#define BCDR_DPPU    (1U << 15)
#define EP_CTR_RX    (1U << 15)
#define EP_SETUP     (1U << 11)
#define EP_CONTROL   (1U << 9)
#define EP_CTR_TX    (1U << 7)

/* Debug output pads */
#define DBG1_PORT  GPIOA
#define DBG1_MASK  (1UL << 0)   /* Pad 8 */
#define DBG2_PORT  GPIOA
#define DBG2_MASK  (1UL << 3)   /* Pad 6 */

static volatile uint8_t got_reset = 0;
static volatile uint8_t got_setup = 0;

void USB_IRQHandler(void)
{
    uint16_t istr = ISTR;

    if (istr & ISTR_RESET) {
        ISTR = (uint16_t)~ISTR_RESET;
        got_reset = 1;

        BTABLE = 0;

        /* BDT for EP0: no stride, no shift (variant #4) */
        *(volatile uint16_t *)(PMA + 0) = 0x58;    /* ADDR_TX */
        *(volatile uint16_t *)(PMA + 2) = 0;       /* COUNT_TX */
        *(volatile uint16_t *)(PMA + 4) = 0x18;    /* ADDR_RX */
        *(volatile uint16_t *)(PMA + 6) = 0x8400;  /* COUNT_RX */

        /* Set type=CONTROL, preserve CTR bits */
        EP0R = (uint16_t)(EP_CONTROL | EP_CTR_RX | EP_CTR_TX);

        /* Toggle STAT_TX→NAK, STAT_RX→VALID from current state */
        {
            uint16_t r = EP0R;
            uint16_t tog_tx = (((r >> 4) & 3) ^ 2) << 4;   /* NAK=2 */
            uint16_t tog_rx = (((r >> 12) & 3) ^ 3) << 12;  /* VALID=3 */
            EP0R = (uint16_t)((r & 0x070F) | EP_CTR_RX | EP_CTR_TX
                              | tog_tx | tog_rx);
        }

        DADDR = DADDR_EF;
        CNTR = CNTR_CTRM | CNTR_RESETM;

        /* Read back and output STAT_RX to scope */
        {
            uint16_t r = EP0R;
            if ((r >> 12) & 1) ll_gpio_set(DBG1_PORT, DBG1_MASK);
            else               ll_gpio_clear(DBG1_PORT, DBG1_MASK);
            if ((r >> 13) & 1) ll_gpio_set(DBG2_PORT, DBG2_MASK);
            else               ll_gpio_clear(DBG2_PORT, DBG2_MASK);
        }
        return;
    }

    if (istr & ISTR_CTR) {
        uint16_t ep0r = EP0R;
        if (ep0r & EP_CTR_RX) {
            EP0R = (uint16_t)((ep0r & 0x070F) | EP_CTR_TX);

            ll_gpio_clear(DBG1_PORT, DBG1_MASK);
            for (volatile int i = 0; i < 10; i++) ;
            ll_gpio_set(DBG1_PORT, DBG1_MASK);

            if (ep0r & EP_SETUP) {
                got_setup = 1;
                ll_gpio_clear(DBG2_PORT, DBG2_MASK);
            }

            /* Re-arm RX */
            {
                uint16_t r = EP0R;
                uint16_t tog = (((r >> 12) & 3) ^ 3) << 12;
                EP0R = (uint16_t)((r & 0x070F) | EP_CTR_RX | EP_CTR_TX | tog);
            }
        }
        if (ep0r & EP_CTR_TX) {
            EP0R = (uint16_t)((ep0r & 0x070F) | EP_CTR_RX);
        }
    }

    ISTR = 0;
}

static void blink_n(int n, int ms)
{
    for (int i = 0; i < n; i++) {
        LED_ON();  ll_delay_ms(ms);
        LED_OFF(); ll_delay_ms(ms);
    }
}

int main(void)
{
    tile_init();
    ll_systick_init(SYSCLK_HZ);

    ll_rcc_gpio_clk_enable(LED_PORT);
    ll_gpio_config_output(LED_PORT, LED_PIN);

    ll_rcc_gpio_clk_enable(GPIOA);
    ll_rcc_gpio_clk_enable(GPIOB);
    ll_gpio_config_output(DBG1_PORT, 0);
    ll_gpio_config_output(DBG2_PORT, 3);

    /* Scope verify */
    for (int i = 0; i < 3; i++) {
        LED_ON();
        ll_gpio_set(DBG1_PORT, DBG1_MASK);
        ll_gpio_set(DBG2_PORT, DBG2_MASK);
        ll_delay_ms(200);
        LED_OFF();
        ll_gpio_clear(DBG1_PORT, DBG1_MASK);
        ll_gpio_clear(DBG2_PORT, DBG2_MASK);
        ll_delay_ms(200);
    }

    /* GPIO: PA11=AF10, PA12=AF10 — required for USB detection */
    ll_gpio_config_af(GPIOA, 11, 10, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_NONE);
    ll_gpio_config_af(GPIOA, 12, 10, LL_GPIO_OTYPE_PP, LL_GPIO_SPEED_VHIGH, LL_GPIO_PULL_NONE);

    /* HSI48 + CRS */
    ll_rcc_hsi48_enable();
    while (!ll_rcc_hsi48_ready()) ;
    ll_rcc_set_usb_clk_source(LL_RCC_USB48_HSI48);
    ll_rcc_apb1_clk_enable(LL_APB1_CRS);
    ll_crs_usb_sync_enable();

    /* VDDUSB */
    ll_rcc_apb1_clk_enable(1UL << 28);
    SET_BITS(REG32(0x40007004UL), (1UL << 10));

    /* USB peripheral */
    ll_rcc_apb1_clk_enable(LL_APB1_USB);
    CNTR = CNTR_FRES;
    for (volatile int i = 0; i < 100; i++) ;
    CNTR = 0;
    ISTR = 0;
    BTABLE = 0;

    CNTR = CNTR_RESETM;

    hal_nvic_set_priority(HAL_IRQ_USB, 0);
    hal_nvic_enable_irq(HAL_IRQ_USB);

    BCDR |= BCDR_DPPU;

    blink_n(3, 100);
    ll_delay_ms(5000);

    while (1) {
        ll_delay_ms(2000);
        if (got_setup)
            blink_n(3, 500);
        else if (got_reset)
            blink_n(2, 500);
        else
            blink_n(1, 500);
    }
}
