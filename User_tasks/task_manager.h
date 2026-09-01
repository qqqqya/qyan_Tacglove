/**
 * @file task_manager.h
 * @brief FreeRTOS应用任务集中注册接口。
 */
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建固件需要的全部应用任务。
 * @retval TASK_OK 所有任务均创建成功。
 * @retval TASK_ERROR_NO_MEMORY 任一任务因FreeRTOS heap不足创建失败。
 * @retval TASK_ERROR 其他任务创建错误。
 * @note 本函数相当于CubeMX工程中的MX_FREERTOS_Init()。
 */
task_status_t task_manager_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MANAGER_H */
