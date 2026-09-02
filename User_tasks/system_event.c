/**
 * @file system_event.c
 * @brief 基于FreeRTOS Queue的应用事件通道实现。
 */
#include "system_event.h"

#include "queue.h"

/** @brief 控制队列最多缓存4个事件。 */
#define SYSTEM_CONTROL_QUEUE_LENGTH 4U

/** @brief 通信观察队列最多缓存4个物理按键事件。 */
#define SYSTEM_OBSERVER_QUEUE_LENGTH 4U

/** @brief LED状态机消费的控制队列。 */
static QueueHandle_t s_control_queue;

/** @brief 通信任务消费的物理输入观察队列。 */
static QueueHandle_t s_observer_queue;

task_status_t system_event_init(void)
{
    s_control_queue = xQueueCreate(SYSTEM_CONTROL_QUEUE_LENGTH,
                                   sizeof(system_event_t));
    s_observer_queue = xQueueCreate(SYSTEM_OBSERVER_QUEUE_LENGTH,
                                    sizeof(system_event_t));
    if ((NULL == s_control_queue) || (NULL == s_observer_queue))
    {
        return TASK_ERROR_NO_MEMORY;
    }

    return TASK_OK;
}

task_status_t system_event_publish(const system_event_t *event,
                                   TickType_t timeout_ticks)
{
    if (NULL == s_observer_queue)
    {
        return TASK_ERROR_RESOURCE;
    }

    const task_status_t control_status =
        system_event_publish_control(event, timeout_ticks);
    if (TASK_OK != control_status)
    {
        return control_status;
    }

    if (pdPASS != xQueueSend(s_observer_queue, event, timeout_ticks))
    {
        return TASK_ERROR_TIMEOUT;
    }

    return TASK_OK;
}

task_status_t system_event_publish_control(const system_event_t *event,
                                           TickType_t timeout_ticks)
{
    if (NULL == event)
    {
        return TASK_ERROR_PARAMETER;
    }

    if (NULL == s_control_queue)
    {
        return TASK_ERROR_RESOURCE;
    }

    if (pdPASS != xQueueSend(s_control_queue, event, timeout_ticks))
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

    if (NULL == s_control_queue)
    {
        return TASK_ERROR_RESOURCE;
    }

    if (pdPASS != xQueueReceive(s_control_queue, event, timeout_ticks))
    {
        return TASK_ERROR_TIMEOUT;
    }

    return TASK_OK;
}

task_status_t system_event_observe(system_event_t *event,
                                   TickType_t timeout_ticks)
{
    if (NULL == event)
    {
        return TASK_ERROR_PARAMETER;
    }

    if (NULL == s_observer_queue)
    {
        return TASK_ERROR_RESOURCE;
    }

    if (pdPASS != xQueueReceive(s_observer_queue, event, timeout_ticks))
    {
        return TASK_ERROR_TIMEOUT;
    }

    return TASK_OK;
}

void system_event_clear(void)
{
    if (NULL != s_control_queue)
    {
        (void)xQueueReset(s_control_queue);
    }

    if (NULL != s_observer_queue)
    {
        (void)xQueueReset(s_observer_queue);
    }
}
