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

/** @brief 蜂鸣器Handler层函数返回状态。 */
typedef enum
{
    HANDLER_BEEP_OK              = 0,    /**< Operation completed successfully. */
    HANDLER_BEEP_ERROR           = 1,    /**< General runtime error. */
    HANDLER_BEEP_TIMEOUT   = 2,    /**< Operation timed out. */
    HANDLER_BEEP_RESOURCE  = 3,    /**< Required resource is unavailable. */
    HANDLER_BEEP_PARAMETER = 4,    /**< Invalid parameter. */
    HANDLER_BEEP_RESERVED        = 0xFF  /**< Reserved status. */
} beep_handler_status_t;

/** @brief 初始化蜂鸣器并保持静音。 */
beep_handler_status_t bsp_beep_handler_init(void);

/** @brief 开始鸣叫。 */
beep_handler_status_t bsp_beep_handler_start(void);

/** @brief 停止鸣叫。 */
beep_handler_status_t bsp_beep_handler_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BEEP_HANDLER_H */
