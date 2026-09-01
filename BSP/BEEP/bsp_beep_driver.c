/**
 * @file bsp_beep_driver.c
 * @brief STM32F042 PA1有源蜂鸣器底层驱动。
 */
#include "bsp_beep_driver.h"

#include "main.h"

/** @brief 记录Driver是否完成初始化，阻止未初始化访问。 */
static bool s_beep_initialized;

beep_driver_status_t bsp_beep_driver_init(void)
{
    /* PA1为低电平有效；初始化时必须先输出高电平，防止持续鸣叫。 */
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
    s_beep_initialized = true;
    return BEEP_DRIVER_OK;
}

beep_driver_status_t bsp_beep_driver_set(bool active)
{
    if (!s_beep_initialized)
    {
        return BEEP_DRIVER_ERROR_RESOURCE;
    }

    HAL_GPIO_WritePin(BEEP_GPIO_Port,
                      BEEP_Pin,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return BEEP_DRIVER_OK;
}
