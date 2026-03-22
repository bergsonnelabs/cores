/**
 * ble_irq.c — BLE radio interrupt handlers for STM32WBA55
 *
 * The BLE link layer binary installs callbacks via ll_sys_setup_radio_intr()
 * and ll_sys_setup_radio_sw_low_intr(). These IRQ handlers forward to those
 * callbacks.
 *
 * RADIO_IRQHandler (IRQ 66) — high-priority radio ISR
 * HASH_IRQHandler  (IRQ 61) — repurposed as software low-priority radio ISR
 */

#include <stdint.h>
#include <stddef.h>
#include "hal_common.h"

/* Callbacks installed by the link layer via ll_sys_setup_radio_intr/sw_low_intr */
void (*radio_callback)(void) = NULL;
void (*low_isr_callback)(void) = NULL;

volatile uint8_t radio_sw_low_isr_is_running_high_prio = 0;

/* RCC register for radio sleep timer clock */
#define RCC_BASE            0x46020C00UL
#define RCC_RADIOENR        (*(volatile uint32_t *)(RCC_BASE + 0x0A8))
#define RCC_RADIOENR_STIMEN (1U << 0)

void RADIO_IRQHandler(void)
{
    if (radio_callback != NULL)
    {
        radio_callback();
    }

    /* Disable radio sleep timer clock after ISR */
    RCC_RADIOENR &= ~RCC_RADIOENR_STIMEN;
    __asm volatile ("isb");
}

void HASH_IRQHandler(void)
{
    /* Disable SW radio low interrupt to prevent nested calls */
    hal_nvic_disable_irq(HAL_IRQ_HASH);

    if (low_isr_callback != NULL)
    {
        low_isr_callback();
    }

    /* Check if nested SW radio low interrupt has been requested */
    if (radio_sw_low_isr_is_running_high_prio != 0)
    {
        hal_nvic_set_priority(HAL_IRQ_HASH, 5);
        radio_sw_low_isr_is_running_high_prio = 0;
    }

    hal_nvic_enable_irq(HAL_IRQ_HASH);
}
