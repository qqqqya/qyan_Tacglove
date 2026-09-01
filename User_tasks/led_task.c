/**
 * @file led_task.c
 * @brief 七颗SK6805指示灯及数据采集状态机任务。
 * @details
 * 保留原板级灯效函数用于后续调试；当前任务入口运行产品状态流程：
 * 上电自检、待机、采集准备、采集中以及故障指示。
 */
#include "led_task.h"

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_led_handler.h"
#include "bsp_beep_handler.h"
#include "system_event.h"

#define LED_TASK_STACK_WORDS      128U                    /**< 128 words，即512 bytes。 */
#define LED_TASK_PRIORITY         (tskIDLE_PRIORITY + 1U) /**< 板级显示任务优先级。 */
#define LED_DEMO_COLOR_HOLD_MS    500U                    /**< 全色画面保持时间。 */
#define LED_DEMO_CHASE_HOLD_MS    250U                    /**< 跑马灯单步保持时间。 */
#define LED_DEMO_BLINK_HOLD_MS    200U                    /**< 系统灯闪烁半周期。 */
#define LED_DEMO_FINAL_HOLD_MS    1200U                   /**< 组合画面保持时间。 */
#define LED_ERROR_RETRY_DELAY_MS  1000U                   /**< 故障后任务检查周期。 */
#define LED_SELF_TEST_BLINK_COUNT 3U                      /**< 上电绿色自检闪烁次数。 */
#define LED_SELF_TEST_HALF_MS     250U                    /**< 上电自检闪烁半周期。 */
#define LED_PREPARE_BLINK_COUNT   3U                      /**< 采集准备蓝灯闪烁次数。 */
#define LED_PREPARE_HALF_MS       250U                    /**< 采集准备闪烁半周期。 */
#define LED_STATUS_BRIGHTNESS     20U                     /**< 状态灯亮度，降低整机电流。 */
#define BEEP_CONFIRM_TIME_MS      120U                    /**< 开始和结束时的单次短鸣长度。 */

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
 * @brief 同时显示六路设备状态和LED7系统状态。
 * @param device_color LED1~LED6共同显示的颜色。
 * @param system_color LED7显示的颜色。
 * @retval HANDLER_OK 颜色设置及物理刷新成功。
 * @return 其他LED Handler状态表示设置或发送失败。
 */
static led_handler_status_t led_show_system_state(bsp_led_color_t device_color,
                                                   bsp_led_color_t system_color)
{
    bsp_led_handler_clear();

    led_handler_status_t status =
        bsp_led_handler_set_all_cameras(device_color);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_set_fisheye(device_color);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_set_system(system_color);
    if (HANDLER_OK != status)
    {
        return status;
    }

    return bsp_led_handler_commit();
}

/**
 * @brief 执行当前阶段的设备上电自检占位流程。
 * @retval true 自检通过，可以进入待机。
 * @retval false 任一设备自检失败，应进入红色故障状态。
 * @note 相机和采集链路尚未接入，因此当前仅验证LED和蜂鸣器初始化结果。
 */
static bool led_devices_self_test(void)
{
    /*
     * TODO(camera): 逐路检查五个USB相机是否枚举、通信及帧数据正常。
     * TODO(fisheye): 检查鱼眼相机是否枚举、通信及帧数据正常。
     * TODO(storage): 检查采集数据存储介质容量和写入功能。
     * TODO(link): 检查数据通道连接和初始丢包计数。
     */
    return true;
}

/**
 * @brief 运行上电绿色闪烁自检灯效并完成设备自检占位检查。
 * @retval HANDLER_OK 灯效发送成功且设备自检通过。
 * @retval HANDLER_ERROR 设备自检失败。
 * @return 其他LED Handler状态表示灯效发送失败。
 */
static led_handler_status_t led_run_power_on_self_test(void)
{
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};
    const bsp_led_color_t green = {
        .red = 0U, .green = LED_STATUS_BRIGHTNESS, .blue = 0U};

    for (uint8_t count = 0U; count < LED_SELF_TEST_BLINK_COUNT; ++count)
    {
        led_handler_status_t status = led_show_system_state(green, off);
        if (HANDLER_OK != status)
        {
            return status;
        }
        led_delay(LED_SELF_TEST_HALF_MS);

        status = led_show_system_state(off, off);
        if (HANDLER_OK != status)
        {
            return status;
        }
        led_delay(LED_SELF_TEST_HALF_MS);
    }

    if (!led_devices_self_test())
    {
        return HANDLER_ERROR;
    }

    return HANDLER_OK;
}

/**
 * @brief 输出一次用于确认开始或结束录制的短鸣。
 * @retval true 蜂鸣器成功开启并在规定时间后关闭。
 * @retval false 蜂鸣器Handler操作失败。
 */
static bool led_beep_once(void)
{
    if (HANDLER_BEEP_OK != bsp_beep_handler_start())
    {
        return false;
    }

    led_delay(BEEP_CONFIRM_TIME_MS);

    if (HANDLER_BEEP_OK != bsp_beep_handler_stop())
    {
        return false;
    }

    return true;
}

/**
 * @brief 启动尚未接入的数据采集设备。
 * @retval true 所有采集设备均成功进入采集状态。
 * @retval false 任一设备启动失败。
 */
static bool led_data_capture_start(void)
{
    /*
     * TODO(capture-start): 在相机驱动接入后，通过独立采集任务的命令队列
     * 发送“开始采集”，并等待所有任务返回启动成功事件。
     */
    return true;
}

/**
 * @brief 停止尚未接入的数据采集设备并完成缓存收尾。
 * @retval true 所有采集设备均已安全停止。
 * @retval false 任一设备停止或数据落盘失败。
 */
static bool led_data_capture_stop(void)
{
    /*
     * TODO(capture-stop): 通知各采集任务停止接收新数据，等待缓存写入完成，
     * 再检查通信中断和丢包统计；异常时进入红色故障状态。
     */
    return true;
}

/**
 * @brief 显示采集准备状态：LED1~LED6绿色常亮，LED7蓝色闪烁。
 * @retval HANDLER_OK 完整准备灯效发送成功。
 * @return 其他LED Handler状态表示设置或发送失败。
 */
static led_handler_status_t led_run_capture_prepare(void)
{
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};
    const bsp_led_color_t green = {
        .red = 0U, .green = LED_STATUS_BRIGHTNESS, .blue = 0U};
    const bsp_led_color_t blue = {
        .red = 0U, .green = 0U, .blue = LED_STATUS_BRIGHTNESS};

    for (uint8_t count = 0U; count < LED_PREPARE_BLINK_COUNT; ++count)
    {
        led_handler_status_t status = led_show_system_state(green, blue);
        if (HANDLER_OK != status)
        {
            return status;
        }
        led_delay(LED_PREPARE_HALF_MS);

        status = led_show_system_state(green, off);
        if (HANDLER_OK != status)
        {
            return status;
        }
        led_delay(LED_PREPARE_HALF_MS);
    }

    return HANDLER_OK;
}

/**
 * @brief 尝试显示不可恢复故障状态。
 * @details 七颗SK6805全部红色常亮，便于在相机故障、通信中断或数据丢包
 * 等故障接入后复用。如果LED发送本身失败，本函数不再递归处理。
 */
static void led_show_fault_state(void)
{
    const bsp_led_color_t red = {
        .red = LED_STATUS_BRIGHTNESS, .green = 0U, .blue = 0U};

    (void)led_show_system_state(red, red);
}

/**
 * @brief LED状态和数据采集流程任务入口。
 * @param argument FreeRTOS任务参数，当前未使用。
 */
static void led_task_entry(void *argument)
{
    (void)argument;
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};
    const bsp_led_color_t green = {
        .red = 0U, .green = LED_STATUS_BRIGHTNESS, .blue = 0U};
    const bsp_led_color_t blue = {
        .red = 0U, .green = 0U, .blue = LED_STATUS_BRIGHTNESS};
    bool capture_active = false;

    /** @brief 初始化LED Handler和蜂鸣器Handler */
    led_handler_status_t led_status = bsp_led_handler_init();
    const beep_handler_status_t beep_status = bsp_beep_handler_init();


    /** @brief 初始化失败，进入故障状态 */
    if ((HANDLER_OK != led_status) || (HANDLER_BEEP_OK != beep_status))
    {
        led_show_fault_state();
        for (;;)/* 无限循环，等待系统重启 */
        {
            led_delay(LED_ERROR_RETRY_DELAY_MS);
        }
    }

    led_status = led_run_power_on_self_test();
    if (HANDLER_OK != led_status)
    {
        led_show_fault_state();
        for (;;)
        {
            led_delay(LED_ERROR_RETRY_DELAY_MS);
        }
    }

    /* 自检期间按下的按键无效，进入待机后必须重新按下一次。 */
    system_event_clear();
    led_status = led_show_system_state(green, off);

    for (;;)
    {
        system_event_t event = {.type = SYSTEM_EVENT_NONE};

        if (HANDLER_OK != led_status)
        {
            led_show_fault_state();
            led_delay(LED_ERROR_RETRY_DELAY_MS);
            continue;
        }

        const task_status_t event_status =
            system_event_wait(&event, portMAX_DELAY);
        if ((TASK_OK != event_status) ||
            (SYSTEM_EVENT_CAPTURE_KEY_PRESSED != event.type))
        {
            /* 当前没有其他事件；队列资源错误时按系统故障处理。 */
            if (TASK_ERROR_TIMEOUT != event_status)
            {
                led_status = HANDLER_ERROR;
            }
            continue;
        }

        if (!capture_active)
        {
            /* 蓝灯闪烁表示准备中；蜂鸣结束后才允许数据采集开始。 */
            led_status = led_run_capture_prepare();
            if ((HANDLER_OK != led_status) || !led_beep_once() ||
                !led_data_capture_start())
            {
                led_status = HANDLER_ERROR;
                continue;
            }

            led_status = led_show_system_state(green, blue);
            capture_active = (HANDLER_OK == led_status);
        }
        else
        {
            /* 先安全结束和落盘，再鸣叫确认，最后回到绿色待机状态。 */
            if (!led_data_capture_stop() || !led_beep_once())
            {
                led_status = HANDLER_ERROR;
                continue;
            }

            capture_active = false;
            led_status = led_show_system_state(green, off);
        }
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
