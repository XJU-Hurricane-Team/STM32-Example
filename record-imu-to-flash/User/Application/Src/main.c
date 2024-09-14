/**
 * @file    main.c
 * @author  Deadline039
 * @brief   主函数
 * @version 1.0
 * @date    2024-07-29
 */

#include "includes.h"

void rtc_key_set_time(UART_HandleTypeDef *huart);

/**
 * @brief 主函数
 *
 * @return 退出码
 */
int main(void) {
    bsp_init();
    rtc_key_set_time(&usart1_handle);

    char local_time_buffer[50];
    struct tm *now_time;

    while (1) {
        now_time = rtc_get_time();
        strftime(local_time_buffer, sizeof(local_time_buffer),
                 "%Y-%m-%d %H:%M:%S", now_time);
        puts(local_time_buffer);
        delay_ms(1000);
    }
}

/**
 * @brief 设置RTC时间
 *
 * @param huart 串口句柄
 */
void rtc_key_set_time(UART_HandleTypeDef *huart) {
    uart_printf(huart, "Press KEY0 to set time, wait 1 seconds...\r\n");

    uint32_t start_time = HAL_GetTick();

    key_press_t key;

    do {
        key = key_scan(0);
        if (key == KEY0_PRESS) {
            break;
        }
    } while (HAL_GetTick() - start_time < 1000);

    if (HAL_GetTick() - start_time >= 1000) {
        return;
    }

    start_time = HAL_GetTick();
    uart_printf(
        huart, "Please input date & time, format: YYYY-MM-DD HH:MM:SS, wait 10 "
               "seconds...\r\n");

    char buffer[50];
    struct tm input_time = {0};

    do {
        /* 这里是用DMA读取缓冲区, 可以换成你们自己的方式 */
        if (uart_dmarx_read(huart, buffer, sizeof(buffer))) {
            sscanf(buffer, "%d-%d-%d %d:%d:%d", &input_time.tm_year,
                   &input_time.tm_mon, &input_time.tm_mday, &input_time.tm_hour,
                   &input_time.tm_min, &input_time.tm_sec);
            break;
        }
    } while (HAL_GetTick() - start_time < 10000);

    if (HAL_GetTick() - start_time >= 10000) {
        uart_printf(huart, "Wait input timeout. \r\n");
    } else {
        input_time.tm_year -= 1900;
        input_time.tm_mon -= 1;
        rtc_set_time(&input_time);
        uart_printf(huart, "Time set. \r\n");
    }
}
