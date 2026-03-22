/**
 * hal_systick.c — SysTick interrupt handler
 *
 * Provides the SysTick_Handler that increments the global
 * _systick_ticks counter used by ll_delay_ms() and hal_tick().
 */

#include <stdint.h>

/* Global tick counter — declared extern in ll_systick.h and hal_common.h */
volatile uint32_t _systick_ticks;

void SysTick_Handler(void)
{
    _systick_ticks++;
}
