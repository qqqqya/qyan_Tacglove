/**
 * @file led_task.h
 * @brief SK6805板级自检任务创建接口。
 */
#ifndef LED_TASK_H
#define LED_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建SK6805板级自检任务。
 * @retval true FreeRTOS任务创建成功。
 * @retval false FreeRTOS heap不足或任务创建失败。
 */
bool led_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_TASK_H */
