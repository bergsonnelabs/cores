/**
 * hal_dfu.h — Reboot into DFU bootloader
 *
 * Call hal_dfu_reboot() from application code to trigger a firmware
 * update via the custom DFU bootloader. Writes a magic value to a
 * reserved RAM address and performs a system reset. The bootloader
 * checks this address on boot and enters USB DFU mode.
 *
 * The magic lives in the last 16 bytes of SRAM, which is reserved
 * by the linker scripts (both bootloader.ld and stm32l422tb_app.ld
 * use LENGTH = 40K - 16). The startup code doesn't touch this area.
 *
 * Typical usage: wire this into a USB CDC command handler so users
 * can trigger a firmware update over serial.
 */

#ifndef HAL_DFU_H
#define HAL_DFU_H

#include "ll_common.h"

/* Magic value and its fixed RAM address (top of SRAM, outside linker regions) */
#define DFU_MAGIC           0xDEADBEEFUL
#define DFU_MAGIC_ADDR      (*(volatile uint32_t *)0x20009FF0UL)

/* SCB AIRCR: Application Interrupt and Reset Control Register */
#define SCB_AIRCR           REG32(0xE000ED0CUL)
#define SCB_AIRCR_SYSRESETREQ  (1UL << 2)
#define SCB_AIRCR_VECTKEY      (0x05FAUL << 16)

/**
 * Reboot into DFU mode. Does not return.
 *
 * Writes the DFU magic to a reserved SRAM address, then triggers
 * a system reset. SRAM survives system reset, so the bootloader
 * will see the magic and enumerate as a USB DFU device.
 */
static inline void hal_dfu_reboot(void) __attribute__((noreturn));
static inline void hal_dfu_reboot(void)
{
    DFU_MAGIC_ADDR = DFU_MAGIC;

    __asm volatile ("dsb" ::: "memory");

    /* Request system reset */
    SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;

    __asm volatile ("dsb" ::: "memory");
    while (1)
        ;
}

#endif /* HAL_DFU_H */
