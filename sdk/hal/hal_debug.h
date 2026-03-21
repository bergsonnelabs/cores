/**
 * hal_debug.h — Debug output via SWO/ITM
 *
 * Printf-style debug output through the SWD trace pin.
 * No UART adapter needed — uses the existing debug probe cable.
 *
 * Usage:
 *   hal_debug_init(SYSCLK_HZ);
 *   hal_debug_printf("Hello from Core.U.2!\r\n");
 *   hal_debug_printf("ADC = %d mV\r\n", voltage);
 *
 * Capture with:
 *   make swo         (opens OpenOCD and prints SWO output)
 */

#ifndef HAL_DEBUG_H
#define HAL_DEBUG_H

#include "hal_common.h"

/**
 * Initialize SWO debug output.
 *   sysclk_hz: system clock frequency
 */
void hal_debug_init(uint32_t sysclk_hz);

/**
 * Printf via SWO/ITM. Same format as standard printf.
 * On Cortex-M0+ (Core.L), this is a no-op.
 */
int hal_debug_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * Print a string via SWO (no formatting).
 */
void hal_debug_puts(const char *str);

/**
 * Print a single character via SWO.
 */
void hal_debug_putc(char c);

#endif /* HAL_DEBUG_H */
