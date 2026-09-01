/**
 * @file system_event.h
 * @brief 应用任务之间的系统事件队列接口。
 * @details 当前由按键任务发布采集按键事件，由LED状态任务消费并驱动
 * 采集状态机。BSP层不依赖FreeRTOS，保持硬件访问与任务通信解耦。
 */
#ifndef SYSTEM_EVENT_H
#define SYSTEM_EVENT_H

#include <stdint.h>

#include "FreeRTOS.h"

#include "task_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 应用层可传递的事件类型。 */
typedef enum
{
    SYSTEM_EVENT_NONE = 0,              /**< 无事件，仅用于初始化。 */
    SYSTEM_EVENT_CAPTURE_KEY_PRESSED = 1 /**< PB6按键完成消抖后被按下一次。 */
} system_event_type_t;

/** @brief 系统事件队列中的单条消息。 */
typedef struct
{
    system_event_type_t type; /**< 事件类型。 */
} system_event_t;

/**
 * @brief 创建系统事件队列。
 * @retval TASK_OK 队列创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS heap不足。
 */
task_status_t system_event_init(void);

/**
 * @brief 向系统事件队列投递事件。
 * @param event 待发送事件。
 * @param timeout_ticks 队列满时允许等待的RTOS Tick数。
 * @retval TASK_OK 事件已进入队列。
 * @retval TASK_ERROR_RESOURCE 队列尚未初始化。
 * @retval TASK_ERROR_TIMEOUT 在限定时间内未能写入队列。
 * @retval TASK_ERROR_PARAMETER event为空指针。
 */
task_status_t system_event_publish(const system_event_t *event,
                                   TickType_t timeout_ticks);

/**
 * @brief 等待并取出一条系统事件。
 * @param[out] event 用于保存收到的事件。
 * @param timeout_ticks 最长等待时间，可使用portMAX_DELAY永久等待。
 * @retval TASK_OK 成功收到事件。
 * @retval TASK_ERROR_TIMEOUT 等待超时。
 * @retval TASK_ERROR_RESOURCE 队列尚未初始化。
 * @retval TASK_ERROR_PARAMETER event为空指针。
 */
task_status_t system_event_wait(system_event_t *event,
                                TickType_t timeout_ticks);

/**
 * @brief 清空尚未处理的系统事件。
 * @details 用于上电自检结束时丢弃自检期间的误触发，确保进入待机后必须
 * 重新完整按下一次按键才会开始采集。
 */
void system_event_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_EVENT_H */
