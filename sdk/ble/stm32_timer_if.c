/**
 * stm32_timer_if.c -- Timer server low-level interface using SysTick
 *
 * Implements the UTIL_TIMER_Driver_s interface that stm32_timer.c
 * calls. Uses SysTick millisecond counter for timing.
 *
 * The timer server works in "tick" units. We define 1 tick = 1 ms.
 */

#include <stdint.h>
#include "stm32_timer.h"

/* SysTick millisecond counter from our HAL */
extern volatile uint32_t _systick_ticks;

/* Timer context -- saved at the start of each timer cycle */
static uint32_t timer_context = 0;

/* Pending alarm */
static volatile uint32_t alarm_target = 0;
static volatile uint8_t  alarm_active = 0;

/* ---- Low-level driver functions ---- */

static UTIL_TIMER_Status_t TimerIF_Init(void)
{
    timer_context = 0;
    alarm_active = 0;
    return UTIL_TIMER_OK;
}

static UTIL_TIMER_Status_t TimerIF_DeInit(void)
{
    alarm_active = 0;
    return UTIL_TIMER_OK;
}

static UTIL_TIMER_Status_t TimerIF_StartTimer(uint32_t timeout)
{
    alarm_target = timeout;
    alarm_active = 1;
    return UTIL_TIMER_OK;
}

static UTIL_TIMER_Status_t TimerIF_StopTimer(void)
{
    alarm_active = 0;
    return UTIL_TIMER_OK;
}

static uint32_t TimerIF_SetTimerContext(void)
{
    timer_context = _systick_ticks;
    return timer_context;
}

static uint32_t TimerIF_GetTimerContext(void)
{
    return timer_context;
}

static uint32_t TimerIF_GetTimerElapsedTime(void)
{
    return _systick_ticks - timer_context;
}

static uint32_t TimerIF_GetTimerValue(void)
{
    return _systick_ticks;
}

static uint32_t TimerIF_GetMinimumTimeout(void)
{
    return 1; /* 1 ms minimum */
}

static uint32_t TimerIF_ms2Tick(uint32_t ms)
{
    return ms; /* 1:1 mapping */
}

static uint32_t TimerIF_Tick2ms(uint32_t tick)
{
    return tick; /* 1:1 mapping */
}

/* ---- The driver struct ---- */

const UTIL_TIMER_Driver_s UTIL_TimerDriver =
{
    .InitTimer          = TimerIF_Init,
    .DeInitTimer        = TimerIF_DeInit,
    .StartTimerEvt      = TimerIF_StartTimer,
    .StopTimerEvt       = TimerIF_StopTimer,
    .SetTimerContext     = TimerIF_SetTimerContext,
    .GetTimerContext     = TimerIF_GetTimerContext,
    .GetTimerElapsedTime = TimerIF_GetTimerElapsedTime,
    .GetTimerValue       = TimerIF_GetTimerValue,
    .GetMinimumTimeout   = TimerIF_GetMinimumTimeout,
    .ms2Tick             = TimerIF_ms2Tick,
    .Tick2ms             = TimerIF_Tick2ms,
};

/**
 * ble_timer_server_check() -- Call from main loop to fire expired timers
 *
 * The timer server sets an alarm via StartTimerEvt. We check if the
 * alarm has expired and call UTIL_TIMER_IRQ_Handler().
 */
void ble_timer_server_check(void)
{
    if (alarm_active)
    {
        uint32_t elapsed = _systick_ticks - timer_context;
        if (elapsed >= alarm_target)
        {
            alarm_active = 0;
            UTIL_TIMER_IRQ_Handler();
        }
    }
}
