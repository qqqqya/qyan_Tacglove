/**
 * @file key_task.h
 * @brief 数据采集按键扫描任务创建接口。
 */
#ifndef KEY_TASK_H
#define KEY_TASK_H

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建PB6按键扫描与消抖任务。
 * @retval TASK_OK 任务创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS heap不足。
 */
task_status_t key_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* KEY_TASK_H */
