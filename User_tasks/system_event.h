/**
 * @file system_event.h
 * @brief 应用控制事件及通信观察事件接口。
 * @details 按键任务把明确的开始/停止请求同时投递给LED状态机和通信观察
 * 队列；PC命令只进入LED控制队列，防止被误报为物理按键事件。
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
    SYSTEM_EVENT_NONE = 0,                /**< 无事件，仅用于初始化。 */
    SYSTEM_EVENT_CAPTURE_START_REQUEST = 1, /**< 请求开始采集。 */
    SYSTEM_EVENT_CAPTURE_STOP_REQUEST = 2,  /**< 请求停止采集。 */
    SYSTEM_EVENT_BEEP_SHORT_REQUEST = 3     /**< 请求蜂鸣器短鸣一次。 */
} system_event_type_t;

/** @brief 系统事件队列中的单条消息。 */
typedef struct
{
    system_event_type_t type; /**< 事件类型。 */
    uint32_t timestamp_ms;    /**< 事件产生时的MCU单调运行时间。 */
} system_event_t;

/**
 * @brief 创建系统事件队列。
 * @retval TASK_OK 队列创建成功。
 * @retval TASK_ERROR_NO_MEMORY FreeRTOS heap不足。
 */
task_status_t system_event_init(void);

/**
 * @brief 投递一个物理输入事件。
 * @param event 待发送事件。
 * @param timeout_ticks 队列满时允许等待的RTOS Tick数。
 * @details 事件先进入LED控制队列，再复制到通信观察队列。只允许按键任务
 * 调用本接口；上位机命令应调用system_event_publish_control()。
 * @retval TASK_OK 事件已进入队列。
 * @retval TASK_ERROR_RESOURCE 队列尚未初始化。
 * @retval TASK_ERROR_TIMEOUT 在限定时间内未能写入队列。
 * @retval TASK_ERROR_PARAMETER event为空指针。
 */
task_status_t system_event_publish(const system_event_t *event,
                                   TickType_t timeout_ticks);

/**
 * @brief 只向LED状态机投递控制事件，不生成物理按键上报。
 * @param event 待发送事件。
 * @param timeout_ticks 队列满时允许等待的RTOS Tick数。
 * @return TASK_OK表示已进入控制队列，其他值表示参数、资源或超时错误。
 */
task_status_t system_event_publish_control(const system_event_t *event,
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
 * @brief 通信任务读取一条物理输入观察事件。
 * @param[out] event 保存按键任务复制的事件。
 * @param timeout_ticks 最长等待时间；通信轮询通常传0。
 * @return TASK_OK表示收到事件，其他值表示参数、资源或超时错误。
 */
task_status_t system_event_observe(system_event_t *event,
                                   TickType_t timeout_ticks);

/**
 * @brief 清空控制队列和通信观察队列中尚未处理的系统事件。
 * @details 用于上电自检结束时丢弃自检期间的误触发，确保进入待机后必须
 * 重新完整按下一次按键才会开始采集。
 */
void system_event_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_EVENT_H */
