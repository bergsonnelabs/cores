/**
 * hal_debug.c — Debug output via SWO/ITM
 */

#include "hal_debug.h"
#include <stdio.h>
#include <stdarg.h>

#if !defined(STM32L011xx)
#include "ll_itm.h"
#endif

#ifndef HAL_DEBUG_BUF_SIZE
  #define HAL_DEBUG_BUF_SIZE 256
#endif

void hal_debug_init(uint32_t sysclk_hz)
{
#if !defined(STM32L011xx)
    ll_itm_init_default(sysclk_hz);
#else
    (void)sysclk_hz;
#endif
}

int hal_debug_printf(const char *fmt, ...)
{
#if !defined(STM32L011xx)
    char buf[HAL_DEBUG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len > (int)sizeof(buf) - 1)
            len = (int)sizeof(buf) - 1;
        ll_itm_write((const uint8_t *)buf, (uint32_t)len);
    }
    return len;
#else
    (void)fmt;
    return 0;
#endif
}

void hal_debug_puts(const char *str)
{
#if !defined(STM32L011xx)
    ll_itm_puts(str);
#else
    (void)str;
#endif
}

void hal_debug_putc(char c)
{
#if !defined(STM32L011xx)
    ll_itm_putc(c);
#else
    (void)c;
#endif
}
