/**
 * core_rtc.h — Real-time clock
 */

#ifndef CORE_RTC_H
#define CORE_RTC_H

#include "ll_rtc.h"

/* ============================================================
 * Init
 * ============================================================ */

/**
 * Initialize the RTC using LSI (~32kHz internal RC).
 * For LSE, call ll_rtc_init(1) directly.
 */
static inline void core_rtc_init(void)
{
    ll_rtc_init(0);
}

/* ============================================================
 * Time
 * ============================================================ */

/** Set the time (24h format). */
static inline void core_rtc_set_time(uint8_t h, uint8_t m, uint8_t s)
{
    ll_rtc_set_time(h, m, s);
}

/** Read the current time. */
static inline void core_rtc_get_time(uint8_t *h, uint8_t *m, uint8_t *s)
{
    ll_rtc_get_time(h, m, s);
}

/* ============================================================
 * Date
 * ============================================================ */

/** Set the date (year = 2-digit, weekday = 1–7 Mon–Sun). */
static inline void core_rtc_set_date(uint8_t y, uint8_t mo, uint8_t d,
                                      uint8_t wd)
{
    ll_rtc_set_date(y, mo, d, wd);
}

/** Read the current date. */
static inline void core_rtc_get_date(uint8_t *y, uint8_t *mo, uint8_t *d,
                                      uint8_t *wd)
{
    ll_rtc_get_date(y, mo, d, wd);
}

/* ============================================================
 * Wakeup alarm
 * ============================================================ */

/** Configure a periodic wakeup alarm (1–65535 seconds). */
static inline void core_rtc_alarm(uint32_t seconds)
{
    ll_rtc_wakeup_config(seconds);
}

/** Disable the wakeup alarm. */
static inline void core_rtc_alarm_stop(void)
{
    ll_rtc_wakeup_disable();
}

#endif /* CORE_RTC_H */
