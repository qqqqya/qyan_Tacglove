/**
 * @file bsp_key_handler.c
 * @brief PB6低电平有效数据采集按键的板级实现。
 */
#include "bsp_key_handler.h"

#include "main.h"

/** @brief 标记按键Handler是否已经完成初始化。 */
static bool s_key_initialized;

key_handler_status_t bsp_key_handler_init(void)
{
    s_key_initialized = true;
    return KEY_HANDLER_OK;
}

key_handler_status_t bsp_key_handler_is_pressed(bool *pressed)
{
    if (NULL == pressed)
    {
        return KEY_HANDLER_ERROR_PARAMETER;
    }

    if (!s_key_initialized)
    {
        return KEY_HANDLER_ERROR_RESOURCE;
    }

    *pressed = (GPIO_PIN_RESET ==
                HAL_GPIO_ReadPin(KEY_Capdata_GPIO_Port, KEY_Capdata_Pin));
    return KEY_HANDLER_OK;
}
