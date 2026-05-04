/**
 * core_gpio.h — GPIO subsystem: pad I/O, edge interrupts, and read/write
 *
 * Single include for all GPIO functionality:
 *   - core_pad_output(), core_pad_input(), core_pad_read(), core_pad_write()
 *   - core_pad_on_change() for edge-triggered interrupts (EXTI)
 *   - Pull, speed, and output-type aliases
 *
 * Typical usage:
 *
 *   #include "core_gpio.h"
 *
 *   core_pad_output(3);
 *   core_pad_write(3, ON);
 *
 *   core_pad_on_change(5, EDGE_FALLING, on_button, NULL);
 *
 * (No @studio coverage block — this is a re-export of core_pad.h, which
 * carries the GPIO entry on the SDK Coverage Table.)
 */

#ifndef CORE_GPIO_H
#define CORE_GPIO_H

#include "core_pad.h"

#endif /* CORE_GPIO_H */
