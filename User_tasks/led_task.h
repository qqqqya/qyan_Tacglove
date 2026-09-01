/**
 * @file led_task.h
 * @brief SK6805板级自检任务创建接口。
 */
#ifndef LED_TASK_H
#define LED_TASK_H

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建SK6805板级自检任务。
 * @retval TASK_OK FreeRTOS任务创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS heap不足。
 * @retval TASK_ERROR 其他任务创建错误。
 */
task_status_t led_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_TASK_H */
