/**
 * @file    rtc.h
 * @author  Deadline039
 * @brief   RTC时钟
 * @version 1.0
 * @date    2024-08-31
 * @note    F4的RTC闹钟比较复杂, 建议使用HAL库的RTC闹钟函数配置
 */

#ifndef __RTC_H
#define __RTC_H

#include "stm32f4xx_hal.h"

#include <time.h>

void rtc_init(void);

uint8_t rtc_get_week(uint16_t year, uint8_t month, uint8_t day);

time_t rtc_get_time_t(void);
struct tm *rtc_get_time(void);

void rtc_set_time_t(const time_t *_time);
void rtc_set_time(const struct tm *_tm);

#endif /* __RTC_H */
