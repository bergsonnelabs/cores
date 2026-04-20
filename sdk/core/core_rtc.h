/**
 * core_rtc.h — Real-time clock
 *
 * @tessera category rtc label=Core.RTC icon=◴
 */

#ifndef CORE_RTC_H
#define CORE_RTC_H

#include "ll_rtc.h"

/* ============================================================
 * Init
 * ============================================================ */

/**
 * Initialize the RTC using LSI (~32kHz internal RC). Call once from
 * `on start` before setting time, date, or alarms.
 * For LSE, call ll_rtc_init(1) directly in hand-written C.
 *
 * @tessera expose category=rtc name=init
 */
static inline void core_rtc_init(void)
{
    ll_rtc_init(0);
}

/* ============================================================
 * Time
 * ============================================================ */

/**
 * Set the time (24h format).
 *
 * @tessera expose category=rtc name=set_time
 * @param h [0..23] Hours.
 * @param m [0..59] Minutes.
 * @param s [0..59] Seconds.
 */
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

/**
 * Set the date.
 *
 * @tessera expose category=rtc name=set_date
 * @param y [0..99] 2-digit year (2025 = 25).
 * @param mo [1..12] Month.
 * @param d [1..31] Day of month.
 * @param wd [1..7] Weekday (1 = Mon, 7 = Sun).
 */
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
 * Periodic wakeup timer
 * ============================================================ */

/**
 * Configure a periodic wakeup timer.
 *
 * @tessera expose category=rtc name=wakeup
 * @param seconds [1..65535] Wakeup interval in seconds.
 */
static inline void core_rtc_wakeup(uint32_t seconds)
{
    ll_rtc_wakeup_config(seconds);
}

/**
 * Disable the periodic wakeup timer.
 *
 * @tessera expose category=rtc name=wakeup_stop
 */
static inline void core_rtc_wakeup_stop(void)
{
    ll_rtc_wakeup_disable();
}

/* Backward compat */
#define core_rtc_alarm       core_rtc_wakeup
#define core_rtc_alarm_stop  core_rtc_wakeup_stop

/* ============================================================
 * Alarm A — calendar-based alarm
 *
 * Triggers when the RTC time matches the configured fields.
 * Pass 0xFF for any field to ignore it (wildcard).
 *
 * Usage:
 *   core_rtc_set_alarm(8, 30, 0);      // every day at 08:30:00
 *   core_rtc_set_alarm(0xFF, 0, 0);    // every hour at XX:00:00
 *   core_rtc_set_alarm(0xFF, 0xFF, 0); // every minute at XX:XX:00
 * ============================================================ */

/**
 * Set Alarm A to trigger at a specific time.
 * Pass 0xFF for hours, minutes, or seconds to ignore that field.
 * The alarm fires when all non-masked fields match the RTC time.
 *
 * @tessera expose category=rtc name=set_alarm
 * @param hours   [0..255] 0-23, or 255 (0xFF) to match any hour.
 * @param minutes [0..255] 0-59, or 255 (0xFF) to match any minute.
 * @param seconds [0..255] 0-59, or 255 (0xFF) to match any second.
 */
static inline void core_rtc_set_alarm(uint8_t hours, uint8_t minutes,
                                       uint8_t seconds)
{
    ll_rtc_alarm_a_set(hours, minutes, seconds);
}

/**
 * Clear the alarm (disable Alarm A).
 *
 * @tessera expose category=rtc name=clear_alarm
 */
static inline void core_rtc_clear_alarm(void)
{
    ll_rtc_alarm_a_disable();
}

/**
 * Check if the alarm has fired (poll mode). Clears the flag on read.
 *
 * @tessera expose category=rtc name=alarm_fired returns=bool
 */
static inline int core_rtc_alarm_fired(void)
{
    if (ll_rtc_alarm_a_flag()) {
        ll_rtc_alarm_a_clear_flag();
        return 1;
    }
    return 0;
}

#endif /* CORE_RTC_H */
