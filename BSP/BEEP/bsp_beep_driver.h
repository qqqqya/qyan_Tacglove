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
    BSP_BEEP_STATUS_OK = 0,       /**< 操作成功。 */
    BSP_BEEP_STATUS_NOT_INITIALIZED /**< 尚未初始化Driver。 */
} bsp_beep_status_t;

/**
 * @brief 初始化蜂鸣器底层并确保蜂鸣器关闭。
 * @retval BSP_BEEP_STATUS_OK 初始化成功。
 */
bsp_beep_status_t bsp_beep_driver_init(void);

/**
 * @brief 设置蜂鸣器开关状态。
 * @param active true表示鸣叫，false表示停止。
 * @retval BSP_BEEP_STATUS_OK 设置成功。
 * @retval BSP_BEEP_STATUS_NOT_INITIALIZED 尚未调用初始化函数。
 * @note 原理图使用S8550驱动有源蜂鸣器，因此MCU输出低电平时鸣叫。
 */
bsp_beep_status_t bsp_beep_driver_set(bool active);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BEEP_DRIVER_H */
