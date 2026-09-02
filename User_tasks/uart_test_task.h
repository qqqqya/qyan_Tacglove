/**
 * @file uart_test_task.h
 * @brief 阶段1 USART2 DMA通信验证任务创建接口。
 */
#ifndef UART_TEST_TASK_H
#define UART_TEST_TASK_H

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建UART DMA阶段1测试任务。
 * @retval TASK_OK 任务创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS Heap不足。
 */
task_status_t uart_test_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_TEST_TASK_H */
