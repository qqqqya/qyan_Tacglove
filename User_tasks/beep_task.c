/**
 * @file beep_task.c
 * @brief 有源蜂鸣器上电自检任务。
 * @details 调度器启动后输出三次短鸣，然后关闭蜂鸣器并挂起自身。
 */
#include "beep_task.h"

#include "FreeRTOS.h"
#include "task.h"

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
 * @retval HANDLER_BEEP_OK 每一次启停操作都成功。
 * @return 其他蜂鸣器Handler状态表示对应步骤失败。
 */
static beep_handler_status_t beep_run_startup_test(void)
{
    for (uint8_t count = 0U; count < BEEP_TEST_COUNT; ++count)
    {
        beep_handler_status_t status = bsp_beep_handler_start();
        if (HANDLER_BEEP_OK != status)
        {
            return status;
        }

        beep_delay(BEEP_ON_TIME_MS);

        status = bsp_beep_handler_stop();
        if (HANDLER_BEEP_OK != status)
        {
            return status;
        }

        beep_delay(BEEP_OFF_TIME_MS);
    }

    return HANDLER_BEEP_OK;
}

/**
 * @brief 蜂鸣器任务入口。
 * @param argument FreeRTOS任务参数，当前未使用。
 */
static void beep_task_entry(void *argument)
{
    (void)argument;
    beep_handler_status_t status = bsp_beep_handler_init();
    if (HANDLER_BEEP_OK != status)
    {
        /* 日志预留：记录蜂鸣器Handler初始化失败及status。 */
    }
    else
    {
        status = beep_run_startup_test();
        if (HANDLER_BEEP_OK != status)
        {
            /* 日志预留：记录蜂鸣器自检步骤失败及status。 */
        }
    }

    /* 无论自检是否成功，都再次请求关闭，避免错误路径留下持续鸣叫。 */
    status = bsp_beep_handler_stop();
    if (HANDLER_BEEP_OK != status)
    {
        /* 日志预留：记录蜂鸣器安全关闭失败及status。 */
    }

    /* 当前关闭vTaskDelete，任务完成后挂起自身并保留调试现场。 */
    for (;;)
    {
        vTaskSuspend(NULL);
    }
}

task_status_t beep_task_create(void)
{
    const BaseType_t result = xTaskCreate(beep_task_entry,
                                          "beep",
                                          BEEP_TASK_STACK_WORDS,
                                          NULL,
                                          BEEP_TASK_PRIORITY,
                                          NULL);
    if (result != pdPASS)
    {
        /* 日志预留：记录蜂鸣器任务创建失败和FreeRTOS heap余量。 */
        return TASK_ERROR_NO_MEMORY;
    }

    return TASK_OK;
}
