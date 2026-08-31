/**
 * @file led_task.c
 * @brief 七颗SK6805指示灯循环板级自检任务。
 * @details
 * 任务循环显示全色、逐路相机灯、鱼眼灯和系统灯，用于验证灯链通信、
 * RGB顺序以及LED1~LED7物理映射。当前灯效是开发自检，不是产品状态机。
 */
#include "led_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "app_log.h"
#include "bsp_led_handler.h"

#define LED_TASK_STACK_WORDS      128U                    /**< 128 words，即512 bytes。 */
#define LED_TASK_PRIORITY         (tskIDLE_PRIORITY + 1U) /**< 板级显示任务优先级。 */
#define LED_DEMO_COLOR_HOLD_MS    500U                    /**< 全色画面保持时间。 */
#define LED_DEMO_CHASE_HOLD_MS    250U                    /**< 跑马灯单步保持时间。 */
#define LED_DEMO_BLINK_HOLD_MS    200U                    /**< 系统灯闪烁半周期。 */
#define LED_DEMO_FINAL_HOLD_MS    1200U                   /**< 组合画面保持时间。 */
#define LED_ERROR_RETRY_DELAY_MS  1000U                   /**< 故障后任务检查周期。 */

/** @brief 让当前任务阻塞指定毫秒，阻塞期间CPU可运行其他任务。 */
static void led_delay(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/**
 * @brief 将七颗灯设置为同一颜色并立即刷新。
 * @param color 七颗灯共同显示的颜色。
 * @retval true 每一步设置和发送都成功。
 * @retval false 任意一步失败。
 */
static bool led_show_all(bsp_led_color_t color)
{
    bsp_led_driver_clear();

    if (!bsp_led_handler_set_all_cameras(color))
    {
        APP_LOG_ERROR("LED set all cameras failed");
        return false;
    }

    if (!bsp_led_handler_set_fisheye(color))
    {
        APP_LOG_ERROR("LED set fisheye failed");
        return false;
    }

    if (!bsp_led_handler_set_system(color))
    {
        APP_LOG_ERROR("LED set system failed");
        return false;
    }

    if (!bsp_led_handler_commit())
    {
        APP_LOG_ERROR("LED frame commit failed");
        return false;
    }

    return true;
}

/**
 * @brief 运行一轮完整的七灯板级自检。
 * @retval true 本轮所有画面均发送成功。
 * @retval false 任意一次底层操作失败。
 */
static bool led_run_demo_cycle(void)
{
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};
    const bsp_led_color_t red = {.red = 24U, .green = 0U, .blue = 0U};
    const bsp_led_color_t green = {.red = 0U, .green = 24U, .blue = 0U};
    const bsp_led_color_t blue = {.red = 0U, .green = 0U, .blue = 24U};
    const bsp_led_color_t white = {.red = 16U, .green = 16U, .blue = 16U};
    const bsp_led_color_t magenta = {.red = 24U, .green = 0U, .blue = 24U};
    const bsp_led_color_t cyan = {.red = 0U, .green = 24U, .blue = 24U};

    APP_LOG_INFO("LED demo: all red");
    if (!led_show_all(red))
    {
        return false;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    APP_LOG_INFO("LED demo: all green");
    if (!led_show_all(green))//所有灯变绿
    {
        return false;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    APP_LOG_INFO("LED demo: all blue");
    if (!led_show_all(blue))
    {
        return false;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    APP_LOG_INFO("LED demo: all off");
    if (!led_show_all(off))
    {
        return false;
    }
    led_delay(LED_DEMO_CHASE_HOLD_MS);

    for (uint8_t camera = 0U; camera < BSP_LED_CAMERA_COUNT; ++camera)
    {
        bsp_led_driver_clear();

        if (!bsp_led_handler_set_camera((bsp_led_camera_id_t)camera, white))
        {
            APP_LOG_ERROR("LED camera set failed: camera=%u", (unsigned int)camera);
            return false;
        }

        if (!bsp_led_handler_commit())
        {
            APP_LOG_ERROR("LED camera commit failed: camera=%u", (unsigned int)camera);
            return false;
        }

        APP_LOG_INFO("LED demo: camera=%u", (unsigned int)camera);
        led_delay(LED_DEMO_CHASE_HOLD_MS);
    }

    bsp_led_driver_clear();
    if (!bsp_led_handler_set_fisheye(magenta))
    {
        APP_LOG_ERROR("LED fisheye set failed");
        return false;
    }
    if (!bsp_led_handler_commit())
    {
        APP_LOG_ERROR("LED fisheye commit failed");
        return false;
    }
    APP_LOG_INFO("LED demo: fisheye");
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    for (uint8_t blink = 0U; blink < 3U; ++blink)
    {
        bsp_led_driver_clear();
        if (!bsp_led_handler_set_system(cyan))
        {
            APP_LOG_ERROR("LED system set failed: blink=%u", (unsigned int)blink);
            return false;
        }
        if (!bsp_led_handler_commit())
        {
            APP_LOG_ERROR("LED system commit failed: blink=%u", (unsigned int)blink);
            return false;
        }
        APP_LOG_INFO("LED demo: system on blink=%u", (unsigned int)blink);
        led_delay(LED_DEMO_BLINK_HOLD_MS);

        if (!led_show_all(off))
        {
            return false;
        }
        led_delay(LED_DEMO_BLINK_HOLD_MS);
    }

    bsp_led_driver_clear();
    if (!bsp_led_handler_set_all_cameras(green))
    {
        APP_LOG_ERROR("LED final cameras set failed");
        return false;
    }
    if (!bsp_led_handler_set_fisheye(blue))
    {
        APP_LOG_ERROR("LED final fisheye set failed");
        return false;
    }
    if (!bsp_led_handler_set_system(white))
    {
        APP_LOG_ERROR("LED final system set failed");
        return false;
    }
    if (!bsp_led_handler_commit())
    {
        APP_LOG_ERROR("LED final frame commit failed");
        return false;
    }

    APP_LOG_INFO("LED demo cycle passed");
    led_delay(LED_DEMO_FINAL_HOLD_MS);
    return true;
}

/**
 * @brief LED任务入口。
 * @param argument FreeRTOS任务参数，当前未使用。
 */
static void led_task_entry(void *argument)
{
    (void)argument;

    if (!bsp_led_handler_init())
    {
        APP_LOG_ERROR("LED handler init failed");
    }
    else
    {
        APP_LOG_INFO("LED handler init passed");
        for (;;)
        {
            if (!led_run_demo_cycle())
            {
                APP_LOG_ERROR("LED demo cycle failed");
                break;
            }
        }
    }

    /* LED故障不再关闭全局中断，避免同时冻结蜂鸣器及其他任务。 */
    for (;;)
    {
        led_delay(LED_ERROR_RETRY_DELAY_MS);
    }
}

bool led_task_create(void)
{
    const BaseType_t result = xTaskCreate(led_task_entry,
                                          "led",
                                          LED_TASK_STACK_WORDS,
                                          NULL,
                                          LED_TASK_PRIORITY,
                                          NULL);
    if (result != pdPASS)
    {
        APP_LOG_ERROR("LED task create failed: free_heap=%u",
                      (unsigned int)xPortGetFreeHeapSize());
        return false;
    }

    APP_LOG_INFO("LED task created: free_heap=%u",
                 (unsigned int)xPortGetFreeHeapSize());
    return true;
}
