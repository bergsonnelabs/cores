/*
 * Minimal PAL stubs for WAMR footprint measurement.
 *
 * These exist so objects link into a static archive without undefined
 * references. Real implementations will come later when we wire WAMR into
 * the firmware. For size measurement, the code they contain is tiny and
 * dominated by whatever WAMR itself requires.
 */

#include "platform_api_vmcore.h"

/* ---- init / destroy ---- */
int  bh_platform_init(void)    { return 0; }
void bh_platform_destroy(void) {}

/* ---- memory allocator ----
 * We forward to newlib's malloc (which the firmware will provide via
 * _sbrk once linked). For size measurement these are just tiny wrappers. */
void *os_malloc(unsigned size)              { return malloc(size); }
void *os_realloc(void *ptr, unsigned size)  { return realloc(ptr, size); }
void  os_free(void *ptr)                    { free(ptr); }

/* ---- printf / vprintf ---- */
int os_printf(const char *format, ...) {
    va_list ap;
    int n;
    va_start(ap, format);
    n = vprintf(format, ap);
    va_end(ap);
    return n;
}

int os_vprintf(const char *format, va_list ap) { return vprintf(format, ap); }

/* ---- time ----
 * Real impl will hook to SysTick / DWT cycle counter.
 * Stub returns 0 for now. */
uint64 os_time_get_boot_us(void)       { return 0; }
uint64 os_time_thread_cputime_us(void) { return 0; }

/* ---- threading ----
 * Single-threaded target, so these are no-ops / identity. */
korp_tid os_self_thread(void) {
    static korp_thread self = 0;
    return &self;
}

uint8 *os_thread_get_stack_boundary(void) { return NULL; }

void os_thread_jit_write_protect_np(bool enabled) { (void)enabled; }

/* ---- mutex: single-threaded no-ops ---- */
int os_mutex_init(korp_mutex *m)    { (void)m; return 0; }
int os_mutex_destroy(korp_mutex *m) { (void)m; return 0; }
int os_mutex_lock(korp_mutex *m)    { (void)m; return 0; }
int os_mutex_unlock(korp_mutex *m)  { (void)m; return 0; }

/* ---- mmap / mprotect ----
 * AOT loader uses these to stage the module into writable/executable memory.
 * Real impl (XIP or static pool) comes later. */
void *os_mmap(void *hint, size_t size, int prot, int flags, os_file_handle file) {
    (void)hint; (void)prot; (void)flags; (void)file;
    return malloc(size);
}

void os_munmap(void *addr, size_t size) {
    (void)size;
    free(addr);
}

int os_mprotect(void *addr, size_t size, int prot) {
    (void)addr; (void)size; (void)prot;
    return 0;
}

void *os_mremap(void *old_addr, size_t old_size, size_t new_size) {
    void *p = realloc(old_addr, new_size);
    (void)old_size;
    return p;
}

/* ---- cache flush ----
 * Real impl on Cortex-M: DSB + ISB barriers. Stub no-op for measurement. */
void os_dcache_flush(void) {}
void os_icache_flush(void *start, size_t len) { (void)start; (void)len; }

/* wasm_c_api stubs live in the host firmware main.c, not here, so they stay
 * outside the WAMR archive and play nicely with LTO. */
