/**
 * Power Management Test
 *
 * Tests sleep modes and wakeup sources on Core.U.2.
 * USB serial interface — type a command + Enter.
 *
 * Commands:
 *   sleep    — WFI sleep (wake on any interrupt, e.g., SysTick)
 *   stop     — Stop mode + RTC wakeup after 3 seconds
 *   stop5    — Stop mode + RTC wakeup after 5 seconds
 *   standby  — Standby mode + RTC wakeup after 5 seconds (SRAM lost, full reset)
 *   exti     — Stop mode + wake on pad 10 falling edge (touch pad 10 to GND)
 *
 * After wake from Stop, the clock is restored to 80MHz PLL and USB
 * re-enumerates. After Standby, the MCU resets completely.
 */

#include "core.h"
#include "core_power.h"
#include "core_rtc.h"
#include "core_pad.h"
#include "hal_usb_cdc.h"
#include "hal_exti.h"
#include "ll_pwr.h"
#include "ll_usb.h"

/* ---- Command handling ---- */

static char cmd[16];
static uint8_t cmd_len;

static int streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

static void do_sleep(void)
{
    hal_usb_cdc_printf("Entering Sleep mode (WFI)...\r\n");
    hal_usb_cdc_printf("SysTick will wake us in ~1ms.\r\n");
    core_delay_ms(50);  /* flush USB */

    uint32_t before = hal_tick();
    core_sleep();
    uint32_t after = hal_tick();

    hal_usb_cdc_printf("Woke from Sleep! Slept ~%lu ms\r\n\r\n", after - before);
}

static void do_stop(uint32_t seconds)
{
    hal_usb_cdc_printf("Entering Stop mode, RTC wakeup in %lu seconds...\r\n", seconds);
    hal_usb_cdc_printf("USB will disconnect. Reconnect terminal after wake.\r\n");
    core_delay_ms(100);  /* flush USB */

    /* Disconnect USB cleanly before Stop */
    ll_usb_disconnect();

    /* Enable PWR clock and backup domain for RTC */
    ll_rcc_pwr_clk_enable();
    ll_pwr_enable_backup_access();

    /* Init RTC (LSI) and set wakeup alarm */
    core_rtc_init();
    core_rtc_alarm(seconds);

    /* Configure RTC wakeup as EXTI line 20 (needed to wake from Stop) */
    /* On STM32L4, RTC wakeup is EXTI line 20, rising edge, event mode */
    SET_BITS(REG32(0x40010400UL + 0x00UL), (1UL << 20));  /* EXTI IMR1: unmask line 20 */
    SET_BITS(REG32(0x40010400UL + 0x08UL), (1UL << 20));  /* EXTI RTSR1: rising edge */

    /* Enter Stop mode */
    core_deep_sleep();

    /* --- We wake up here, running on MSI 4MHz --- */

    /* Restore 80MHz PLL + SysTick */
    core_clock_init();

    /* Re-init USB */
    hal_usb_cdc_init();

    /* LED feedback — blink fast to show we're alive */
    for (int i = 0; i < 6; i++) {
        LED_TOGGLE();
        core_delay_ms(100);
    }

    /* Wait for host to reconnect terminal */
    while (!hal_usb_cdc_connected())
        ;
    core_delay_ms(200);

    hal_usb_cdc_printf("Woke from Stop mode! Clock restored to 80MHz.\r\n\r\n");
}

static void do_standby(uint32_t seconds)
{
    hal_usb_cdc_printf("Entering Standby mode, RTC wakeup in %lu seconds...\r\n", seconds);
    hal_usb_cdc_printf("SRAM will be lost. MCU will reset on wake.\r\n");
    core_delay_ms(100);

    ll_usb_disconnect();

    ll_rcc_pwr_clk_enable();
    ll_pwr_enable_backup_access();

    core_rtc_init();
    core_rtc_alarm(seconds);

    /* RTC wakeup EXTI line 20 */
    SET_BITS(REG32(0x40010400UL + 0x00UL), (1UL << 20));
    SET_BITS(REG32(0x40010400UL + 0x08UL), (1UL << 20));

    core_shutdown();
    /* Never returns — MCU resets on wake */
}

static volatile int exti_woke;

static void exti_cb(void *ctx)
{
    (void)ctx;
    exti_woke = 1;
}

static void do_exti_stop(void)
{
    hal_usb_cdc_printf("Entering Stop mode, wake on pad 10 falling edge...\r\n");
    hal_usb_cdc_printf("Touch pad 10 to GND to wake. USB will disconnect.\r\n");
    core_delay_ms(100);

    ll_usb_disconnect();

    /* Configure pad 10 (PA5) as input with pull-up, EXTI on falling edge */
    exti_woke = 0;
    core_pad_input(10, PULL_UP);
    hal_exti_enable(10, HAL_EXTI_FALLING, exti_cb, NULL);

    /* Enter Stop mode */
    core_deep_sleep();

    /* --- Woke up --- */
    core_clock_init();
    hal_usb_cdc_init();

    for (int i = 0; i < 6; i++) {
        LED_TOGGLE();
        core_delay_ms(100);
    }

    while (!hal_usb_cdc_connected())
        ;
    core_delay_ms(200);

    hal_usb_cdc_printf("Woke from Stop via EXTI on pad 10!\r\n\r\n");
}

static void check_serial(void)
{
    uint8_t byte;
    while (hal_usb_cdc_rx_try(&byte)) {
        if (byte == '\r' || byte == '\n') {
            if (cmd_len > 0) {
                cmd[cmd_len] = '\0';

                if (streq(cmd, "sleep"))        do_sleep();
                else if (streq(cmd, "stop"))    do_stop(3);
                else if (streq(cmd, "stop5"))   do_stop(5);
                else if (streq(cmd, "standby")) do_standby(5);
                else if (streq(cmd, "exti"))    do_exti_stop();
                else if (streq(cmd, "wkup")) {
                    hal_usb_cdc_printf("Entering Standby, wake on pad 8 falling edge...\r\n");
                    hal_usb_cdc_printf("Touch pad 8 to GND to wake (full reset).\r\n");
                    core_delay_ms(100);
                    ll_usb_disconnect();
                    core_standby_until_on_change(8, EDGE_FALLING);
                }
                else {
                    hal_usb_cdc_printf("Unknown: %s\r\n", cmd);
                    hal_usb_cdc_printf("Commands: sleep, stop, stop5, standby, exti, wkup\r\n");
                }
                cmd_len = 0;
            }
        } else if (cmd_len < sizeof(cmd) - 1) {
            cmd[cmd_len++] = (char)byte;
        }
    }
}

/* ---- Main ---- */

int main(void)
{
    core_init();
    core_led_init();
    hal_usb_cdc_init();

    /* Check if we woke from standby */
    if (core_woke_from_standby()) {
        core_clear_standby_flag();
        /* Wait for USB to enumerate and terminal to connect */
        while (!hal_usb_cdc_connected())
            ;
        core_delay_ms(200);
        hal_usb_cdc_printf("*** Woke from Standby! (full reset) ***\r\n\r\n");
    }

    while (!hal_usb_cdc_connected())
        ;
    core_delay_ms(100);

    hal_usb_cdc_printf("=== Power Management Test ===\r\n");
    hal_usb_cdc_printf("Commands: sleep, stop, stop5, standby, exti\r\n\r\n");

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);
        check_serial();
    }
}
