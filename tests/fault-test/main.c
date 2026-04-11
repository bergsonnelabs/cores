/**
 * Fault Handler Test
 *
 * USB serial interface that triggers various faults on command.
 * Verifies that hal_fault.c catches them, dumps registers over
 * USB CDC, and blinks SOS.
 *
 * Commands (type + Enter):
 *   hard    — trigger HardFault (call to invalid address)
 *   bus     — trigger BusFault (read from invalid peripheral address)
 *   usage   — trigger UsageFault (undefined instruction)
 *   div0    — trigger UsageFault (divide by zero, after enabling trap)
 *   null    — dereference null pointer
 */

#include "core.h"
#include "hal_usb_cdc.h"
#include "hal_fault.h"

/* ---- Fault triggers ---- */

static void trigger_hardfault(void)
{
    /* Call an invalid function pointer */
    void (*bad_func)(void) = (void (*)(void))0xDEADDEAD;
    bad_func();
}

static void trigger_busfault(void)
{
    /* Enable BusFault handler (SCB SHCSR bit 17) so it doesn't
     * escalate to HardFault */
    SET_BITS(REG32(0xE000ED24UL), (1UL << 17));

    /* Read from an invalid peripheral address */
    volatile uint32_t val = *(volatile uint32_t *)0x60000000UL;
    (void)val;
}

static void trigger_usagefault(void)
{
    /* Enable UsageFault handler (SCB SHCSR bit 18) */
    SET_BITS(REG32(0xE000ED24UL), (1UL << 18));

    /* Execute an undefined instruction */
    __asm volatile (".word 0xDEAD");
}

static void trigger_divzero(void)
{
    /* Enable UsageFault handler + DIV_0_TRP (CCR bit 4) */
    SET_BITS(REG32(0xE000ED24UL), (1UL << 18));
    SET_BITS(REG32(0xE000ED14UL), (1UL << 4));

    /* Divide by zero */
    volatile int a = 1;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

static void trigger_nullptr(void)
{
    /* Dereference null pointer */
    volatile uint32_t val = *(volatile uint32_t *)0x00000000UL;
    (void)val;
}

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

static void check_serial(void)
{
    uint8_t byte;
    while (hal_usb_cdc_rx_try(&byte)) {
        if (byte == '\r' || byte == '\n') {
            if (cmd_len > 0) {
                cmd[cmd_len] = '\0';

                if (streq(cmd, "hard"))       trigger_hardfault();
                else if (streq(cmd, "bus"))   trigger_busfault();
                else if (streq(cmd, "usage")) trigger_usagefault();
                else if (streq(cmd, "div0"))  trigger_divzero();
                else if (streq(cmd, "null"))  trigger_nullptr();
                else {
                    hal_usb_cdc_printf("Unknown command: %s\r\n", cmd);
                    hal_usb_cdc_printf("Commands: hard, bus, usage, div0, null\r\n");
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

    /* Wait for USB connection */
    while (!hal_usb_cdc_connected())
        ;
    core_delay_ms(100);

    hal_usb_cdc_printf("=== Fault Handler Test ===\r\n");
    hal_usb_cdc_printf("Commands: hard, bus, usage, div0, null\r\n\r\n");

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);
        check_serial();
    }
}
