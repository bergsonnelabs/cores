/*
 * Minimal bare-metal platform_internal.h for WAMR footprint measurement.
 *
 * Satisfies the types and macros WAMR expects without pulling in an OS.
 * Good enough for compile-only measurement on Cortex-M4 bare metal.
 */

#ifndef _PLATFORM_INTERNAL_H
#define _PLATFORM_INTERNAL_H

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#ifndef BH_PLATFORM_BARE_METAL
#define BH_PLATFORM_BARE_METAL
#endif

/* Single-thread stubs: the runtime compiles out thread usage when we don't
 * enable pthread / shared-memory, but the core still references these types
 * via `korp_*` typedefs, so we provide opaque aliases. */
typedef int          korp_thread;
typedef korp_thread *korp_tid;
typedef int          korp_mutex;
typedef int          korp_sem;
typedef int          korp_cond;
typedef int          korp_rwlock;

/* AOT load needs os_file_handle; we stub it as int. */
typedef int   os_file_handle;
typedef void *os_dir_stream;
typedef int   os_raw_file_handle;

static inline os_file_handle os_get_invalid_handle(void) { return -1; }
static inline int            os_getpagesize(void)        { return 4096; }

#define BH_APPLET_PRESERVED_STACK_SIZE (2 * 1024)
#define BH_THREAD_DEFAULT_PRIORITY     0

#endif /* _PLATFORM_INTERNAL_H */
