/**
 * app_entry.c — BLE application entry point (adapted from ST's reference)
 *
 * Provides:
 *   - MX_APPE_Config()  — HSE tuning from OTP
 *   - MX_APPE_Init()    — full system + BLE subsystem init
 *   - MX_APPE_Process()  — main loop processing (sequencer)
 *   - AMM_RegisterBasicMemoryManager / AMM_ProcessRequest
 *   - UTIL_SEQ_Idle/PreIdle/PostIdle
 *   - BPKACB_Process, HWCB_RNG_Process, FM_ProcessRequest callbacks
 *
 * Original: Core-W Bluetooth/Core/Src/app_entry.c
 * Copyright (c) 2025 STMicroelectronics. Licensed under ST terms.
 */

#include <stdint.h>
#include <stddef.h>
#include "app_conf.h"
#include "stm32_timer.h"
#include "advanced_memory_manager.h"
#include "stm32_mm.h"
#include "stm32_seq.h"
#include "otp.h"
#include "scm.h"
#include "bpka.h"
#include "flash_driver.h"
#include "flash_manager.h"
#include "simple_nvm_arbiter.h"

/* Use proper headers for function declarations */
#include "stm32wbaxx_hal.h"

/* HW_RNG functions from hw.h / hw_rng.c */
extern void HW_RNG_Start(void);
extern int HW_RNG_Process(void);

/* ============================================================
 * AMM configuration
 * ============================================================ */

static uint32_t AMM_Pool[CFG_AMM_POOL_SIZE];
static AMM_VirtualMemoryConfig_t vmConfig[CFG_AMM_VIRTUAL_MEMORY_NUMBER] =
{
    {
        .Id = CFG_AMM_VIRTUAL_STACK_BLE,
        .BufferSize = CFG_AMM_VIRTUAL_STACK_BLE_BUFFER_SIZE
    },
    {
        .Id = CFG_AMM_VIRTUAL_APP_BLE,
        .BufferSize = CFG_AMM_VIRTUAL_APP_BLE_BUFFER_SIZE
    },
};

static AMM_InitParameters_t ammInitConfig =
{
    .p_PoolAddr = AMM_Pool,
    .PoolSize = CFG_AMM_POOL_SIZE,
    .VirtualMemoryNumber = CFG_AMM_VIRTUAL_MEMORY_NUMBER,
    .p_VirtualMemoryConfigList = vmConfig
};

/* ============================================================
 * Private functions
 * ============================================================ */

static void Config_HSE(void)
{
    OTP_Data_s *otp_ptr = NULL;

    if (OTP_Read(DEFAULT_OTP_IDX, &otp_ptr) != HAL_OK)
    {
        /* OTP not present, apply default HSE trim */
        HAL_RCCEx_HSESetTrimming(0x0C);
    }
    else
    {
        HAL_RCCEx_HSESetTrimming(otp_ptr->hsetune);
    }
}

static void System_Init(void)
{
    /* Initialize the Timer Server */
    UTIL_TIMER_Init();
}

static void SystemPower_Config(void)
{
#if (CFG_SCM_SUPPORTED == 1)
    scm_init();
#endif
    /* LPM disabled in our config (CFG_LPM_LEVEL == 0) */
}

static void APPE_RNG_Init(void)
{
    HW_RNG_Start();
    UTIL_SEQ_RegTask(1U << CFG_TASK_HW_RNG, UTIL_SEQ_RFU, (void (*)(void))HW_RNG_Process);
}

static void APPE_FLASH_MANAGER_Init(void)
{
    UTIL_SEQ_RegTask(1U << CFG_TASK_FLASH_MANAGER, UTIL_SEQ_RFU, FM_BackgroundProcess);

    FD_SetStatus(FD_FLASHACCESS_RFTS, LL_FLASH_DISABLE);
    FD_SetStatus(FD_FLASHACCESS_RFTS_BYPASS, LL_FLASH_ENABLE);
    FD_SetStatus(FD_FLASHACCESS_SYSTEM, LL_FLASH_ENABLE);
}

static void APPE_BPKA_Init(void)
{
    UTIL_SEQ_RegTask(1U << CFG_TASK_BPKA, UTIL_SEQ_RFU, BPKA_BG_Process);
}

static void AMM_WrapperInit(uint32_t * const p_PoolAddr, const uint32_t PoolSize)
{
    UTIL_MM_Init((uint8_t *)p_PoolAddr, ((size_t)PoolSize * sizeof(uint32_t)));
}

static uint32_t * AMM_WrapperAllocate(const uint32_t BufferSize)
{
    return (uint32_t *)UTIL_MM_GetBuffer(((size_t)BufferSize * sizeof(uint32_t)));
}

static void AMM_WrapperFree(uint32_t * const p_BufferAddr)
{
    UTIL_MM_ReleaseBuffer((void *)p_BufferAddr);
}

static void APPE_AMM_Init(void)
{
    AMM_Init(&ammInitConfig);
    UTIL_SEQ_RegTask(1U << CFG_TASK_AMM, UTIL_SEQ_RFU, AMM_BackgroundProcess);
}

/* ============================================================
 * Public API
 * ============================================================ */

void MX_APPE_Config(void)
{
    Config_HSE();
}

uint32_t MX_APPE_Init(void *p_param)
{
    (void)p_param;

    System_Init();
    SystemPower_Config();
    APPE_AMM_Init();
    APPE_RNG_Init();
    APPE_FLASH_MANAGER_Init();
    APPE_BPKA_Init();
    SNVMA_Init((uint32_t *)CFG_SNVMA_START_ADDRESS);
    FD_SetStatus(FD_FLASHACCESS_RFTS_BYPASS, LL_FLASH_DISABLE);

    return 0; /* WPAN_SUCCESS */
}

void MX_APPE_Process(void)
{
    UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
}

/* ============================================================
 * Wrap functions — callbacks required by ST middleware
 * ============================================================ */

void UTIL_SEQ_Idle(void)
{
    volatile uint32_t *scb_scr = (volatile uint32_t *)0xE000ED10UL;
    *scb_scr &= ~(1UL << 2);  /* Clear SLEEPDEEP */
    __asm volatile ("wfi" ::: "memory");
}

void UTIL_SEQ_PreIdle(void)
{
}

void UTIL_SEQ_PostIdle(void)
{
    volatile uint32_t *rcc_ahb5enr = (volatile uint32_t *)(0x46020C00UL + 0x098UL);
    *rcc_ahb5enr |= (1UL << 0);  /* RADIOEN */
}

void BPKACB_Process(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_BPKA, CFG_SEQ_PRIO_0);
}

void HWCB_RNG_Process(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_HW_RNG, CFG_SEQ_PRIO_0);
}

void AMM_RegisterBasicMemoryManager(AMM_BasicMemoryManagerFunctions_t * const p_BasicMemoryManagerFunctions)
{
    p_BasicMemoryManagerFunctions->Init = AMM_WrapperInit;
    p_BasicMemoryManagerFunctions->Allocate = AMM_WrapperAllocate;
    p_BasicMemoryManagerFunctions->Free = AMM_WrapperFree;
}

void AMM_ProcessRequest(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_AMM, CFG_SEQ_PRIO_0);
}

void FM_ProcessRequest(void)
{
    UTIL_SEQ_SetTask(1U << CFG_TASK_FLASH_MANAGER, CFG_SEQ_PRIO_0);
}

/* Error_Handler — required by ST middleware (main.h) */
void Error_Handler(void)
{
    while (1) ;
}
