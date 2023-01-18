// SPDX-License-Identifier: MIT
// Copyright (c) 2021 Ulrich Hecht

#include "sunxi_rtc.h"
#include "sunxi_clk.h"
#include "io.h"
#include <stdint.h>
#include "types.h"
#include "debug.h"

static volatile rtc_t *const rtc = (rtc_t *)(0x07090000);

/* Control register */
#define LOSC_CTRL				  0x0000
#define LOSC_CTRL_KEY			  (0x16aa << 16)
#define LOSC_CTRL_AUTO_SWT_BYPASS BIT(15)
#define LOSC_CTRL_ALM_DHMS_ACC	  BIT(9)
#define LOSC_CTRL_RTC_HMS_ACC	  BIT(8)
#define LOSC_CTRL_RTC_YMD_ACC	  BIT(7)
#define LOSC_CTRL_EXT_LOSC_EN	  BIT(4)
#define LOSC_CTRL_EXT_OSC		  BIT(0)
#define LOSC_CTRL_ACC_MASK		  GENMASK(9, 7)

#define YEAR_MIN	 1970
#define YEAR_OFF	 (YEAR_MIN - 1900)
#define SECS_PER_DAY (24 * 3600ULL)

/*
 * Get date values
 */
#define DATE_GET_DAY_VALUE(x)  ((x)&0x0000001f)
#define DATE_GET_MON_VALUE(x)  (((x)&0x00000f00) >> 8)
#define DATE_GET_YEAR_VALUE(x) (((x)&0x003f0000) >> 16)
#define LEAP_GET_VALUE(x)	   (((x)&0x00400000) >> 22)

/*
 * Get time values
 */
#define TIME_GET_SEC_VALUE(x)  ((x)&0x0000003f)
#define TIME_GET_MIN_VALUE(x)  (((x)&0x00003f00) >> 8)
#define TIME_GET_HOUR_VALUE(x) (((x)&0x001f0000) >> 16)

/*
 * Set date values
 */
#define DATE_SET_DAY_VALUE(x)  ((x)&0x0000001f)
#define DATE_SET_MON_VALUE(x)  ((x) << 8 & 0x00000f00)
#define DATE_SET_YEAR_VALUE(x) ((x) << 16 & 0x003f0000)
#define LEAP_SET_VALUE(x)	   ((x) << 22 & 0x00400000)

/*
 * Set time values
 */
#define TIME_SET_SEC_VALUE(x)  ((x)&0x0000003f)
#define TIME_SET_MIN_VALUE(x)  ((x) << 8 & 0x00003f00)
#define TIME_SET_HOUR_VALUE(x) ((x) << 16 & 0x001f0000)

/**
 * upper_32_bits - return bits 32-63 of a number
 * @n: the number we're accessing
 *
 * A basic shift-right of a 64- or 32-bit quantity.  Use this to suppress
 * the "right shift count >= width of type" warning when that quantity is
 * 32-bits.
 */
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

/**
 * lower_32_bits - return bits 0-31 of a number
 * @n: the number we're accessing
 */
#define lower_32_bits(n) ((u32)((n)&0xffffffff))

inline static uint8_t is_leap_year(uint32_t year)
{
	return (!(year % 4) && (year % 100)) || !(year % 400);
}

inline static void wait_for_time_write(void)
{
	while (rtc->losc_ctrl & LOSC_CTRL_RTC_HMS_ACC) {
		// XXX: timeout
	}
}

inline static void wait_for_date_write(void)
{
	while (rtc->losc_ctrl & LOSC_CTRL_RTC_YMD_ACC) {
		// XXX: timeout
	}
}

static void rtc_time64_to_tm(u64 time, struct rtc_time *tm)
{
	unsigned int secs;
	int			 days;

	u64 u64tmp;
	u32 u32tmp, udays, century, day_of_century, year_of_century, year, day_of_year, month, day;
	u8	is_Jan_or_Feb, is_leap_year;

	/* time must be positive */
	days = time / 86400;
	secs = time % 86400;

	/* day of the week, 1970-01-01 was a Thursday */
	tm->tm_wday = (days + 4) % 7;

	udays = ((u32)days) + 719468;

	u32tmp		   = 4 * udays + 3;
	century		   = u32tmp / 146097;
	day_of_century = u32tmp % 146097 / 4;

	u32tmp			= 4 * day_of_century + 3;
	u64tmp			= 2939745ULL * u32tmp;
	year_of_century = upper_32_bits(u64tmp);
	day_of_year		= lower_32_bits(u64tmp) / 2939745 / 4;

	year		 = 100 * century + year_of_century;
	is_leap_year = year_of_century != 0 ? year_of_century % 4 == 0 : century % 4 == 0;

	u32tmp = 2141 * day_of_year + 132377;
	month  = u32tmp >> 16;
	day	   = ((u16)u32tmp) / 2141;

	/*
	 * Recall that January 01 is the 306-th day of the year in the
	 * computational (not Gregorian) calendar.
	 */
	is_Jan_or_Feb = day_of_year >= 306;

	/* Converts to the Gregorian calendar. */
	year  = year + is_Jan_or_Feb;
	month = is_Jan_or_Feb ? month - 12 : month;
	day	  = day + 1;

	day_of_year = is_Jan_or_Feb ? day_of_year - 306 : day_of_year + 31 + 28 + is_leap_year;

	/* Converts to rtc_time's format. */
	tm->tm_year = (int)year;
	tm->tm_mon	= (int)month;
	tm->tm_mday = (int)day;
	tm->tm_yday = (int)day_of_year + 1;

	tm->tm_hour = secs / 3600;
	secs -= tm->tm_hour * 3600;
	tm->tm_min = secs / 60;
	tm->tm_sec = secs - tm->tm_min * 60;

	tm->tm_isdst = 0;
}

static u64 mktime64(const unsigned int year0, const unsigned int mon0, const unsigned int day, const unsigned int hour,
					const unsigned int min, const unsigned int sec)
{
	unsigned int mon = mon0, year = year0;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (int)(mon -= 2)) {
		mon += 12; /* Puts Feb last since it has leap day */
		year -= 1;
	}

	return ((((u64)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) + year * 365 - 719499) * 24 +
			 hour /* now have hours - midnight tomorrow handled here */
			 ) * 60 +
			min /* now have minutes */
			) * 60 +
		   sec; /* finally seconds */
}

u64 rtc_tm_to_time64(struct rtc_time *tm)
{
	return mktime64(((unsigned int)tm->tm_year), tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void rtc_init()
{
	uint32_t addr, val;
	addr = T113_R_CCU_BASE + CLK_BUS_R_RTC;

	val = read32(addr);
	val |= R_CCU_BIT_EN;
	write32(addr, val);

	val |= R_CCU_BIT_RST;
	write32(addr, val);

	// 	while (read32(addr) & R_CCU_BIT_RST) {
	// 	};

	rtc->losc_ctrl = (1 << 14) | (1 << 4);
	rtc->losc_ctrl |= 0x16aa0000;
	rtc->losc_ctrl |= 1;
}

void rtc_set_datetime(struct rtc_time *rtc_tm)
{
	u32 days;

	wait_for_time_write();
	mdelay(2);
	rtc->hms =
		TIME_SET_HOUR_VALUE(rtc_tm->tm_hour) | TIME_SET_MIN_VALUE(rtc_tm->tm_min) | TIME_SET_SEC_VALUE(rtc_tm->tm_sec);
	wait_for_time_write();
	mdelay(2);
	debug("RTC: set time %08X\r\n", TIME_SET_HOUR_VALUE(rtc_tm->tm_hour) | TIME_SET_MIN_VALUE(rtc_tm->tm_min) |
										TIME_SET_SEC_VALUE(rtc_tm->tm_sec));

	days = ((u64)(rtc_tm_to_time64(rtc_tm) / (u64)SECS_PER_DAY)) & 0xffff;
	debug("RTC: set days %lu\r\n", days);

	// while (rtc->days != days) {
	wait_for_date_write();
	mdelay(2);
	rtc->days = days;
	wait_for_date_write();
	mdelay(2);
	// }
}

void rtc_get_datetime(struct rtc_time *rtc_tm)
{
	u32 time = rtc->hms;
	u32 days = rtc->days;
	debug("RTC: got time %08X\r\n", time);
	debug("RTC: got days %lu\r\n", days);
	rtc_time64_to_tm(days * SECS_PER_DAY, rtc_tm);

	rtc_tm->tm_sec	= TIME_GET_SEC_VALUE(time);
	rtc_tm->tm_min	= TIME_GET_MIN_VALUE(time);
	rtc_tm->tm_hour = TIME_GET_HOUR_VALUE(time);
}
