/**
 * core_debug.h — Debug output via SWO/ITM trace
 */

#ifndef CORE_DEBUG_H
#define CORE_DEBUG_H

#include "hal_debug.h"
#include "core_config.h"  /* SYSCLK_HZ */

/* ============================================================
 * Init
 * ============================================================ */

/** Initialize SWO debug output using the project's SYSCLK_HZ. */
static inline void core_debug_init(void)
{
    hal_debug_init(SYSCLK_HZ);
}

/* ============================================================
 * Output
 * ============================================================ */

/** Print a string via SWO (no formatting). */
static inline void core_debug_print(const char *str)
{
    hal_debug_puts(str);
}

/** Printf via SWO/ITM — same format as standard printf. */
#define core_debug_printf  hal_debug_printf

#endif /* CORE_DEBUG_H */
