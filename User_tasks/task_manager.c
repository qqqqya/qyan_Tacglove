/**
 * @file task_manager.c
 * @brief FreeRTOS应用任务集中创建实现。
 */
#include "task_manager.h"

#include "app_log.h"
#include "beep_task.h"
#include "led_task.h"

bool task_manager_init(void)
{
    bool result = led_task_create();
    if (!result)
    {
        APP_LOG_ERROR("LED task registration failed");
        return false;
    }
    APP_LOG_INFO("LED task registration passed");

    result = beep_task_create();
    if (!result)
    {
        APP_LOG_ERROR("beep task registration failed");
        return false;
    }
    APP_LOG_INFO("beep task registration passed");

    return true;
}
