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
 * @retval HANDLER_OK 每一步设置和发送都成功。
 * @return 其他LED Handler状态表示对应步骤失败。
 */
static led_handler_status_t led_show_all(bsp_led_color_t color)
{
    bsp_led_driver_clear();

    led_handler_status_t status = bsp_led_handler_set_all_cameras(color);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_set_fisheye(color);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_set_system(color);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_commit();
    if (HANDLER_OK != status)
    {
        return status;
    }

    return HANDLER_OK;
}

/**
 * @brief 运行一轮完整的七灯板级自检。
 * @retval HANDLER_OK 本轮所有画面均发送成功。
 * @return 其他LED Handler状态表示对应步骤失败。
 */
static led_handler_status_t led_run_demo_cycle(void)
{
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};
    const bsp_led_color_t red = {.red = 24U, .green = 0U, .blue = 0U};
    const bsp_led_color_t green = {.red = 0U, .green = 24U, .blue = 0U};
    const bsp_led_color_t blue = {.red = 0U, .green = 0U, .blue = 24U};
    const bsp_led_color_t white = {.red = 16U, .green = 16U, .blue = 16U};
    const bsp_led_color_t magenta = {.red = 24U, .green = 0U, .blue = 24U};
    const bsp_led_color_t cyan = {.red = 0U, .green = 24U, .blue = 24U};

    /** @brief 红绿蓝 跑马 雾灯变绿  led7  闪两下  然后led6 blue */
    led_handler_status_t status = led_show_all(red);
    if (HANDLER_OK != status)
    {
        return status;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);
#if 0    

    status = led_show_all(green);//所有灯变绿
    if (HANDLER_OK != status)
    {
        return status;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    status = led_show_all(blue);
    if (HANDLER_OK != status)
    {
        return status;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    status = led_show_all(off);
    if (HANDLER_OK != status)
    {
        return status;
    }
    led_delay(LED_DEMO_CHASE_HOLD_MS);

    for (uint8_t camera = 0U; camera < BSP_LED_CAMERA_COUNT; ++camera)
    {
        bsp_led_driver_clear();

        status = bsp_led_handler_set_camera((bsp_led_camera_id_t)camera, white);
        if (HANDLER_OK != status)
        {
            return status;
        }

        status = bsp_led_handler_commit();
        if (HANDLER_OK != status)
        {
            return status;
        }

        led_delay(LED_DEMO_CHASE_HOLD_MS);
    }

    bsp_led_driver_clear();
    status = bsp_led_handler_set_fisheye(magenta);
    if (HANDLER_OK != status)
    {
        return status;
    }
    status = bsp_led_handler_commit();
    if (HANDLER_OK != status)
    {
        return status;
    }
    led_delay(LED_DEMO_COLOR_HOLD_MS);

    for (uint8_t blink = 0U; blink < 3U; ++blink)
    {
        bsp_led_driver_clear();
        status = bsp_led_handler_set_system(cyan);
        if (HANDLER_OK != status)
        {
            return status;
        }
        status = bsp_led_handler_commit();
        if (HANDLER_OK != status)
        {
            return status;
        }
        led_delay(LED_DEMO_BLINK_HOLD_MS);

        status = led_show_all(off);
        if (HANDLER_OK != status)
        {
            return status;
        }
        led_delay(LED_DEMO_BLINK_HOLD_MS);
    }

    bsp_led_driver_clear();
    status = bsp_led_handler_set_all_cameras(green);
    if (HANDLER_OK != status)
    {
        return status;
    }
    status = bsp_led_handler_set_fisheye(blue);
    if (HANDLER_OK != status)
    {
        return status;
    }
    status = bsp_led_handler_set_system(white);
    if (HANDLER_OK != status)
    {
        return status;
    }

#endif
    status = led_show_all(off);
    if (HANDLER_OK != status)
    {
        return status;
    }
    led_delay(LED_DEMO_CHASE_HOLD_MS);

    status = bsp_led_handler_set_system(white);
    if (HANDLER_OK != status)
    {
        return status;
    }
    status = bsp_led_handler_commit();
    if (HANDLER_OK != status)
    {
        return status;
    }

    led_delay(LED_DEMO_FINAL_HOLD_MS);
    return HANDLER_OK;
}

/**
 * @brief LED任务入口。
 * @param argument FreeRTOS任务参数，当前未使用。
 */
static void led_task_entry(void *argument)
{
    (void)argument;
    led_handler_status_t status = bsp_led_handler_init();

    status = led_run_demo_cycle();

    // if (HANDLER_OK != status)
    // {
    //     /* 日志预留：记录LED Handler初始化失败及status。 
    //     return status;
    //     */
    // }
    // else
    // {
    //     for (;;)
    //     {
    //         status = led_run_demo_cycle();
    //         if (HANDLER_OK != status)
    //         {
    //             /* 日志预留：记录LED自检步骤失败及status。 */
    //             break;
    //         }
    //     }
    // }

    /* LED故障不再关闭全局中断，避免同时冻结蜂鸣器及其他任务。 */
    for (;;)
    {
        led_delay(LED_ERROR_RETRY_DELAY_MS);
    }
}

task_status_t led_task_create(void)
{
    const BaseType_t result = xTaskCreate(led_task_entry,
                                          "led",
                                          LED_TASK_STACK_WORDS,
                                          NULL,
                                          LED_TASK_PRIORITY,
                                          NULL);
    if (result != pdPASS)
    {
        /* 日志预留：记录LED任务创建失败和FreeRTOS heap余量。 */
        return TASK_ERROR_NO_MEMORY;
    }

    return TASK_OK;
}
