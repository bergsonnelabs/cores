/**
 * Startup file for STM32L011xx
 * Minimal vector table + Reset_Handler with .data/.bss init
 *
 * This is a clean, hand-written startup — no CubeIDE code generation.
 *
 * Cortex-M0+ notes:
 *   - No FPU
 *   - No cbz/cbnz — use cmp + beq/bne
 *   - No movw/movt — use ldr r0, =symbol
 *   - Only r0-r7 available for most Thumb-1 instructions
 */

    .syntax unified
    .cpu cortex-m0plus
    .thumb

/* Symbols from linker script */
.word _sidata       /* Start of .data initializers in FLASH */
.word _sdata        /* Start of .data in SRAM */
.word _edata        /* End of .data in SRAM */
.word _sbss         /* Start of .bss in SRAM */
.word _ebss         /* End of .bss in SRAM */
.word _estack        /* Initial stack pointer (top of SRAM) */

/**
 * Reset_Handler — called on power-on or reset.
 * Copies .data from FLASH to SRAM, zeros .bss, then calls main().
 */
    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Set stack pointer (redundant — hardware does this from vector[0],
       but nice for debugger resets) */
    ldr r0, =_estack
    mov sp, r0

    /* Copy .data section from FLASH to SRAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b .Ldata_check
.Ldata_copy:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4
.Ldata_check:
    adds r4, r0, r3
    cmp r4, r1
    bcc .Ldata_copy

    /* Zero-fill .bss section */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    b .Lbss_check
.Lbss_zero:
    str r2, [r0]
    adds r0, r0, #4
.Lbss_check:
    cmp r0, r1
    bcc .Lbss_zero

    /* Call main() */
    bl main

    /* If main() returns, loop forever */
.Lhang:
    b .Lhang

    .size Reset_Handler, .-Reset_Handler

/**
 * Default handler for unimplemented interrupts — infinite loop.
 */
    .section .text.Default_Handler, "ax", %progbits
Default_Handler:
    b Default_Handler
    .size Default_Handler, .-Default_Handler

/**
 * Vector table — placed at 0x0000_0000 in FLASH via .isr_vector section.
 *
 * Cortex-M0+ has 16 system exceptions + 32 peripheral IRQs (0-31).
 * We define all of them as weak aliases to Default_Handler so any can
 * be overridden by simply defining the function in C.
 */
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    /* Cortex-M0+ system exceptions */
    .word _estack                   /*  0: Initial stack pointer */
    .word Reset_Handler             /*  1: Reset */
    .word NMI_Handler               /*  2: NMI */
    .word HardFault_Handler         /*  3: Hard fault */
    .word 0                         /*  4: Reserved (no MemManage on M0+) */
    .word 0                         /*  5: Reserved (no BusFault on M0+) */
    .word 0                         /*  6: Reserved (no UsageFault on M0+) */
    .word 0                         /*  7: Reserved */
    .word 0                         /*  8: Reserved */
    .word 0                         /*  9: Reserved */
    .word 0                         /* 10: Reserved */
    .word SVC_Handler               /* 11: SVCall */
    .word 0                         /* 12: Reserved (no DebugMon on M0+) */
    .word 0                         /* 13: Reserved */
    .word PendSV_Handler            /* 14: PendSV */
    .word SysTick_Handler           /* 15: SysTick */

    /* STM32L011 peripheral interrupts (IRQ 0-31) */
    .word WWDG_IRQHandler           /*  0: Window Watchdog */
    .word PVD_IRQHandler            /*  1: PVD through EXTI */
    .word RTC_IRQHandler            /*  2: RTC through EXTI */
    .word FLASH_IRQHandler          /*  3: Flash */
    .word RCC_IRQHandler            /*  4: RCC */
    .word EXTI0_1_IRQHandler        /*  5: EXTI Lines 0-1 */
    .word EXTI2_3_IRQHandler        /*  6: EXTI Lines 2-3 */
    .word EXTI4_15_IRQHandler       /*  7: EXTI Lines 4-15 */
    .word 0                         /*  8: Reserved */
    .word DMA1_Channel1_IRQHandler  /*  9: DMA1 Channel 1 */
    .word DMA1_Channel2_3_IRQHandler /* 10: DMA1 Channel 2-3 */
    .word DMA1_Channel4_5_6_7_IRQHandler /* 11: DMA1 Channel 4-7 */
    .word ADC1_COMP_IRQHandler      /* 12: ADC1 / COMP */
    .word LPTIM1_IRQHandler         /* 13: LPTIM1 */
    .word 0                         /* 14: Reserved */
    .word TIM2_IRQHandler           /* 15: TIM2 */
    .word 0                         /* 16: Reserved */
    .word 0                         /* 17: Reserved */
    .word 0                         /* 18: Reserved */
    .word 0                         /* 19: Reserved */
    .word TIM21_IRQHandler          /* 20: TIM21 */
    .word 0                         /* 21: Reserved */
    .word 0                         /* 22: Reserved */
    .word I2C1_IRQHandler           /* 23: I2C1 */
    .word 0                         /* 24: Reserved */
    .word SPI1_IRQHandler           /* 25: SPI1 */
    .word 0                         /* 26: Reserved */
    .word 0                         /* 27: Reserved */
    .word USART2_IRQHandler         /* 28: USART2 */
    .word LPUART1_IRQHandler        /* 29: LPUART1 */
    .word 0                         /* 30: Reserved */
    .word 0                         /* 31: Reserved */

/**
 * Weak aliases — any of these can be overridden by defining the
 * function in C code. By default they all land in Default_Handler.
 */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler
    .weak RTC_IRQHandler
    .thumb_set RTC_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak EXTI0_1_IRQHandler
    .thumb_set EXTI0_1_IRQHandler, Default_Handler
    .weak EXTI2_3_IRQHandler
    .thumb_set EXTI2_3_IRQHandler, Default_Handler
    .weak EXTI4_15_IRQHandler
    .thumb_set EXTI4_15_IRQHandler, Default_Handler
    .weak DMA1_Channel1_IRQHandler
    .thumb_set DMA1_Channel1_IRQHandler, Default_Handler
    .weak DMA1_Channel2_3_IRQHandler
    .thumb_set DMA1_Channel2_3_IRQHandler, Default_Handler
    .weak DMA1_Channel4_5_6_7_IRQHandler
    .thumb_set DMA1_Channel4_5_6_7_IRQHandler, Default_Handler
    .weak ADC1_COMP_IRQHandler
    .thumb_set ADC1_COMP_IRQHandler, Default_Handler
    .weak LPTIM1_IRQHandler
    .thumb_set LPTIM1_IRQHandler, Default_Handler
    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler
    .weak TIM21_IRQHandler
    .thumb_set TIM21_IRQHandler, Default_Handler
    .weak I2C1_IRQHandler
    .thumb_set I2C1_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
    .weak LPUART1_IRQHandler
    .thumb_set LPUART1_IRQHandler, Default_Handler
