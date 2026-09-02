/**
 * @file communication_task.h
 * @brief 阶段3 MCU二进制业务通信任务创建接口。
 */
#ifndef COMMUNICATION_TASK_H
#define COMMUNICATION_TASK_H

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建USART2二进制协议通信任务。
 * @retval TASK_OK 任务创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS heap不足。
 */
task_status_t communication_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMUNICATION_TASK_H */
