/**
 * hal_dfu.h — Reboot into DFU bootloader
 *
 * Two DFU modes are supported:
 *
 * 1. Custom bootloader (BOOTLOADER=1):
 *    App at 0x08002000, custom DFU 1.1 bootloader at 0x08000000.
 *    hal_dfu_reboot() writes magic → reset → bootloader enters DFU.
 *
 * 2. ROM bootloader (ROM_DFU=1):
 *    App at 0x08000000 (no custom bootloader needed).
 *    hal_dfu_reboot() writes magic → reset → core_init() detects magic
 *    → hal_dfu_jump_to_rom() remaps system memory and jumps to the
 *    factory-programmed ST ROM bootloader (DfuSe protocol, 0483:DF11).
 *
 * The magic lives in the last 16 bytes of SRAM, which is reserved
 * by the linker scripts (LENGTH = <total> - 16). The startup code
 * doesn't touch this area, and SRAM survives system reset.
 *
 * Typical trigger: 1200-baud touch on USB CDC (detected in
 * hal_usb_cdc.c) calls hal_dfu_reboot().
 */

#ifndef HAL_DFU_H
#define HAL_DFU_H

#include "ll_common.h"

/* Magic value and its fixed RAM address (top of SRAM, outside linker regions).
 * The linker scripts reserve the last 16 bytes of SRAM for this. */
#define DFU_MAGIC           0xDEADBEEFUL

#if defined(STM32L422xx)
  /* L422: 40KB SRAM, top = 0x2000A000 */
  #define DFU_MAGIC_ADDR    (*(volatile uint32_t *)0x20009FF0UL)
  #define DFU_ROM_ADDR      0x1FFF0000UL
#elif defined(STM32H523xx)
  /* H523: 272KB SRAM, top = 0x20044000 */
  #define DFU_MAGIC_ADDR    (*(volatile uint32_t *)0x20043FF0UL)
  #define DFU_ROM_ADDR      0x0BF97000UL  /* AN2606: H523/H533 bootloader entry */
#endif

/* SCB AIRCR: Application Interrupt and Reset Control Register */
#define SCB_AIRCR           REG32(0xE000ED0CUL)
#define SCB_AIRCR_SYSRESETREQ  (1UL << 2)
#define SCB_AIRCR_VECTKEY      (0x05FAUL << 16)

/**
 * Reboot into DFU mode. Does not return.
 *
 * Writes the DFU magic to a reserved SRAM address, then triggers
 * a system reset. SRAM survives system reset, so the bootloader
 * (custom or ROM) will see the magic on the next boot.
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

/**
 * Jump to the STM32 ROM (system memory) bootloader. Does not return.
 *
 * Called from core_init() when ROM_DFU is enabled and the DFU magic
 * was found. Runs before ANY peripheral or clock init, so the MCU is
 * in near-reset state. The ROM bootloader configures its own clocks
 * (HSI48 + CRS for USB) and enumerates as 0483:DF11 with DfuSe.
 *
 * L4 (Cortex-M4): Remaps system memory to 0x00000000 via SYSCFG_MEMRMP,
 *   then jumps. The remap ensures the ROM bootloader reads its own
 *   vectors from address 0 and doesn't bounce back to flash.
 *
 * H5 (Cortex-M33): No MEMRMP equivalent in SBS. Instead, sets VTOR
 *   directly to the ROM bootloader base — the M33 always respects VTOR
 *   for vector fetches, so no memory remap is needed.
 */
static inline void hal_dfu_jump_to_rom(void) __attribute__((noreturn));
static inline void hal_dfu_jump_to_rom(void)
{
#if defined(STM32L422xx)
    /* Enable SYSCFG clock: RCC_APB2ENR bit 0 (SYSCFGEN)
     * RCC_BASE = 0x40021000, APB2ENR offset = 0x60 */
    SET_BITS(REG32(0x40021060UL), (1UL << 0));
    (void)REG32(0x40021060UL);  /* read-back fence */

    /* Remap system memory to 0x00000000:
     * SYSCFG_MEMRMP (0x40010000) bits [2:0] = 001 (System Flash) */
    MOD_BITS(REG32(0x40010000UL), 0x07UL, 0x01UL);

#elif defined(STM32H523xx)
    /* H5 Cortex-M33: Set VTOR to ROM bootloader base.
     * SBS has no MEMRMP — VTOR handles vector relocation directly. */
    REG32(0xE000ED08UL) = DFU_ROM_ADDR;
#endif

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    /* Read ROM bootloader's initial SP and reset vector */
    volatile uint32_t *rom = (volatile uint32_t *)DFU_ROM_ADDR;
    uint32_t sp = rom[0];
    uint32_t rv = rom[1];

    __asm volatile (
        "msr msp, %0 \n"
        "bx  %1      \n"
        :
        : "r" (sp), "r" (rv)
        : "memory"
    );

    __builtin_unreachable();
}

#endif /* HAL_DFU_H */
