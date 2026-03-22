/* Linker script for STM32WBA55HG */
/* 1MB Flash, 128KB SRAM (split: 64K SRAM1 + 64K SRAM2) */

MEMORY
{
    FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM1 (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
    SRAM2 (rwx)  : ORIGIN = 0x20010000, LENGTH = 64K
}

/* Entry point */
ENTRY(Reset_Handler)

/* Highest address of the stack — top of SRAM1 */
_estack = ORIGIN(SRAM1) + LENGTH(SRAM1);

/* Minimum heap and stack sizes */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

SECTIONS
{
    /* Vector table and code go into FLASH */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ARM exception handling — needed by some C library functions */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* Used by startup to initialize .data */
    _sidata = LOADADDR(.data);

    /* Initialized data — loaded from FLASH, lives in SRAM1 */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >SRAM1 AT> FLASH

    /* Uninitialized data — zero-filled in SRAM1 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >SRAM1

    /* Heap grows up, stack grows down — check they don't collide */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >SRAM1

    /* BLE stack buffers — dedicated SRAM2 region */
    .bss_ble (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss_ble = .;
        *(.bss_ble)
        *(.bss_ble*)
        . = ALIGN(4);
        _ebss_ble = .;
    } >SRAM2

    /* Discard debug sections from standard libraries */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
