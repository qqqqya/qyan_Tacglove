/**
 * @file task_manager.h
 * @brief FreeRTOS应用任务集中注册接口。
 */
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建固件需要的全部应用任务。
 * @retval true 所有任务均创建成功。
 * @retval false 至少一个任务创建失败。
 * @note 本函数相当于CubeMX工程中的MX_FREERTOS_Init()。
 */
bool task_manager_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MANAGER_H */
