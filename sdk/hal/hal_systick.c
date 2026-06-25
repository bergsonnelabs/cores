/**
 * hal_systick.c — SysTick interrupt handler
 *
 * Provides the SysTick_Handler that increments the global
 * _systick_ticks counter used by ll_delay_ms() and hal_tick().
 */

#include <stdint.h>

/* Global tick counter — declared extern in ll_systick.h and hal_common.h */
volatile uint32_t _systick_ticks;

/* Ticks per microsecond — set by ll_systick_init(), declared extern in ll_systick.h */
uint32_t _systick_us_ticks;

/* Per-millisecond hook. core_led.c provides the strong definition that
 * services the free-running LED heartbeat; this weak no-op stands in when
 * core_led isn't linked, keeping the HAL independent of the Core layer. */
__attribute__((weak)) void core_led_systick_tick(void);
__attribute__((weak)) void core_led_systick_tick(void) {}

void SysTick_Handler(void)
{
    _systick_ticks++;
    core_led_systick_tick();
}
