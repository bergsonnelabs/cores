/**
 * ll_rtc.h — Low-level RTC (Real-Time Clock) operations
 *
 * Time/date set and read, wakeup timer, alarm configuration.
 * The RTC register layout is largely consistent across all four
 * families. Requires backup domain access (ll_pwr_enable_backup_access).
 *
 * RTC is clocked from LSE (32.768kHz crystal) or LSI (~32kHz RC).
 * The async/sync prescalers divide the clock to get a 1Hz tick:
 *   LSE: PREDIV_A=127, PREDIV_S=255 → 32768/128/256 = 1Hz
 *   LSI: PREDIV_A=127, PREDIV_S=249 → 32000/128/250 ≈ 1Hz
 */

#ifndef LL_RTC_H
#define LL_RTC_H

#include "ll_common.h"

/* ---- RTC base address ---- */

#if defined(STM32L011xx)
  #define RTC_BASE          0x40002800UL
#elif defined(STM32L422xx)
  #define RTC_BASE          0x40002800UL
#elif defined(STM32WBA55xx)
  #define RTC_BASE          0x46007800UL
#elif defined(STM32H523xx)
  #define RTC_BASE          0x44007800UL
#endif

/* ---- RTC register offsets ---- */

#define RTC_TR              REG32(RTC_BASE + 0x00UL)  /* Time register */
#define RTC_DR              REG32(RTC_BASE + 0x04UL)  /* Date register */
#define RTC_SSR             REG32(RTC_BASE + 0x08UL)  /* Sub second register */
#define RTC_ICSR            REG32(RTC_BASE + 0x0CUL)  /* Init/status (L4+) */
#define RTC_PRER            REG32(RTC_BASE + 0x10UL)  /* Prescaler register */
#define RTC_WUTR            REG32(RTC_BASE + 0x14UL)  /* Wakeup timer */
#define RTC_CR              REG32(RTC_BASE + 0x18UL)  /* Control register */
#define RTC_WPR             REG32(RTC_BASE + 0x24UL)  /* Write protection */

/* For L0, ISR is at 0x0C instead of ICSR (same bits, different name) */
#if defined(STM32L011xx)
  #define RTC_ISR           REG32(RTC_BASE + 0x0CUL)
#endif

/* ---- CR bit definitions ---- */

#define LL_RTC_CR_WUTE          (1UL << 10)   /* Wakeup timer enable */
#define LL_RTC_CR_ALRAE         (1UL << 8)    /* Alarm A enable */
#define LL_RTC_CR_WUTIE         (1UL << 14)   /* Wakeup timer interrupt enable */
#define LL_RTC_CR_ALRAIE        (1UL << 12)   /* Alarm A interrupt enable */
#define LL_RTC_CR_FMT           (1UL << 6)    /* Hour format: 1=12h, 0=24h */

/* Wakeup clock selection (WUCKSEL[2:0] in CR) */
#define LL_RTC_WUCKSEL_DIV16    0x0UL   /* RTC/16 */
#define LL_RTC_WUCKSEL_DIV8     0x1UL
#define LL_RTC_WUCKSEL_DIV4     0x2UL
#define LL_RTC_WUCKSEL_DIV2     0x3UL
#define LL_RTC_WUCKSEL_1HZ      0x4UL   /* 1Hz from calendaring clock */

/* ---- ISR/ICSR bit definitions ---- */

#define LL_RTC_ISR_INITF        (1UL << 6)    /* Init mode flag */
#define LL_RTC_ISR_INIT         (1UL << 7)    /* Init mode enable */
#define LL_RTC_ISR_RSF          (1UL << 5)    /* Registers sync flag */
#define LL_RTC_ISR_WUTF         (1UL << 10)   /* Wakeup timer flag */
#define LL_RTC_ISR_WUTWF        (1UL << 2)    /* Wakeup timer write flag */

/* ============================================================
 * BCD helpers
 * ============================================================ */

static inline uint8_t ll_bcd_to_bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static inline uint8_t ll_bin_to_bcd(uint8_t bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

/* ============================================================
 * Write protection
 * ============================================================ */

/** Unlock RTC write protection */
static inline void ll_rtc_unlock(void)
{
    RTC_WPR = 0xCAUL;
    RTC_WPR = 0x53UL;
}

/** Re-enable RTC write protection */
static inline void ll_rtc_lock(void)
{
    RTC_WPR = 0xFFUL;
}

/* ============================================================
 * LSE / LSI clock enable
 * ============================================================ */

/**
 * Enable LSE (32.768kHz external crystal).
 * Requires backup domain access to be enabled first.
 */
static inline void ll_rcc_lse_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x50UL), (1UL << 0));   /* CSR: LSEON */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x90UL), (1UL << 0));   /* BDCR: LSEON */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xE0UL), (1UL << 0));   /* BDCR1: LSEON */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xF0UL), (1UL << 0));   /* BDCR: LSEON */
#endif
}

static inline int ll_rcc_lse_ready(void)
{
#if defined(STM32L011xx)
    return (REG32(RCC_BASE + 0x50UL) & (1UL << 1)) != 0;
#elif defined(STM32L422xx)
    return (REG32(RCC_BASE + 0x90UL) & (1UL << 1)) != 0;
#elif defined(STM32WBA55xx)
    return (REG32(RCC_BASE + 0xE0UL) & (1UL << 1)) != 0;
#elif defined(STM32H523xx)
    return (REG32(RCC_BASE + 0xF0UL) & (1UL << 1)) != 0;
#else
    return 0;
#endif
}

/**
 * Enable LSI (~32kHz internal RC).
 */
static inline void ll_rcc_lsi_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x50UL), (1UL << 8));   /* CSR: LSION */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x94UL), (1UL << 0));   /* CSR: LSION */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xE4UL), (1UL << 0));   /* BDCR2: LSION? */
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xF0UL), (1UL << 26));  /* BDCR: LSION */
#endif
}

static inline int ll_rcc_lsi_ready(void)
{
#if defined(STM32L011xx)
    return (REG32(RCC_BASE + 0x50UL) & (1UL << 9)) != 0;
#elif defined(STM32L422xx)
    return (REG32(RCC_BASE + 0x94UL) & (1UL << 1)) != 0;
#elif defined(STM32WBA55xx)
    return (REG32(RCC_BASE + 0xE4UL) & (1UL << 1)) != 0;
#elif defined(STM32H523xx)
    return (REG32(RCC_BASE + 0xF0UL) & (1UL << 27)) != 0;  /* BDCR: LSIRDY */
#else
    return 0;
#endif
}

/**
 * Select RTC clock source. Call before enabling RTC.
 *   0 = no clock, 1 = LSE, 2 = LSI, 3 = HSE/32
 */
static inline void ll_rcc_rtc_set_source(uint32_t source)
{
#if defined(STM32L011xx)
    MOD_BITS(REG32(RCC_BASE + 0x50UL), 0x3UL << 16, source << 16);
#elif defined(STM32L422xx)
    MOD_BITS(REG32(RCC_BASE + 0x90UL), 0x3UL << 8, source << 8);  /* BDCR RTCSEL */
#elif defined(STM32WBA55xx)
    MOD_BITS(REG32(RCC_BASE + 0xE0UL), 0x3UL << 8, source << 8);
#elif defined(STM32H523xx)
    MOD_BITS(REG32(RCC_BASE + 0xF0UL), 0x3UL << 8, source << 8);
#endif
}

/** Enable RTC peripheral clock */
static inline void ll_rcc_rtc_enable(void)
{
#if defined(STM32L011xx)
    SET_BITS(REG32(RCC_BASE + 0x50UL), (1UL << 18));  /* CSR: RTCEN */
#elif defined(STM32L422xx)
    SET_BITS(REG32(RCC_BASE + 0x90UL), (1UL << 15));  /* BDCR: RTCEN */
#elif defined(STM32WBA55xx)
    SET_BITS(REG32(RCC_BASE + 0xE0UL), (1UL << 15));
#elif defined(STM32H523xx)
    SET_BITS(REG32(RCC_BASE + 0xF0UL), (1UL << 15));
#endif
}

/* ============================================================
 * RTC initialization
 * ============================================================ */

/** Enter RTC init mode (freezes calendar, allows register writes) */
static inline void ll_rtc_enter_init(void)
{
#if defined(STM32L011xx)
    SET_BITS(RTC_ISR, LL_RTC_ISR_INIT);
    while (!(RTC_ISR & LL_RTC_ISR_INITF))
        ;
#else
    SET_BITS(RTC_ICSR, LL_RTC_ISR_INIT);
    while (!(RTC_ICSR & LL_RTC_ISR_INITF))
        ;
#endif
}

/** Exit RTC init mode (resumes calendar) */
static inline void ll_rtc_exit_init(void)
{
#if defined(STM32L011xx)
    CLR_BITS(RTC_ISR, LL_RTC_ISR_INIT);
#else
    CLR_BITS(RTC_ICSR, LL_RTC_ISR_INIT);
#endif
}

/**
 * Initialize the RTC with LSE or LSI.
 *   use_lse: 1 = use LSE (32.768kHz crystal), 0 = use LSI (~32kHz)
 *
 * Prerequisites:
 *   - PWR clock enabled
 *   - Backup domain access enabled
 */
static inline void ll_rtc_init(int use_lse)
{
    if (use_lse) {
        ll_rcc_lse_enable();
        while (!ll_rcc_lse_ready())
            ;
        ll_rcc_rtc_set_source(1);  /* LSE */
    } else {
        ll_rcc_lsi_enable();
        while (!ll_rcc_lsi_ready())
            ;
        ll_rcc_rtc_set_source(2);  /* LSI */
    }

    ll_rcc_rtc_enable();

#if defined(STM32H523xx)
    /* H5: enable the RTC APB interface clock (not enabled by default in run mode) */
    SET_BITS(REG32(RCC_BASE + 0xA8UL), (1UL << 21));  /* APB3ENR: RTCAPBEN */
    (void)REG32(RCC_BASE + 0xA8UL);  /* read-back fence */
#endif

    ll_rtc_unlock();
    ll_rtc_enter_init();

    /* Set prescalers for 1Hz */
    if (use_lse) {
        /* 32768 / 128 / 256 = 1Hz */
        RTC_PRER = (127UL << 16) | 255UL;
    } else {
        /* ~32000 / 128 / 250 ≈ 1Hz */
        RTC_PRER = (127UL << 16) | 249UL;
    }

    /* 24-hour format */
    CLR_BITS(RTC_CR, LL_RTC_CR_FMT);

    ll_rtc_exit_init();
    ll_rtc_lock();
}

/* ============================================================
 * Time / Date
 * ============================================================ */

/** Set the time (24h format) */
static inline void ll_rtc_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    ll_rtc_unlock();
    ll_rtc_enter_init();

    RTC_TR = ((uint32_t)ll_bin_to_bcd(hours) << 16)
           | ((uint32_t)ll_bin_to_bcd(minutes) << 8)
           | ((uint32_t)ll_bin_to_bcd(seconds));

    ll_rtc_exit_init();
    ll_rtc_lock();
}

/** Set the date */
static inline void ll_rtc_set_date(uint8_t year, uint8_t month,
                                   uint8_t day, uint8_t weekday)
{
    ll_rtc_unlock();
    ll_rtc_enter_init();

    RTC_DR = ((uint32_t)ll_bin_to_bcd(year) << 16)
           | ((uint32_t)weekday << 13)
           | ((uint32_t)ll_bin_to_bcd(month) << 8)
           | ((uint32_t)ll_bin_to_bcd(day));

    ll_rtc_exit_init();
    ll_rtc_lock();
}

/** Read the current time */
static inline void ll_rtc_get_time(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
    uint32_t tr = RTC_TR;
    *hours   = ll_bcd_to_bin((tr >> 16) & 0x3F);
    *minutes = ll_bcd_to_bin((tr >> 8) & 0x7F);
    *seconds = ll_bcd_to_bin(tr & 0x7F);
}

/** Read the current date */
static inline void ll_rtc_get_date(uint8_t *year, uint8_t *month,
                                   uint8_t *day, uint8_t *weekday)
{
    uint32_t dr = RTC_DR;
    *year    = ll_bcd_to_bin((dr >> 16) & 0xFF);
    *month   = ll_bcd_to_bin((dr >> 8) & 0x1F);
    *day     = ll_bcd_to_bin(dr & 0x3F);
    *weekday = (dr >> 13) & 0x07;
}

/* ============================================================
 * Wakeup timer
 * ============================================================ */

/**
 * Configure the RTC wakeup timer.
 *   seconds: wakeup interval in seconds (1-65535 with 1Hz clock)
 *
 * Uses WUCKSEL = 1Hz (CK_SPRE) for simple second-based intervals.
 * EXTI: line 20 on L0/L4/WBA, line 17 on H5 (direct event).
 */
static inline void ll_rtc_wakeup_config(uint32_t seconds)
{
    ll_rtc_unlock();

    /* Disable wakeup timer */
    CLR_BITS(RTC_CR, LL_RTC_CR_WUTE);

    /* Wait for wakeup timer write flag */
#if defined(STM32L011xx)
    while (!(RTC_ISR & LL_RTC_ISR_WUTWF))
        ;
#else
    while (!(RTC_ICSR & LL_RTC_ISR_WUTWF))
        ;
#endif

    /* Set wakeup clock to 1Hz and reload value */
    MOD_BITS(RTC_CR, 0x7UL, LL_RTC_WUCKSEL_1HZ);
    RTC_WUTR = seconds - 1;

#if defined(STM32H523xx)
    /* H5: WUTE must be set during init mode for reliable timer start.
     * Setting WUTE outside init mode doesn't propagate to the RTC domain
     * (WUTWF stays 1, counter never starts). Enter init briefly to set WUTE. */
    ll_rtc_enter_init();
    SET_BITS(RTC_CR, LL_RTC_CR_WUTE | LL_RTC_CR_WUTIE);
    ll_rtc_exit_init();
#else
    /* Enable wakeup timer and its interrupt */
    SET_BITS(RTC_CR, LL_RTC_CR_WUTE | LL_RTC_CR_WUTIE);
#endif

    ll_rtc_lock();
}

/** Disable the wakeup timer */
static inline void ll_rtc_wakeup_disable(void)
{
    ll_rtc_unlock();
    CLR_BITS(RTC_CR, LL_RTC_CR_WUTE | LL_RTC_CR_WUTIE);
    ll_rtc_lock();
}

/** Clear the wakeup timer flag */
static inline void ll_rtc_wakeup_clear_flag(void)
{
#if defined(STM32L011xx)
    CLR_BITS(RTC_ISR, LL_RTC_ISR_WUTF);
#elif defined(STM32H523xx)
    /* H5: SCR at offset 0x5C (RM0481/RM0492), CWUTF at bit 2 */
    REG32(RTC_BASE + 0x5CUL) = (1UL << 2);
#else
    /* L4/WBA: SCR at offset 0x34, CWUTF at bit 2 */
    REG32(RTC_BASE + 0x34UL) = (1UL << 2);
#endif
}

/* ============================================================
 * Alarm A
 *
 * Alarm A matches on a configurable subset of calendar fields
 * (hours, minutes, seconds). Set MSKx bits to ignore a field.
 * ============================================================ */

#define RTC_ALRMAR          REG32(RTC_BASE + 0x1CUL)  /* Alarm A register */
#define RTC_ALRMASSR        REG32(RTC_BASE + 0x44UL)  /* Alarm A sub-second */

/* ALRMAR field masks */
#define LL_RTC_ALRM_MSK4   (1UL << 31)  /* Mask day/date (ignore) */
#define LL_RTC_ALRM_MSK3   (1UL << 23)  /* Mask hours (ignore) */
#define LL_RTC_ALRM_MSK2   (1UL << 15)  /* Mask minutes (ignore) */
#define LL_RTC_ALRM_MSK1   (1UL << 7)   /* Mask seconds (ignore) */

/**
 * Set Alarm A to trigger at a specific time (24h format).
 * Set hours/minutes/seconds to the desired alarm time.
 * Fields with 0xFF are masked (ignored for matching).
 *
 * Example: alarm at 08:30:00 every day:
 *   ll_rtc_alarm_a_set(8, 30, 0);
 *
 * Example: alarm every hour at XX:30:00:
 *   ll_rtc_alarm_a_set(0xFF, 30, 0);
 */
static inline void ll_rtc_alarm_a_set(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    ll_rtc_unlock();

    /* Disable Alarm A */
    CLR_BITS(RTC_CR, LL_RTC_CR_ALRAE);

    /* Wait for alarm write allowed */
#if defined(STM32L011xx)
    while (!(RTC_ISR & (1UL << 0)))   /* ALRAWF */
        ;
#elif !defined(STM32H523xx)
    while (!(RTC_ICSR & (1UL << 0)))  /* ALRAWF */
        ;
#else
    /* H5: no ALRAWF — write is immediately effective after ALRAE=0 */
    for (volatile int i = 0; i < 10; i++) ;
#endif

    /* Build alarm register value */
    uint32_t alrm = LL_RTC_ALRM_MSK4;  /* Always mask day/date */

    if (hours == 0xFF)   alrm |= LL_RTC_ALRM_MSK3;
    else                 alrm |= ((uint32_t)ll_bin_to_bcd(hours) << 16);

    if (minutes == 0xFF) alrm |= LL_RTC_ALRM_MSK2;
    else                 alrm |= ((uint32_t)ll_bin_to_bcd(minutes) << 8);

    if (seconds == 0xFF) alrm |= LL_RTC_ALRM_MSK1;
    else                 alrm |= (uint32_t)ll_bin_to_bcd(seconds);

    RTC_ALRMAR = alrm;

    /* Enable Alarm A + interrupt */
    SET_BITS(RTC_CR, LL_RTC_CR_ALRAE | LL_RTC_CR_ALRAIE);

    ll_rtc_lock();
}

/** Disable Alarm A. */
static inline void ll_rtc_alarm_a_disable(void)
{
    ll_rtc_unlock();
    CLR_BITS(RTC_CR, LL_RTC_CR_ALRAE | LL_RTC_CR_ALRAIE);
    ll_rtc_lock();
}

/** Clear the Alarm A flag. */
static inline void ll_rtc_alarm_a_clear_flag(void)
{
#if defined(STM32L011xx)
    CLR_BITS(RTC_ISR, (1UL << 8));  /* ALRAF */
#elif defined(STM32H523xx)
    REG32(RTC_BASE + 0x5CUL) = (1UL << 0);  /* SCR: CALRAF */
#else
    REG32(RTC_BASE + 0x34UL) = (1UL << 0);  /* SCR: CALRAF */
#endif
}

/** Check if Alarm A flag is set. */
static inline int ll_rtc_alarm_a_flag(void)
{
#if defined(STM32L011xx)
    return (RTC_ISR & (1UL << 8)) != 0;
#elif defined(STM32H523xx)
    /* H5: MISR at offset 0x58, ALRAMF at bit 0 */
    return (REG32(RTC_BASE + 0x58UL) & (1UL << 0)) != 0;
#else
    /* L4/WBA: MISR at offset 0x30 or SR at offset 0x30, ALRAF at bit 0 */
    return (REG32(RTC_BASE + 0x30UL) & (1UL << 0)) != 0;
#endif
}

/* ============================================================
 * Backup registers (BKP0R–BKP31R)
 *
 * Reading requires RTCAPBEN on H5 (enabled by ll_rtc_init).
 * Writing requires PWR clock + backup domain access:
 *   ll_rcc_pwr_clk_enable();
 *   ll_pwr_enable_backup_access();
 * ============================================================ */

#define RTC_BKP_BASE        (RTC_BASE + 0x50UL)

/** Read backup register n (0–31). No clock setup needed. */
static inline uint32_t ll_rtc_bkp_read(uint32_t n)
{
    return REG32(RTC_BKP_BASE + n * 4);
}

/** Write backup register n (0–31). Requires backup domain access. */
static inline void ll_rtc_bkp_write(uint32_t n, uint32_t val)
{
    REG32(RTC_BKP_BASE + n * 4) = val;
}

#endif /* LL_RTC_H */
