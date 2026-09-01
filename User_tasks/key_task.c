/**
 * @file key_task.c
 * @brief PB6数据采集按键的周期扫描、软件消抖和事件发布任务。
 * @details 每20 ms读取一次按键；原始状态连续3次一致才确认变化，等效
 * 消抖时间约60 ms。一次完整的按下只发布一条事件，长按不会重复触发。
 */
#include "key_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_key_handler.h"
#include "system_event.h"

#define KEY_TASK_STACK_WORDS       64U                    /**< 64 words，即256 bytes。 */
#define KEY_TASK_PRIORITY          (tskIDLE_PRIORITY + 2U) /**< 高于显示任务，及时采样。 */
#define KEY_SCAN_PERIOD_MS         20U                    /**< GPIO扫描周期。 */
#define KEY_DEBOUNCE_SAMPLE_COUNT  3U                     /**< 连续3次一致后确认变化。 */

/**
 * @brief 按键扫描任务入口。
 * @param argument FreeRTOS任务参数，当前未使用。
 */
static void key_task_entry(void *argument)
{
    (void)argument;

    bool stable_pressed = false;
    bool previous_sample = false;
    uint8_t equal_sample_count = 0U;

    if (KEY_HANDLER_OK != bsp_key_handler_init())
    {
        /* GPIO资源异常时保留任务现场，避免发布不可信的按键事件。 */
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    for (;;)
    {
        bool current_sample = false;
        const key_handler_status_t key_status =
            bsp_key_handler_is_pressed(&current_sample);

        if (KEY_HANDLER_OK == key_status)
        {
            if (current_sample == previous_sample)
            {
                if (equal_sample_count < KEY_DEBOUNCE_SAMPLE_COUNT)
                {
                    ++equal_sample_count;
                }
            }
            else
            {
                previous_sample = current_sample;
                equal_sample_count = 1U;
            }

            if ((equal_sample_count >= KEY_DEBOUNCE_SAMPLE_COUNT) &&
                (stable_pressed != current_sample))
            {
                stable_pressed = current_sample;

                if (stable_pressed)
                {
                    const system_event_t event = {
                        .type = SYSTEM_EVENT_CAPTURE_KEY_PRESSED};

                    /* 已做消抖且长按只产生一次事件，队列正常情况下不会满。 */
                    (void)system_event_publish(&event, 0U);
                }
            }
        }
        else
        {
            /* 读取失败时重新开始消抖，禁止沿用不连续的历史样本。 */
            equal_sample_count = 0U;
        }

        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_PERIOD_MS));
    }
}

task_status_t key_task_create(void)
{
    const BaseType_t result = xTaskCreate(key_task_entry,
                                          "key_Capdata",
                                          KEY_TASK_STACK_WORDS,
                                          NULL,
                                          KEY_TASK_PRIORITY,
                                          NULL);
    if (pdPASS != result)
    {
        return TASK_ERROR_NO_MEMORY;
    }

    return TASK_OK;
}
