/**
 * utilities_conf.h — Configuration for ST utilities (sequencer, etc.)
 *
 * Provides the critical section macros and memory utilities needed
 * by stm32_seq.c without depending on the ST HAL.
 */

#ifndef UTILITIES_CONF_H
#define UTILITIES_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_conf.h"
#include <stdint.h>
#include <string.h>

/* ============================================================
 * Critical section macros (PRIMASK-based)
 * ============================================================ */

#define UTIL_SEQ_INIT_CRITICAL_SECTION()

#define UTIL_SEQ_ENTER_CRITICAL_SECTION() \
    uint32_t primask_bit; \
    __asm volatile ("MRS %0, primask" : "=r" (primask_bit)); \
    __asm volatile ("cpsid i" ::: "memory")

#define UTIL_SEQ_EXIT_CRITICAL_SECTION() \
    __asm volatile ("MSR primask, %0" :: "r" (primask_bit) : "memory")

/* ============================================================
 * Memory utilities
 * ============================================================ */

#define UTIL_SEQ_MEMSET8(dest, value, size)  memset((dest), (value), (size))

/* ============================================================
 * Placement / alignment macros (used by some ST utilities)
 * ============================================================ */

#define UTIL_PLACE_IN_SECTION(__x__)  __attribute__((section(__x__)))

#ifndef ALIGN
#define ALIGN(n)  __attribute__((aligned(n)))
#endif

#ifdef __cplusplus
}
#endif

#endif /* UTILITIES_CONF_H */
