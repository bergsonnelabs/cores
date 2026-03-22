/**
 * stm32_seq.c — ST Sequencer (cooperative task scheduler)
 *
 * Copied from ST Utilities/sequencer/stm32_seq.c with minimal adaptation.
 * This replaces the minimal stub sequencer in ble_glue.c.
 *
 * Original: Copyright (c) 2019 STMicroelectronics. Licensed under ST terms.
 */

#include "stm32_seq.h"
#include "utilities_conf.h"

/* ---- Types ---- */

typedef struct
{
    uint32_t priority;
    uint32_t round_robin;
} UTIL_SEQ_Priority_t;

/* ---- Defines ---- */

#ifndef UTIL_SEQ_ENTER_CRITICAL_SECTION_IDLE
#define UTIL_SEQ_ENTER_CRITICAL_SECTION_IDLE( )    UTIL_SEQ_ENTER_CRITICAL_SECTION( )
#endif

#ifndef UTIL_SEQ_EXIT_CRITICAL_SECTION_IDLE
#define UTIL_SEQ_EXIT_CRITICAL_SECTION_IDLE( )     UTIL_SEQ_EXIT_CRITICAL_SECTION( )
#endif

#define UTIL_SEQ_NOTASKRUNNING       (0xFFFFFFFFU)
#define UTIL_SEQ_NO_BIT_SET          (0U)
#define UTIL_SEQ_ALL_BIT_SET         (~0U)

#ifndef UTIL_SEQ_CONF_TASK_NBR
#define UTIL_SEQ_CONF_TASK_NBR  (32)
#endif

#if UTIL_SEQ_CONF_TASK_NBR > 32
#error "UTIL_SEQ_CONF_TASK_NBR must be less than or equal to 32"
#endif

#ifndef UTIL_SEQ_CONF_PRIO_NBR
#define UTIL_SEQ_CONF_PRIO_NBR  (2)
#endif

#ifndef UTIL_SEQ_MEMSET8
#define UTIL_SEQ_MEMSET8( dest, value, size )   do { \
    uint8_t *d = (uint8_t*)(dest); \
    for (uint32_t i = 0; i < (size); i++) d[i] = (uint8_t)(value); \
} while(0)
#endif

/* ---- Variables ---- */

static volatile UTIL_SEQ_bm_t TaskSet;
static volatile UTIL_SEQ_bm_t TaskMask = UTIL_SEQ_ALL_BIT_SET;
static UTIL_SEQ_bm_t SuperMask = UTIL_SEQ_ALL_BIT_SET;
static volatile UTIL_SEQ_bm_t EvtSet = UTIL_SEQ_NO_BIT_SET;
static volatile UTIL_SEQ_bm_t EvtWaited = UTIL_SEQ_NO_BIT_SET;
static uint32_t CurrentTaskIdx = 0U;
static void (*TaskCb[UTIL_SEQ_CONF_TASK_NBR])(void);
static volatile UTIL_SEQ_Priority_t TaskPrio[UTIL_SEQ_CONF_PRIO_NBR];
static UTIL_SEQ_bm_t TaskClearList = 0;

/* ---- Private functions ---- */

uint8_t SEQ_BitPosition(uint32_t Value);

/* ---- Public functions ---- */

void UTIL_SEQ_Init(void)
{
    TaskSet = UTIL_SEQ_NO_BIT_SET;
    TaskMask = UTIL_SEQ_ALL_BIT_SET;
    SuperMask = UTIL_SEQ_ALL_BIT_SET;
    EvtSet = UTIL_SEQ_NO_BIT_SET;
    EvtWaited = UTIL_SEQ_NO_BIT_SET;
    CurrentTaskIdx = 0U;
    UTIL_SEQ_MEMSET8((uint8_t *)TaskCb, 0, sizeof(TaskCb));
    for (uint32_t index = 0; index < UTIL_SEQ_CONF_PRIO_NBR; index++)
    {
        TaskPrio[index].priority = 0;
        TaskPrio[index].round_robin = 0;
    }
    UTIL_SEQ_INIT_CRITICAL_SECTION();
    TaskClearList = 0;
}

void UTIL_SEQ_DeInit(void)
{
}

void UTIL_SEQ_Run(UTIL_SEQ_bm_t Mask_bm)
{
    uint32_t counter;
    UTIL_SEQ_bm_t current_task_set;
    UTIL_SEQ_bm_t super_mask_backup;
    UTIL_SEQ_bm_t local_taskset;
    UTIL_SEQ_bm_t local_evtset;
    UTIL_SEQ_bm_t local_taskmask;
    UTIL_SEQ_bm_t local_evtwaited;
    uint32_t round_robin[UTIL_SEQ_CONF_PRIO_NBR];
    UTIL_SEQ_bm_t task_starving_list;

    super_mask_backup = SuperMask;
    SuperMask &= Mask_bm;

    local_taskset = TaskSet;
    local_evtset = EvtSet;
    local_taskmask = TaskMask;
    local_evtwaited = EvtWaited;
    while (((local_taskset & local_taskmask & SuperMask) != 0U) && ((local_evtset & local_evtwaited) == 0U))
    {
        counter = 0U;
        while ((TaskPrio[counter].priority & local_taskmask & SuperMask) == 0U)
        {
            counter++;
        }

        current_task_set = TaskPrio[counter].priority & local_taskmask & SuperMask;

        if ((TaskPrio[counter].round_robin & current_task_set) == 0U)
        {
            TaskPrio[counter].round_robin = UTIL_SEQ_ALL_BIT_SET;
        }

        task_starving_list = TaskSet;
        TaskClearList |= (~TaskPrio[counter].round_robin);
        task_starving_list &= (~TaskClearList);

        if ((task_starving_list & current_task_set) != 0U)
        {
            current_task_set = (task_starving_list & current_task_set);
        }

        if (task_starving_list == 0)
        {
            TaskClearList = 0;
        }

        CurrentTaskIdx = (SEQ_BitPosition(current_task_set & TaskPrio[counter].round_robin));

        UTIL_SEQ_ENTER_CRITICAL_SECTION();
        TaskSet &= ~(1U << CurrentTaskIdx);

        for (counter = UTIL_SEQ_CONF_PRIO_NBR; counter != 0U; counter--)
        {
            TaskPrio[counter - 1u].priority &= ~(1U << CurrentTaskIdx);
        }
        UTIL_SEQ_EXIT_CRITICAL_SECTION();

        UTIL_SEQ_PreTask(CurrentTaskIdx);

        if ((CurrentTaskIdx < UTIL_SEQ_CONF_TASK_NBR) && (TaskCb[CurrentTaskIdx] != NULL))
        {
            for (uint32_t index = 0; index < UTIL_SEQ_CONF_PRIO_NBR; index++)
            {
                TaskPrio[index].round_robin &= ~(1U << CurrentTaskIdx);
                round_robin[index] = TaskPrio[index].round_robin;
            }

            TaskCb[CurrentTaskIdx]();

            for (uint32_t index = 0; index < UTIL_SEQ_CONF_PRIO_NBR; index++)
            {
                TaskPrio[index].round_robin &= round_robin[index];
            }

            UTIL_SEQ_PostTask(CurrentTaskIdx);

            local_taskset = TaskSet;
            local_evtset = EvtSet;
            local_taskmask = TaskMask;
            local_evtwaited = EvtWaited;

            TaskClearList |= (1U << CurrentTaskIdx);
        }
        else
        {
            UTIL_SEQ_CatchWarning(UTIL_SEQ_WARNING_INVALIDTASKID);
        }
    }

    CurrentTaskIdx = UTIL_SEQ_NOTASKRUNNING;
    if ((local_evtset & EvtWaited) == 0U)
    {
        UTIL_SEQ_PreIdle();

        UTIL_SEQ_ENTER_CRITICAL_SECTION_IDLE();
        local_taskset = TaskSet;
        local_evtset = EvtSet;
        local_taskmask = TaskMask;
        if ((local_taskset & local_taskmask & SuperMask) == 0U)
        {
            if ((local_evtset & EvtWaited) == 0U)
            {
                UTIL_SEQ_Idle();
            }
        }
        UTIL_SEQ_EXIT_CRITICAL_SECTION_IDLE();

        UTIL_SEQ_PostIdle();
    }

    SuperMask = super_mask_backup;
}

void UTIL_SEQ_RegTask(UTIL_SEQ_bm_t TaskId_bm, uint32_t Flags, void (*Task)(void))
{
    (void)Flags;
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    TaskCb[SEQ_BitPosition(TaskId_bm)] = Task;
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
}

uint32_t UTIL_SEQ_IsRegisteredTask(UTIL_SEQ_bm_t TaskId_bm)
{
    uint32_t _status = 0;
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    if (TaskCb[SEQ_BitPosition(TaskId_bm)] != NULL)
    {
        _status = 1;
    }
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
    return _status;
}

void UTIL_SEQ_SetTask(UTIL_SEQ_bm_t TaskId_bm, uint32_t Task_Prio)
{
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    TaskSet |= TaskId_bm;
    TaskPrio[Task_Prio].priority |= TaskId_bm;
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
}

uint32_t UTIL_SEQ_IsSchedulableTask(UTIL_SEQ_bm_t TaskId_bm)
{
    uint32_t _status;
    UTIL_SEQ_bm_t local_taskset;
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    local_taskset = TaskSet;
    _status = ((local_taskset & TaskMask & SuperMask & TaskId_bm) == TaskId_bm) ? 1U : 0U;
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
    return _status;
}

void UTIL_SEQ_PauseTask(UTIL_SEQ_bm_t TaskId_bm)
{
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    TaskMask &= (~TaskId_bm);
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
}

uint32_t UTIL_SEQ_IsPauseTask(UTIL_SEQ_bm_t TaskId_bm)
{
    uint32_t _status;
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    _status = ((TaskMask & TaskId_bm) == TaskId_bm) ? 0u : 1u;
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
    return _status;
}

void UTIL_SEQ_ResumeTask(UTIL_SEQ_bm_t TaskId_bm)
{
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    TaskMask |= TaskId_bm;
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
}

void UTIL_SEQ_SetEvt(UTIL_SEQ_bm_t EvtId_bm)
{
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    EvtSet |= EvtId_bm;
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
}

void UTIL_SEQ_ClrEvt(UTIL_SEQ_bm_t EvtId_bm)
{
    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    EvtSet &= (~EvtId_bm);
    UTIL_SEQ_EXIT_CRITICAL_SECTION();
}

void UTIL_SEQ_WaitEvt(UTIL_SEQ_bm_t EvtId_bm)
{
    UTIL_SEQ_bm_t event_waited_id_backup;
    UTIL_SEQ_bm_t current_task_idx;
    UTIL_SEQ_bm_t wait_task_idx;

    current_task_idx = CurrentTaskIdx;
    if (UTIL_SEQ_NOTASKRUNNING == CurrentTaskIdx)
    {
        wait_task_idx = 0u;
    }
    else
    {
        wait_task_idx = (uint32_t)1u << CurrentTaskIdx;
    }

    event_waited_id_backup = EvtWaited;
    EvtWaited = EvtId_bm;

    while ((EvtSet & EvtId_bm) == 0U)
    {
        UTIL_SEQ_EvtIdle(wait_task_idx, EvtId_bm);
    }

    CurrentTaskIdx = current_task_idx;

    UTIL_SEQ_ENTER_CRITICAL_SECTION();
    EvtSet &= (~EvtId_bm);
    UTIL_SEQ_EXIT_CRITICAL_SECTION();

    EvtWaited = event_waited_id_backup;
}

UTIL_SEQ_bm_t UTIL_SEQ_IsEvtPend(void)
{
    UTIL_SEQ_bm_t local_evtwaited = EvtWaited;
    return (EvtSet & local_evtwaited);
}

__attribute__((weak)) void UTIL_SEQ_EvtIdle(UTIL_SEQ_bm_t TaskId_bm, UTIL_SEQ_bm_t EvtWaited_bm)
{
    (void)EvtWaited_bm;
    UTIL_SEQ_Run(~TaskId_bm);
}

__attribute__((weak)) void UTIL_SEQ_Idle(void)
{
}

__attribute__((weak)) void UTIL_SEQ_PreIdle(void)
{
}

__attribute__((weak)) void UTIL_SEQ_PostIdle(void)
{
}

__attribute__((weak)) void UTIL_SEQ_PreTask(uint32_t TaskId)
{
    (void)TaskId;
}

__attribute__((weak)) void UTIL_SEQ_PostTask(uint32_t TaskId)
{
    (void)TaskId;
}

__attribute__((weak)) void UTIL_SEQ_CatchWarning(UTIL_SEQ_WARNING WarningId)
{
    (void)WarningId;
}

/* ---- Bit position (Cortex-M33 has CLZ) ---- */

uint8_t SEQ_BitPosition(uint32_t Value)
{
    return (uint8_t)(31 - __builtin_clz(Value));
}
