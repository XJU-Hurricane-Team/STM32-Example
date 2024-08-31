/**
 * @file    rtc.c
 * @author  Deadline039
 * @brief   RTC时钟
 * @version 1.0
 * @date    2024-08-30
 * @note    使用C库time记录时间结构, 不单独定义结构体
 *          由于F1的RTC不像F4那样可以存储年月日, 掉电会丢失. F1的RTC更像时间戳.
 *          因此使用C库的时间戳日期转换函数, 直接操作RTC的寄存器,
 *          硬件配置参考正点原子的代码.
 *          stm32的RTC是32位的寄存器, 最大表示到约2105年
 * @warning 如果使用联网授时, 注意处理时区问题
 */

#include "rtc.h"

#include <assert.h>
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

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* 检查Backup的标志位是否清除 */
    bkp_flag = HAL_RTCEx_BKUPRead(&rtc_handle, RTC_BKP_DR1);

    rtc_handle.Instance = RTC;
    rtc_handle.Init.AsynchPrediv = 0x7FFF;
    rtc_handle.Init.OutPut = RTC_OUTPUTSOURCE_NONE;

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
    time_t time_seconds = time(NULL);
    return localtime(&time_seconds);
}

/**
 * @brief 通过时间戳设置时间
 *
 * @param _time 时间戳
 */
void rtc_set_time_t(const time_t *_time) {
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    /* 上面三步是必须的! */

    RTC->CRL |= 1 << 4; /* 允许配置 */

    RTC->CNTL = *_time & 0xffff;
    RTC->CNTH = *_time >> 16;

    RTC->CRL &= ~(1 << 4); /* 配置更新 */

    /* 等待RTC寄存器操作完成, 即等待RTOFF == 1 */
    while (!__HAL_RTC_ALARM_GET_FLAG(&rtc_handle, RTC_FLAG_RTOFF))
        ;
}

/**
 * @brief 通过时间结构体设置时间
 *
 * @param _tm 时间结构体
 */
void rtc_set_time(const struct tm *_tm) {
    time_t now_time;
    now_time = mktime(_tm);
    rtc_set_time_t(&now_time);
}

/**
 * @brief 获取闹钟时间戳
 *
 * @return 闹钟时间戳
 */
time_t rtc_get_alarm_t(void) {
    return (time_t)((RTC->ALRH << 16) | (RTC->ALRL & 0xffff));
}

/**
 * @brief 获取闹钟时间
 *
 * @return 时间结构体
 */
struct tm *rtc_get_alarm(void) {
    time_t alarm_time = rtc_get_alarm_t();
    return localtime(&alarm_time);
}

/**
 * @brief 通过时间戳设置闹钟
 *
 * @param _time 时间戳
 */
void rtc_set_alarm_t(const time_t *_time) {
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    /* 上面三步是必须的! */

    RTC->CRL |= 1 << 4; /* 允许配置 */

    RTC->ALRL = *_time & 0xffff;
    RTC->ALRH = *_time >> 16;

    RTC->CRL &= ~(1 << 4); /* 配置更新 */

    /* 等待RTC寄存器操作完成, 即等待RTOFF == 1 */
    while (!__HAL_RTC_ALARM_GET_FLAG(&rtc_handle, RTC_FLAG_RTOFF))
        ;
    /* 打开闹钟中断 */
    __HAL_RTC_ALARM_CLEAR_FLAG(&rtc_handle, RTC_FLAG_ALRAF);
    __HAL_RTC_ALARM_ENABLE_IT(&rtc_handle, RTC_IT_ALRA);

    __HAL_RTC_ALARM_EXTI_ENABLE_IT();
    __HAL_RTC_ALARM_EXTI_ENABLE_RISING_EDGE();
}

/**
 * @brief 通过时间结构体设置闹钟
 *
 * @param _tm 时间结构体
 */
void rtc_set_alarm(const struct tm *_tm) {
    time_t alarm_time;
    alarm_time = mktime(_tm);
    rtc_set_alarm_t(&alarm_time);
}

/**
 * @brief RTC闹钟事件回调
 *
 * @param hrtc RTC句柄
 */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
    UNUSED(hrtc);
    printf("Alarm \r\n");
}

/**
 * @brief 调用C库的time函数会链接此函数
 *
 * @param[out] _time 时间戳接收指针
 * @return time_t 当前时间戳
 */
time_t time(time_t *_time) {
    if (_time != NULL) {
        *_time = ((RTC->CNTH << 16) | (RTC->CNTL & 0xffff));
    }
    return (RTC->CNTH << 16) | (RTC->CNTL & 0xffff);
}
