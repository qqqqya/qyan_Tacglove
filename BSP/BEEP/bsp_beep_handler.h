/**
 * @file bsp_beep_handler.h
 * @brief 蜂鸣器板级动作接口。
 * @details 上层任务通过本接口控制蜂鸣器，不直接访问HAL或GPIO有效电平。
 */
#ifndef BSP_BEEP_HANDLER_H
#define BSP_BEEP_HANDLER_H

#include "bsp_beep_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化蜂鸣器并保持静音。 */
bsp_beep_status_t bsp_beep_handler_init(void);

/** @brief 开始鸣叫。 */
bsp_beep_status_t bsp_beep_handler_start(void);

/** @brief 停止鸣叫。 */
bsp_beep_status_t bsp_beep_handler_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BEEP_HANDLER_H */
