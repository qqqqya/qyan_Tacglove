/**
 * @file beep_task.h
 * @brief 蜂鸣器应用任务创建接口。
 */
#ifndef BEEP_TASK_H
#define BEEP_TASK_H

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建蜂鸣器上电自检任务。
 * @retval TASK_OK FreeRTOS任务创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS heap不足。
 * @retval TASK_ERROR 其他任务创建错误。
 */
task_status_t beep_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* BEEP_TASK_H */
