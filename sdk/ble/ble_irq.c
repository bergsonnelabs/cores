/**
 * ble_irq.c — BLE radio interrupt handlers for STM32WBA55
 *
 * RADIO_IRQHandler (IRQ 66) — high-priority radio ISR
 * HASH_IRQHandler  (IRQ 61) — repurposed as software low-priority radio ISR
 */

#include <stdint.h>
#include <stddef.h>
#include "hal_common.h"

/* Callbacks installed by the link layer — defined here, extern'd elsewhere */
void (*radio_callback)(void) = NULL;
void (*low_isr_callback)(void) = NULL;
volatile uint8_t radio_sw_low_isr_is_running_high_prio = 0;

/* Debug counter */
volatile uint32_t dbg_radio_irq_count = 0;

void RADIO_IRQHandler(void)
{
    dbg_radio_irq_count++;

    if (radio_callback != NULL)
    {
        radio_callback();
    }
}

void HASH_IRQHandler(void)
{
    hal_nvic_disable_irq(HAL_IRQ_HASH);

    if (low_isr_callback != NULL)
    {
        low_isr_callback();
    }

    if (radio_sw_low_isr_is_running_high_prio != 0)
    {
        hal_nvic_set_priority(HAL_IRQ_HASH, 5);
        radio_sw_low_isr_is_running_high_prio = 0;
    }

    hal_nvic_enable_irq(HAL_IRQ_HASH);
}
