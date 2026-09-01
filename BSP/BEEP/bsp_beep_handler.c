/**
 * @file bsp_beep_handler.c
 * @brief 蜂鸣器板级动作实现。
 */
#include "bsp_beep_handler.h"

/** @brief 将蜂鸣器Driver层状态转换为Handler层状态。 */
static beep_handler_status_t beep_handler_convert_driver_status(
    beep_driver_status_t driver_status)
{
    beep_handler_status_t handler_status = HANDLER_BEEP_ERROR;

    switch (driver_status)
    {
        case BEEP_DRIVER_OK:
            handler_status = HANDLER_BEEP_OK;
            break;

        case BEEP_DRIVER_ERROR_TIMEOUT:
            handler_status = HANDLER_BEEP_TIMEOUT;
            break;

        case BEEP_DRIVER_ERROR_RESOURCE:
            handler_status = HANDLER_BEEP_RESOURCE;
            break;

        case BEEP_DRIVER_ERROR_PARAMETER:
            handler_status = HANDLER_BEEP_PARAMETER;
            break;

        // case BEEP_DRIVER_ERROR_NO_MEMORY:
        //     handler_status = HANDLER_BEEP_ERROR_NO_MEMORY;
        //     break;

        // case BEEP_DRIVER_ERROR_ISR:
        //     handler_status = HANDLER_BEEP_ERROR_ISR;
        //     break;

        case BEEP_DRIVER_RESERVED:
            handler_status = HANDLER_BEEP_RESERVED;
            break;

        case BEEP_DRIVER_ERROR:
        default:
            handler_status = HANDLER_BEEP_ERROR;
            break;
    }

    return handler_status;
}

beep_handler_status_t bsp_beep_handler_init(void)
{
    beep_driver_status_t driver_status = bsp_beep_driver_init();
    return beep_handler_convert_driver_status(driver_status);
}

beep_handler_status_t bsp_beep_handler_start(void)
{
    beep_driver_status_t driver_status = bsp_beep_driver_set(true);
    return beep_handler_convert_driver_status(driver_status);
}

beep_handler_status_t bsp_beep_handler_stop(void)
{
    beep_driver_status_t driver_status = bsp_beep_driver_set(false);
    return beep_handler_convert_driver_status(driver_status);
}
