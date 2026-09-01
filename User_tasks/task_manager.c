/**
 * @file task_manager.c
 * @brief FreeRTOS应用任务集中创建实现。
 */
#include "task_manager.h"

#include "key_task.h"
#include "led_task.h"
#include "system_event.h"

task_status_t task_manager_init(void)
{
    /* 任务启动前先创建通信队列，避免生产者访问尚未初始化的句柄。 */
    task_status_t result = system_event_init();
    if (TASK_OK != result)
    {
        /* 日志预留：记录系统事件队列创建失败及result。 */
        return result;
    }

    result = led_task_create();
    if (TASK_OK != result)
    {
        /* 日志预留：记录LED任务注册失败及result。 */
        return result;
    }

    result = key_task_create();
    if (TASK_OK != result)
    {
        /* 日志预留：记录按键扫描任务注册失败及result。 */
        return result;
    }

    return TASK_OK;
}
