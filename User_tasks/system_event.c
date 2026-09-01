/**
 * @file system_event.c
 * @brief 基于FreeRTOS Queue的应用事件通道实现。
 */
#include "system_event.h"

#include "queue.h"

/** @brief 按键event    最多缓存4次按键事件，避免短时间任务调度延迟造成事件丢失。 */
#define SYSTEM_EVENT_QUEUE_LENGTH 2U

/** @brief 系统事件队列句柄，仅在本模块内部持有。 */
static QueueHandle_t s_system_event_queue;

task_status_t system_event_init(void)
{
    s_system_event_queue = xQueueCreate(SYSTEM_EVENT_QUEUE_LENGTH,
                                        sizeof(system_event_t));
    if (NULL == s_system_event_queue)
    {
        return TASK_ERROR_NO_MEMORY;
    }

    return TASK_OK;
}

task_status_t system_event_publish(const system_event_t *event,
                                   TickType_t timeout_ticks)
{
    if (NULL == event)
    {
        return TASK_ERROR_PARAMETER;
    }

    if (NULL == s_system_event_queue)
    {
        return TASK_ERROR_RESOURCE;
    }

    if (pdPASS != xQueueSend(s_system_event_queue, event, timeout_ticks))
    {
        return TASK_ERROR_TIMEOUT;
    }

    return TASK_OK;
}

task_status_t system_event_wait(system_event_t *event,
                                TickType_t timeout_ticks)
{
    if (NULL == event)
    {
        return TASK_ERROR_PARAMETER;
    }

    if (NULL == s_system_event_queue)
    {
        return TASK_ERROR_RESOURCE;
    }

    if (pdPASS != xQueueReceive(s_system_event_queue, event, timeout_ticks))
    {
        return TASK_ERROR_TIMEOUT;
    }

    return TASK_OK;
}

void system_event_clear(void)
{
    if (NULL != s_system_event_queue)
    {
        (void)xQueueReset(s_system_event_queue);
    }
}
