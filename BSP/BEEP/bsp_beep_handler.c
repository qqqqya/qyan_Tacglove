/**
 * @file bsp_beep_handler.c
 * @brief 蜂鸣器板级动作实现。
 */
#include "bsp_beep_handler.h"

bsp_beep_status_t bsp_beep_handler_init(void)
{
    return bsp_beep_driver_init();
}

bsp_beep_status_t bsp_beep_handler_start(void)
{
    return bsp_beep_driver_set(true);
}

bsp_beep_status_t bsp_beep_handler_stop(void)
{
    return bsp_beep_driver_set(false);
}
