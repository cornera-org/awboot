// SPDX-License-Identifier: MIT
// Copyright (c) 2021 Ulrich Hecht

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((__packed__)) {
	uint32_t losc_ctrl; // 0x0
	uint32_t losc_auto; // 0x4
	uint32_t losc_prescal; // 0x8
	uint32_t days; // 0x10
	uint32_t hms; // 0x14
} rtc_t;

struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

void rtc_set_datetime(struct rtc_time *rtc_tm);
void rtc_get_datetime(struct rtc_time *rtc_tm);

void rtc_init(void);

#ifdef __cplusplus
}
#endif
