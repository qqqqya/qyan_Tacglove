/**
 * @file bsp_beep_driver.h
 * @brief 有源蜂鸣器GPIO底层驱动接口。
 * @details 本层只处理PA1、低电平有效等硬件细节，不包含鸣叫节奏和RTOS延时。
 */
#ifndef BSP_BEEP_DRIVER_H
#define BSP_BEEP_DRIVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 蜂鸣器Driver层返回状态。 */
typedef enum
{
    BEEP_DRIVER_OK              = 0,    /**< Operation completed successfully. */
    BEEP_DRIVER_ERROR           = 1,    /**< General runtime error. */
    BEEP_DRIVER_ERROR_TIMEOUT   = 2,    /**< Operation timed out. */
    BEEP_DRIVER_ERROR_RESOURCE  = 3,    /**< Required resource is unavailable. */
    BEEP_DRIVER_ERROR_PARAMETER = 4,    /**< Invalid parameter. */
    BEEP_DRIVER_ERROR_NO_MEMORY = 5,    /**< Memory allocation failed. */
    BEEP_DRIVER_ERROR_ISR       = 6,    /**< Operation is not allowed in ISR context. */
    BEEP_DRIVER_RESERVED        = 0xFF  /**< Reserved status. */
} beep_driver_status_t;

/**
 * @brief 初始化蜂鸣器底层并确保蜂鸣器关闭。
 * @retval BEEP_DRIVER_OK 初始化成功。
 */
beep_driver_status_t bsp_beep_driver_init(void);

/**
 * @brief 设置蜂鸣器开关状态。
 * @param active true表示鸣叫，false表示停止。
 * @retval BEEP_DRIVER_OK 设置成功。
 * @retval BEEP_DRIVER_ERROR_RESOURCE 尚未调用初始化函数。
 * @note 原理图使用S8550驱动有源蜂鸣器，因此MCU输出低电平时鸣叫。
 */
beep_driver_status_t bsp_beep_driver_set(bool active);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BEEP_DRIVER_H */
