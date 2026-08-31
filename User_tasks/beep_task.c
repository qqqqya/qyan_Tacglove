/**
 * @file beep_task.c
 * @brief 有源蜂鸣器上电自检任务。
 * @details 调度器启动后输出三次短鸣，然后关闭蜂鸣器并挂起自身。
 */
#include "beep_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_log.h"
#include "bsp_beep_handler.h"

#define BEEP_TASK_STACK_WORDS 96U                    /**< 96 words，即384 bytes。 */
#define BEEP_TASK_PRIORITY    (tskIDLE_PRIORITY + 1U) /**< 板级自检任务优先级。 */
#define BEEP_TEST_COUNT       3U                      /**< 上电短鸣次数。 */
#define BEEP_ON_TIME_MS       120U                    /**< 单次鸣叫时间。 */
#define BEEP_OFF_TIME_MS      180U                    /**< 两次鸣叫间隔。 */

/** @brief 让当前任务阻塞指定毫秒，阻塞期间CPU可运行其他任务。 */
static void beep_delay(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/**
 * @brief 执行三次短鸣的上电自检。
 * @retval true 每一次启停操作都成功。
 * @retval false 任意一次Handler调用失败。
 */
static bool beep_run_startup_test(void)
{
    for (uint8_t count = 0U; count < BEEP_TEST_COUNT; ++count)
    {
        bsp_beep_status_t status = bsp_beep_handler_start();
        if (status != BSP_BEEP_STATUS_OK)
        {
            APP_LOG_ERROR("beep start failed: count=%u status=%d",
                          (unsigned int)count,
                          (int)status);
            return false;
        }

        APP_LOG_INFO("beep started: count=%u", (unsigned int)count);
        beep_delay(BEEP_ON_TIME_MS);

        status = bsp_beep_handler_stop();
        if (status != BSP_BEEP_STATUS_OK)
        {
            APP_LOG_ERROR("beep stop failed: count=%u status=%d",
                          (unsigned int)count,
                          (int)status);
            return false;
        }

        APP_LOG_INFO("beep stopped: count=%u", (unsigned int)count);
        beep_delay(BEEP_OFF_TIME_MS);
    }

    return true;
}

/**
 * @brief 蜂鸣器任务入口。
 * @param argument FreeRTOS任务参数，当前未使用。
 */
static void beep_task_entry(void *argument)
{
    (void)argument;

    bsp_beep_status_t status = bsp_beep_handler_init();
    if (status != BSP_BEEP_STATUS_OK)
    {
        APP_LOG_ERROR("beep init failed: status=%d", (int)status);
    }
    else
    {
        APP_LOG_INFO("beep init passed");
        if (!beep_run_startup_test())
        {
            APP_LOG_ERROR("beep startup test failed");
        }
        else
        {
            APP_LOG_INFO("beep startup test passed");
        }
    }

    /* 无论自检是否成功，都再次请求关闭，避免错误路径留下持续鸣叫。 */
    (void)bsp_beep_handler_stop();

    /* 当前关闭vTaskDelete，任务完成后挂起自身并保留调试现场。 */
    for (;;)
    {
        vTaskSuspend(NULL);
    }
}

bool beep_task_create(void)
{
    const BaseType_t result = xTaskCreate(beep_task_entry,
                                          "beep",
                                          BEEP_TASK_STACK_WORDS,
                                          NULL,
                                          BEEP_TASK_PRIORITY,
                                          NULL);
    if (result != pdPASS)
    {
        APP_LOG_ERROR("beep task create failed: free_heap=%u",
                      (unsigned int)xPortGetFreeHeapSize());
        return false;
    }

    APP_LOG_INFO("beep task created: free_heap=%u",
                 (unsigned int)xPortGetFreeHeapSize());
    return true;
}
