/**
 * @file task_manager.c
 * @brief FreeRTOS应用任务集中创建实现。
 */
#include "task_manager.h"

#include "beep_task.h"
#include "led_task.h"

task_status_t task_manager_init(void)
{
    task_status_t result = led_task_create();
    if (TASK_OK != result)
    {
        /* 日志预留：记录LED任务注册失败及result。 */
        return result;
    }

    result = beep_task_create();
    if (TASK_OK != result)
    {
        /* 日志预留：记录蜂鸣器任务注册失败及result。 */
        return result;
    }

    return TASK_OK;
}
