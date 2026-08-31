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

bool bsp_led_handler_init(void)
{
    bsp_led_driver_init();
    return bsp_led_handler_commit();
}

bool bsp_led_handler_set_camera(bsp_led_camera_id_t camera, bsp_led_color_t color)
{
    if ((uint8_t)camera >= BSP_LED_CAMERA_COUNT)
    {
        return false;
    }

    return bsp_led_driver_set_pixel(s_camera_to_pixel[(uint8_t)camera], color) ==
           BSP_LED_STATUS_OK;
}

bool bsp_led_handler_set_all_cameras(bsp_led_color_t color)
{
    for (uint8_t camera = 0U; camera < BSP_LED_CAMERA_COUNT; ++camera)
    {
        if (!bsp_led_handler_set_camera((bsp_led_camera_id_t)camera, color))
        {
            return false;
        }
    }

    return true;
}

bool bsp_led_handler_set_fisheye(bsp_led_color_t color)
{
    return bsp_led_driver_set_pixel(s_fisheye_pixel, color) == BSP_LED_STATUS_OK;
}

bool bsp_led_handler_set_system(bsp_led_color_t color)
{
    return bsp_led_driver_set_pixel(s_system_pixel, color) == BSP_LED_STATUS_OK;
}

bool bsp_led_handler_commit(void)
{
    return bsp_led_driver_show() == BSP_LED_STATUS_OK;
}

bool bsp_led_handler_show_startup_state(void)
{
    /* 12/255 的绿色用于首次点亮，降低调试阶段的亮度和瞬态电流。 */
    const bsp_led_color_t camera_ready = {.red = 0U, .green = 12U, .blue = 0U};
    const bsp_led_color_t off = {.red = 0U, .green = 0U, .blue = 0U};

    bsp_led_driver_clear();
    return bsp_led_handler_set_all_cameras(camera_ready) &&
           bsp_led_handler_set_fisheye(off) &&
           bsp_led_handler_set_system(off) &&
           bsp_led_handler_commit();
}
