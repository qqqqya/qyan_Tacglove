/**
 * @file bsp_led_handler.c
 * @brief 相机指示灯、鱼眼指示灯和系统状态灯的逻辑实现。
 */
#include "bsp_led_handler.h"

/**
 * @brief 逻辑相机号到串行像素号的映射。
 * @details
 * SK6805 数据首先到达 LED7，因此物理发送顺序为 LED7、LED6、...、LED1。
 * 业务定义使用 LED1~LED5 表示相机 1~5，所以对应像素序号为 6~2。
 */
static const uint8_t s_camera_to_pixel[BSP_LED_CAMERA_COUNT] = {6U, 5U, 4U, 3U, 2U};
static const uint8_t s_fisheye_pixel = 1U; /**< LED6，鱼眼相机指示灯。 */
static const uint8_t s_system_pixel = 0U;  /**< LED7，系统状态指示灯。 */

/**
 * @brief 将LED Driver层状态转换为Handler层状态。
 * @param driver_status LED Driver层返回状态。
 * @return 对应的LED Handler层状态。
 */
static led_handler_status_t led_handler_convert_driver_status(
    led_driver_status_t driver_status)
{
    led_handler_status_t handler_status = HANDLER_ERROR;

    switch (driver_status)
    {
        case LED_OK:
            handler_status = HANDLER_OK;
            break;

        case LED_ERRORTIMEOUT:
            handler_status = HANDLER_ERRORTIMEOUT;
            break;

        case LED_ERRORRESOURCE:
            handler_status = HANDLER_ERRORRESOURCE;
            break;

        case LED_ERRORPARAMETER:
            handler_status = HANDLER_ERRORPARAMETER;
            break;

        case LED_ERRORNOMEMORY:
            handler_status = HANDLER_ERRORNOMEMORY;
            break;

        case LED_ERRORISR:
            handler_status = HANDLER_ERRORISR;
            break;

        case LED_RESERVED:
            handler_status = HANDLER_RESERVED;
            break;

        case LED_ERROR:
        default:
            handler_status = HANDLER_ERROR;
            break;
    }

    return handler_status;
}

led_handler_status_t bsp_led_handler_init(void)
{
    bsp_led_driver_init();

    led_handler_status_t status = bsp_led_handler_commit();
    if (HANDLER_OK != status)
    {
        return status;
    }

    return HANDLER_OK;
}

led_handler_status_t bsp_led_handler_set_camera(bsp_led_camera_id_t camera,
                                                 bsp_led_color_t color)
{
    if ((uint8_t)camera >= BSP_LED_CAMERA_COUNT)
    {
        return HANDLER_ERRORPARAMETER;
    }

    led_driver_status_t driver_status =
        bsp_led_driver_set_pixel(s_camera_to_pixel[(uint8_t)camera], color);
    return led_handler_convert_driver_status(driver_status);
}

led_handler_status_t bsp_led_handler_set_all_cameras(bsp_led_color_t color)
{
    for (uint8_t camera = 0U; camera < BSP_LED_CAMERA_COUNT; ++camera)
    {
        led_handler_status_t status =
            bsp_led_handler_set_camera((bsp_led_camera_id_t)camera, color);
        if (HANDLER_OK != status)
        {
            return status;
        }
    }

    return HANDLER_OK;
}

led_handler_status_t bsp_led_handler_set_fisheye(bsp_led_color_t color)
{
    led_driver_status_t driver_status =
        bsp_led_driver_set_pixel(s_fisheye_pixel, color);
    return led_handler_convert_driver_status(driver_status);
}

led_handler_status_t bsp_led_handler_set_system(bsp_led_color_t color)
{
    led_driver_status_t driver_status =
        bsp_led_driver_set_pixel(s_system_pixel, color);
    return led_handler_convert_driver_status(driver_status);
}

led_handler_status_t bsp_led_handler_commit(void)
{
    led_driver_status_t driver_status = bsp_led_driver_show();
    return led_handler_convert_driver_status(driver_status);
}

led_handler_status_t bsp_led_handler_show_startup_state(void)
{
    /* 12/255 的绿色用于首次点亮，降低调试阶段的亮度和瞬态电流。 */
    const bsp_led_color_t camera_ready = {.red = 0U, .green = 12U, .blue = 0U};
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};

    bsp_led_driver_clear();

    led_handler_status_t status = bsp_led_handler_set_all_cameras(camera_ready);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_set_fisheye(off);
    if (HANDLER_OK != status)
    {
        return status;
    }

    status = bsp_led_handler_set_system(off);
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
