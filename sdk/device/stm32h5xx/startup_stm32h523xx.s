/**
 * Startup file for STM32H523xx
 * Minimal vector table + Reset_Handler with .data/.bss init
 *
 * This is a clean, hand-written startup — no CubeIDE code generation.
 */

    .syntax unified
    .cpu cortex-m33
    .fpu fpv5-sp-d16
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
 * STM32H523 has Cortex-M33 system exceptions + peripheral interrupts.
 * We define all of them as weak aliases to Default_Handler so any can
 * be overridden by simply defining the function in C.
 */
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    /* Cortex-M33 system exceptions */
    .word _estack                   /*  0: Initial stack pointer */
    .word Reset_Handler             /*  1: Reset */
    .word NMI_Handler               /*  2: NMI */
    .word HardFault_Handler         /*  3: Hard fault */
    .word MemManage_Handler         /*  4: Memory management fault */
    .word BusFault_Handler          /*  5: Bus fault */
    .word UsageFault_Handler        /*  6: Usage fault */
    .word SecureFault_Handler       /*  7: Secure fault */
    .word 0                         /*  8: Reserved */
    .word 0                         /*  9: Reserved */
    .word 0                         /* 10: Reserved */
    .word SVC_Handler               /* 11: SVCall */
    .word DebugMon_Handler          /* 12: Debug monitor */
    .word 0                         /* 13: Reserved */
    .word PendSV_Handler            /* 14: PendSV */
    .word SysTick_Handler           /* 15: SysTick */

    /* STM32H523 peripheral interrupts */
    .word WWDG_IRQHandler           /*  0: Window Watchdog */
    .word PVD_AVD_IRQHandler        /*  1: PVD/AVD through EXTI */
    .word RTC_IRQHandler            /*  2: RTC through EXTI */
    .word RTC_S_IRQHandler          /*  3: RTC secure */
    .word TAMP_IRQHandler           /*  4: Tamper */
    .word RAMCFG_IRQHandler         /*  5: RAMCFG */
    .word FLASH_IRQHandler          /*  6: Flash */
    .word FLASH_S_IRQHandler        /*  7: Flash secure */
    .word GTZC_IRQHandler           /*  8: GTZC */
    .word RCC_IRQHandler            /*  9: RCC */
    .word RCC_S_IRQHandler          /* 10: RCC secure */
    .word EXTI0_IRQHandler          /* 11: EXTI Line 0 */
    .word EXTI1_IRQHandler          /* 12: EXTI Line 1 */
    .word EXTI2_IRQHandler          /* 13: EXTI Line 2 */
    .word EXTI3_IRQHandler          /* 14: EXTI Line 3 */
    .word EXTI4_IRQHandler          /* 15: EXTI Line 4 */
    .word EXTI5_IRQHandler          /* 16: EXTI Line 5 */
    .word EXTI6_IRQHandler          /* 17: EXTI Line 6 */
    .word EXTI7_IRQHandler          /* 18: EXTI Line 7 */
    .word EXTI8_IRQHandler          /* 19: EXTI Line 8 */
    .word EXTI9_IRQHandler          /* 20: EXTI Line 9 */
    .word EXTI10_IRQHandler         /* 21: EXTI Line 10 */
    .word EXTI11_IRQHandler         /* 22: EXTI Line 11 */
    .word EXTI12_IRQHandler         /* 23: EXTI Line 12 */
    .word EXTI13_IRQHandler         /* 24: EXTI Line 13 */
    .word EXTI14_IRQHandler         /* 25: EXTI Line 14 */
    .word EXTI15_IRQHandler         /* 26: EXTI Line 15 */
    .word IWDG_IRQHandler           /* 27: IWDG */
    .word 0                         /* 28: Reserved */
    .word GPDMA1_Channel0_IRQHandler /* 29: GPDMA1 Channel 0 */
    .word GPDMA1_Channel1_IRQHandler /* 30: GPDMA1 Channel 1 */
    .word GPDMA1_Channel2_IRQHandler /* 31: GPDMA1 Channel 2 */
    .word GPDMA1_Channel3_IRQHandler /* 32: GPDMA1 Channel 3 */
    .word GPDMA1_Channel4_IRQHandler /* 33: GPDMA1 Channel 4 */
    .word GPDMA1_Channel5_IRQHandler /* 34: GPDMA1 Channel 5 */
    .word GPDMA1_Channel6_IRQHandler /* 35: GPDMA1 Channel 6 */
    .word GPDMA1_Channel7_IRQHandler /* 36: GPDMA1 Channel 7 */
    .word ADC1_IRQHandler           /* 37: ADC1 */
    .word 0                         /* 38: Reserved */
    .word 0                         /* 39: Reserved */
    .word 0                         /* 40: Reserved */
    .word TIM1_UP_IRQHandler        /* 41: TIM1 Update */
    .word 0                         /* 42: Reserved */
    .word 0                         /* 43: Reserved */
    .word TIM1_CC_IRQHandler        /* 44: TIM1 Capture Compare */
    .word TIM2_IRQHandler           /* 45: TIM2 */
    .word TIM3_IRQHandler           /* 46: TIM3 */
    .word 0                         /* 47: Reserved */
    .word 0                         /* 48: Reserved */
    .word 0                         /* 49: Reserved */
    .word 0                         /* 50: Reserved */
    .word SPI1_IRQHandler           /* 51: SPI1 */
    .word SPI2_IRQHandler           /* 52: SPI2 */
    .word SPI3_IRQHandler           /* 53: SPI3 */
    .word 0                         /* 54: Reserved */
    .word I2C1_EV_IRQHandler        /* 55: I2C1 Event */
    .word I2C1_ER_IRQHandler        /* 56: I2C1 Error */
    .word I2C2_EV_IRQHandler        /* 57: I2C2 Event */
    .word USART1_IRQHandler         /* 58: USART1 (shared with I2C2 Error) */
    .word USART2_IRQHandler         /* 59: USART2 */
    .word USART3_IRQHandler         /* 60: USART3 */
    .word 0                         /* 61: Reserved */
    .word 0                         /* 62: Reserved */
    .word LPUART1_IRQHandler        /* 63: LPUART1 */
    .word 0                         /* 64: Reserved */
    .word 0                         /* 65: Reserved */
    .word 0                         /* 66: Reserved */
    .word 0                         /* 67: Reserved */
    .word 0                         /* 68: Reserved */
    .word 0                         /* 69: Reserved */
    .word 0                         /* 70: Reserved */
    .word 0                         /* 71: Reserved */
    .word 0                         /* 72: Reserved */
    .word USB_DRD_FS_IRQHandler     /* 73: USB DRD FS */

/**
 * Weak aliases — any of these can be overridden by defining the
 * function in C code. By default they all land in Default_Handler.
 */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    .weak BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak SecureFault_Handler
    .thumb_set SecureFault_Handler, Default_Handler
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_AVD_IRQHandler
    .thumb_set PVD_AVD_IRQHandler, Default_Handler
    .weak RTC_IRQHandler
    .thumb_set RTC_IRQHandler, Default_Handler
    .weak RTC_S_IRQHandler
    .thumb_set RTC_S_IRQHandler, Default_Handler
    .weak TAMP_IRQHandler
    .thumb_set TAMP_IRQHandler, Default_Handler
    .weak RAMCFG_IRQHandler
    .thumb_set RAMCFG_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak FLASH_S_IRQHandler
    .thumb_set FLASH_S_IRQHandler, Default_Handler
    .weak GTZC_IRQHandler
    .thumb_set GTZC_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak RCC_S_IRQHandler
    .thumb_set RCC_S_IRQHandler, Default_Handler
    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler, Default_Handler
    .weak EXTI1_IRQHandler
    .thumb_set EXTI1_IRQHandler, Default_Handler
    .weak EXTI2_IRQHandler
    .thumb_set EXTI2_IRQHandler, Default_Handler
    .weak EXTI3_IRQHandler
    .thumb_set EXTI3_IRQHandler, Default_Handler
    .weak EXTI4_IRQHandler
    .thumb_set EXTI4_IRQHandler, Default_Handler
    .weak EXTI5_IRQHandler
    .thumb_set EXTI5_IRQHandler, Default_Handler
    .weak EXTI6_IRQHandler
    .thumb_set EXTI6_IRQHandler, Default_Handler
    .weak EXTI7_IRQHandler
    .thumb_set EXTI7_IRQHandler, Default_Handler
    .weak EXTI8_IRQHandler
    .thumb_set EXTI8_IRQHandler, Default_Handler
    .weak EXTI9_IRQHandler
    .thumb_set EXTI9_IRQHandler, Default_Handler
    .weak EXTI10_IRQHandler
    .thumb_set EXTI10_IRQHandler, Default_Handler
    .weak EXTI11_IRQHandler
    .thumb_set EXTI11_IRQHandler, Default_Handler
    .weak EXTI12_IRQHandler
    .thumb_set EXTI12_IRQHandler, Default_Handler
    .weak EXTI13_IRQHandler
    .thumb_set EXTI13_IRQHandler, Default_Handler
    .weak EXTI14_IRQHandler
    .thumb_set EXTI14_IRQHandler, Default_Handler
    .weak EXTI15_IRQHandler
    .thumb_set EXTI15_IRQHandler, Default_Handler
    .weak IWDG_IRQHandler
    .thumb_set IWDG_IRQHandler, Default_Handler
    .weak GPDMA1_Channel0_IRQHandler
    .thumb_set GPDMA1_Channel0_IRQHandler, Default_Handler
    .weak GPDMA1_Channel1_IRQHandler
    .thumb_set GPDMA1_Channel1_IRQHandler, Default_Handler
    .weak GPDMA1_Channel2_IRQHandler
    .thumb_set GPDMA1_Channel2_IRQHandler, Default_Handler
    .weak GPDMA1_Channel3_IRQHandler
    .thumb_set GPDMA1_Channel3_IRQHandler, Default_Handler
    .weak GPDMA1_Channel4_IRQHandler
    .thumb_set GPDMA1_Channel4_IRQHandler, Default_Handler
    .weak GPDMA1_Channel5_IRQHandler
    .thumb_set GPDMA1_Channel5_IRQHandler, Default_Handler
    .weak GPDMA1_Channel6_IRQHandler
    .thumb_set GPDMA1_Channel6_IRQHandler, Default_Handler
    .weak GPDMA1_Channel7_IRQHandler
    .thumb_set GPDMA1_Channel7_IRQHandler, Default_Handler
    .weak ADC1_IRQHandler
    .thumb_set ADC1_IRQHandler, Default_Handler
    .weak TIM1_UP_IRQHandler
    .thumb_set TIM1_UP_IRQHandler, Default_Handler
    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler
    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler
    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler
    .weak SPI3_IRQHandler
    .thumb_set SPI3_IRQHandler, Default_Handler
    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler, Default_Handler
    .weak I2C1_ER_IRQHandler
    .thumb_set I2C1_ER_IRQHandler, Default_Handler
    .weak I2C2_EV_IRQHandler
    .thumb_set I2C2_EV_IRQHandler, Default_Handler
    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
    .weak USART3_IRQHandler
    .thumb_set USART3_IRQHandler, Default_Handler
    .weak LPUART1_IRQHandler
    .thumb_set LPUART1_IRQHandler, Default_Handler
    .weak USB_DRD_FS_IRQHandler
    .thumb_set USB_DRD_FS_IRQHandler, Default_Handler
