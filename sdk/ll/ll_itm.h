/**
 * ll_itm.h — ITM (Instrumentation Trace Macrocell) for SWO printf
 *
 * Sends debug text over the SWD trace pin (SWO) to the debug
 * probe. No UART adapter needed — uses the same cable you flash with.
 *
 * Available on Cortex-M3, M4, M33 (Core.U, Core.W, Core.H).
 * NOT available on Cortex-M0+ (Core.L — no ITM hardware).
 *
 * Usage:
 *   ll_itm_init(80000000);       // Call once with SYSCLK
 *   ll_itm_putc('H');            // Send a character
 *   ll_itm_puts("Hello\r\n");   // Send a string
 *
 * To capture output, run OpenOCD with SWO enabled, or use
 * STM32CubeIDE's SWV console.
 */

#ifndef LL_ITM_H
#define LL_ITM_H

#include "ll_common.h"

#if !defined(STM32L011xx)  /* M0+ has no ITM */

/* ---- Core Debug / ITM registers ---- */

#define ITM_BASE            0xE0000000UL
#define ITM_STIM0           REG32(ITM_BASE + 0x000UL)  /* Stimulus port 0 */
#define ITM_TER             REG32(ITM_BASE + 0xE00UL)  /* Trace enable */
#define ITM_TCR             REG32(ITM_BASE + 0xE80UL)  /* Trace control */
#define ITM_LAR             REG32(ITM_BASE + 0xFB0UL)  /* Lock access */

#define TPIU_BASE           0xE0040000UL
#define TPIU_ACPR           REG32(TPIU_BASE + 0x010UL) /* Async clock prescaler */
#define TPIU_SPPR           REG32(TPIU_BASE + 0x0F0UL) /* Selected pin protocol */
#define TPIU_FFCR           REG32(TPIU_BASE + 0x304UL) /* Formatter and flush */

#define DBGMCU_BASE         0xE0042000UL
#define DBGMCU_CR           REG32(DBGMCU_BASE + 0x04UL)

/* ============================================================
 * Initialization
 * ============================================================ */

/**
 * Initialize ITM for SWO printf output.
 *   sysclk_hz:  system clock frequency (e.g., 80000000)
 *   swo_baud:   desired SWO baud rate (2000000 is typical)
 *
 * The SWO pin (TRACESWO) must be available — on Core.U.2 this
 * is PB3 (Pad 12), but it's directly connected to the ST-Link
 * trace input, so no GPIO config is needed.
 *
 * Note: The debugger (OpenOCD / ST-Link) must also be configured
 * to capture SWO at the matching baud rate.
 */
static inline void ll_itm_init(uint32_t sysclk_hz, uint32_t swo_baud)
{
    /* Enable trace output in DBGMCU */
    SET_BITS(DBGMCU_CR, (1UL << 5));  /* TRACE_IOEN */

    /* Unlock ITM */
    ITM_LAR = 0xC5ACCE55UL;

    /* Configure TPIU */
    TPIU_ACPR = (sysclk_hz / swo_baud) - 1;  /* Async clock prescaler */
    TPIU_SPPR = 2;                             /* NRZ (UART-like) protocol */
    TPIU_FFCR = 0x100;                        /* Continuous formatting */

    /* Enable ITM */
    ITM_TCR = (1UL << 0)    /* ITMENA: enable ITM */
            | (1UL << 3)    /* TXENA: enable hardware event forwarding */
            | (1UL << 16);  /* TraceBusID = 1 */

    /* Enable stimulus port 0 */
    ITM_TER = 1;
}

/**
 * Convenience: init with default 2MHz SWO baud rate.
 */
static inline void ll_itm_init_default(uint32_t sysclk_hz)
{
    ll_itm_init(sysclk_hz, 2000000);
}

/* ============================================================
 * Output
 * ============================================================ */

/**
 * Send a single character via ITM stimulus port 0.
 * Blocks if the port is busy (very briefly — SWO runs at MHz).
 */
static inline void ll_itm_putc(char c)
{
    /* Wait for stimulus port 0 to be ready (bit 0 of STIM0) */
    while (!(ITM_STIM0 & 1))
        ;
    /* Write character (only the low byte matters) */
    *(volatile uint8_t *)&ITM_STIM0 = (uint8_t)c;
}

/** Send a null-terminated string via ITM. */
static inline void ll_itm_puts(const char *str)
{
    while (*str) {
        ll_itm_putc(*str++);
    }
}

/** Send a buffer of bytes via ITM. */
static inline void ll_itm_write(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        ll_itm_putc((char)data[i]);
    }
}

#endif /* !STM32L011xx */
#endif /* LL_ITM_H */
