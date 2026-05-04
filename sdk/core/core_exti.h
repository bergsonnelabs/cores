/**
 * core_exti.h — Backward compatibility shim
 *
 * EXTI functionality has moved into core_pad.h (as core_pad_on_change).
 * Include "core_gpio.h" or "core_pad.h" instead.
 *
 * (No @studio coverage block — this is a re-export of core_pad.h, which
 * carries the GPIO entry on the SDK Coverage Table.)
 */

#ifndef CORE_EXTI_H
#define CORE_EXTI_H

#include "core_pad.h"

#endif /* CORE_EXTI_H */
