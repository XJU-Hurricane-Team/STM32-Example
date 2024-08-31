/**
 * @file    rtc.c
 * @author  Deadline039
 * @brief   RTC时钟
 * @version 1.0
 * @date    2024-08-31
 * @note    为了方便移植, 使用C库time记录时间结构, 封装HAL的RTC函数
 *          硬件配置参考正点原子, 参见ST应用手册an3371
 * @warning 如果使用联网授时, 注意处理时区问题
 */

#include "rtc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

static RTC_HandleTypeDef rtc_handle;

#define RTC_USE_LSE 0x8800 /* 配置为外部低速时钟 */
#define RTC_USE_LSI 0x8801 /* 配置为内部低速时钟 */

/**
 * @brief RTC时钟初始化
 *
 */
void rtc_init(void) {
    HAL_StatusTypeDef res = HAL_OK;
    uint16_t bkp_flag = 0;

    /* 检查Backup的标志位是否清除 */
    bkp_flag = HAL_RTCEx_BKUPRead(&rtc_handle, RTC_BKP_DR1);

    rtc_handle.Instance = RTC;
    rtc_handle.Init.HourFormat = RTC_HOURFORMAT_24;
    rtc_handle.Init.AsynchPrediv = 0x7F;
    rtc_handle.Init.SynchPrediv = 0xFF;
    rtc_handle.Init.OutPut = RTC_OUTPUT_DISABLE;
    rtc_handle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    rtc_handle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    res = HAL_RTC_Init(&rtc_handle);

#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    if ((bkp_flag != RTC_USE_LSE) && (bkp_flag != RTC_USE_LSI)) {
        printf("RTC reseted! Reset to 1970-01-01 0:00:00\r\n");
        time_t init_time = 0;
        rtc_set_time_t(&init_time);
    }

    HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0xF, 0xF);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

/**
 * @brief RTC闹钟中断服务函数
 *
 */
void RTC_Alarm_IRQHandler(void) {
    HAL_RTC_AlarmIRQHandler(&rtc_handle);
}

/**
 * @brief RTC底层初始化
 *
 * @param hrtc RTC句柄
 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc) {
    uint16_t retry = 200;

    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    RCC_OscInitTypeDef rcc_osc_initstruct;
    RCC_PeriphCLKInitTypeDef rcc_periphclk_initstruct;

    RCC->BDCR |= 1 << 0; /* 开启外部低速振荡器LSE */

    while (retry && ((RCC->BDCR & 0X02) == 0)) {
        /* 等待LSE准备好 */
        retry--;
        HAL_Delay(5);
    }

    if (retry == 0) {
        /* LSE起振失败 使用LSI */
        rcc_osc_initstruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
        rcc_osc_initstruct.LSEState = RCC_LSI_ON;
        rcc_osc_initstruct.PLL.PLLState = RCC_PLL_NONE;
        HAL_RCC_OscConfig(&rcc_osc_initstruct);

        rcc_periphclk_initstruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        rcc_periphclk_initstruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
        HAL_RCCEx_PeriphCLKConfig(&rcc_periphclk_initstruct);
        HAL_RTCEx_BKUPWrite(hrtc, RTC_BKP_DR1, RTC_USE_LSI);
    } else {
        rcc_osc_initstruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
        rcc_osc_initstruct.LSEState = RCC_LSE_ON;
        rcc_osc_initstruct.PLL.PLLState = RCC_PLL_NONE;
        HAL_RCC_OscConfig(&rcc_osc_initstruct);

        rcc_periphclk_initstruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        rcc_periphclk_initstruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
        HAL_RCCEx_PeriphCLKConfig(&rcc_periphclk_initstruct);
        HAL_RTCEx_BKUPWrite(hrtc, RTC_BKP_DR1, RTC_USE_LSE);
    }
}

/* 月份天数表, 修正用 */
static const uint8_t month_day_table[12] = {31, 28, 31, 30, 31, 30,
                                            31, 31, 30, 31, 30, 31};

#define IS_LEAP_YEAR(YEAR)                                                     \
    (((((YEAR) % 4 == 0) && ((YEAR) % 100) != 0) || ((YEAR) % 400 == 0)) ? 1   \
                                                                         : 0)

/**
 * @brief 输入公历日期得到星期
 *
 * @param year 年
 * @param month 月
 * @param day 日
 * @return 0, 星期天; 1 ~ 6: 星期一 ~ 星期六
 * @note (起始时间为: 公元0年3月1日开始, 输入往后的任何日期,
 *       都可以获取正确的星期) 使用 基姆拉尔森计算公式 计算, 原理说明见此贴:
 *       https://www.cnblogs.com/fengbohello/p/3264300.html
 */
uint8_t rtc_get_week(uint16_t year, uint8_t month, uint8_t day) {
    uint8_t week = 0;

    if (month < 3) {
        month += 12;
        --year;
    }

    week = (day + 1 + 2 * month + 3 * (month + 1) / 5 + year + (year >> 2) -
            year / 100 + year / 400) %
           7;
    return week;
}

/**
 * @brief 获取时间戳
 *
 * @return 时间戳
 */
time_t rtc_get_time_t(void) {
    return time(NULL);
}

/**
 * @brief 获取RTC时钟时间
 *
 * @return 时间结构体
 */
struct tm *rtc_get_time(void) {
    static struct tm now_time;
    RTC_TimeTypeDef rtc_time;
    RTC_DateTypeDef rtc_date;
    HAL_RTC_GetTime(&rtc_handle, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&rtc_handle, &rtc_date, RTC_FORMAT_BIN);

    /* 这里的Year是0~99的范围, 也就是说从2000年到现在的年份.
     * 但是C库的tm_year定义为从1900年到现在的年份, 因此这里+100年 */
    now_time.tm_year = rtc_date.Year + 100;
    now_time.tm_mon = rtc_date.Month - 1;
    now_time.tm_mday = rtc_date.Date;
    now_time.tm_wday = rtc_date.WeekDay - 1;
    /* 计算今天是今年的第几天 */
    for (int i = 0; i < now_time.tm_mon; ++i) {
        now_time.tm_yday += month_day_table[i];
    }

    /* 如果是闰年而且月份大于2, 需要加1天 */
    if (now_time.tm_mon >= 2 && IS_LEAP_YEAR(1900 + now_time.tm_year)) {
        now_time.tm_yday += 1;
    }

    now_time.tm_isdst = HAL_RTC_DST_ReadStoreOperation(&rtc_handle);

    now_time.tm_hour = rtc_time.Hours;
    now_time.tm_min = rtc_time.Minutes;
    now_time.tm_sec = rtc_time.Seconds;

    return &now_time;
}

/**
 * @brief 通过时间戳设置时间
 *
 * @param _time 时间戳
 */
void rtc_set_time_t(const time_t *_time) {
    struct tm *time_struct = localtime(_time);
    rtc_set_time(time_struct);
}

/**
 * @brief 通过时间结构体设置时间
 *
 * @param _tm 时间结构体
 */
void rtc_set_time(const struct tm *_tm) {
    RTC_DateTypeDef rtc_date = {0};
    RTC_TimeTypeDef rtc_time = {0};

    rtc_date.Year = _tm->tm_year - 100;
    rtc_date.Month = _tm->tm_mon + 1;
    rtc_date.Date = _tm->tm_mday;
    rtc_date.WeekDay = _tm->tm_wday + 1;

    rtc_time.Hours = _tm->tm_hour;
    rtc_time.Minutes = _tm->tm_min;
    rtc_time.Seconds = _tm->tm_sec;

    /* 处理闰秒, 先减1, 然后延时1秒. 否则要处理进位比较麻烦 */
    if (_tm->tm_sec == 60) {
        rtc_time.Seconds = 59;
        HAL_Delay(1000);
    }

    HAL_RTC_SetDate(&rtc_handle, &rtc_date, RTC_FORMAT_BIN);
    HAL_RTC_SetTime(&rtc_handle, &rtc_time, RTC_FORMAT_BIN);

    if (_tm->tm_isdst) {
        /* 使用夏令时 */
        HAL_RTC_DST_SetStoreOperation(&rtc_handle);
    } else {
        HAL_RTC_DST_ClearStoreOperation(&rtc_handle);
    }
}

/**
 * @brief 获取闹钟时间戳
 *
 * @return 闹钟时间戳
 * @note F429有两个闹钟, 功能比较多. 为了与F1兼容,
 *       这里只是简单的获取闹钟A下一次提醒的时间戳
 */
time_t rtc_get_alarm_t(void) {}

/**
 * @brief 获取闹钟时间
 *
 * @return 闹钟时间结构体
 * @note F429有两个闹钟, 功能比较多. 为了与F1兼容,
 *       这里只是简单的获取闹钟A下一次提醒的时间
 */
struct tm *rtc_get_alarm(void) {
    if (__HAL_RTC_ALARM_GET_IT_SOURCE(&rtc_handle, RTC_IT_ALRA) != SET) {
        /* 没有开启闹钟A中断 */
        return NULL;
    }
}

/**
 * @brief 设置闹钟时间戳
 *
 * @param _time 闹钟时间戳
 * @note F429有两个闹钟, 功能比较多. 为了与F1兼容,
 *       这里只是简单的设置闹钟A下一次提醒的时间戳
 */
void rtc_set_alarm_t(const time_t *_time) {}

/**
 * @brief 设置闹钟时间
 *
 * @param _tm 时间结构体
 * @note F429有两个闹钟, 功能比较多. 为了与F1兼容,
 *       这里只是简单的设置闹钟A下一次提醒的时间
 */
void rtc_set_alarm(const struct tm *_tm) {}

/**
 * @brief RTC闹钟事件回调
 *
 * @param hrtc RTC句柄
 */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
    UNUSED(hrtc);
    printf("Alarm! \r\n");
}

/**
 * @brief 调用C库的time函数会链接此函数
 *
 * @param[out] _time 时间戳接收指针
 * @return time_t 当前时间戳
 */
time_t time(time_t *_time) {
    time_t now_time = mktime(rtc_get_time());
    if (_time != NULL) {
        *_time = now_time;
    }
    return now_time;
}