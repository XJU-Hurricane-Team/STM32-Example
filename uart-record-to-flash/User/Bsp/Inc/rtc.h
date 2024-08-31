/**
 * @file    rtc.h
 * @author  Deadline039
 * @brief   RTC时钟
 * @version 1.0
 * @date    2024-08-30
 */

#ifndef __RTC_H
#define __RTC_H

#include "stm32f1xx_hal.h"

#include <time.h>

void rtc_init(void);

time_t rtc_get_time_t(void);
struct tm *rtc_get_time(void);

void rtc_set_time_t(const time_t *_time);
void rtc_set_time(const struct tm *_tm);

time_t rtc_get_alarm_t(void);
struct tm *rtc_get_alarm(void);

void rtc_set_alarm_t(const time_t *_time);
void rtc_set_alarm(const struct tm *_tm);

#endif /* __RTC_H */
