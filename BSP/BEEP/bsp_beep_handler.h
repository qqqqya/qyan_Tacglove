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
    BEEP_HANDLER_OK              = 0,    /**< Operation completed successfully. */
    BEEP_HANDLER_ERROR           = 1,    /**< General runtime error. */
    BEEP_HANDLER_ERROR_TIMEOUT   = 2,    /**< Operation timed out. */
    BEEP_HANDLER_ERROR_RESOURCE  = 3,    /**< Required resource is unavailable. */
    BEEP_HANDLER_ERROR_PARAMETER = 4,    /**< Invalid parameter. */
    BEEP_HANDLER_ERROR_NO_MEMORY = 5,    /**< Memory allocation failed. */
    BEEP_HANDLER_ERROR_ISR       = 6,    /**< Operation is not allowed in ISR context. */
    BEEP_HANDLER_RESERVED        = 0xFF  /**< Reserved status. */
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
