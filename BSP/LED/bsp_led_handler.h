/**
 * @file bsp_led_handler.h
 * @brief 板级指示灯逻辑映射接口。
 * @details
 * 本层把物理灯珠转换为相机 1~5、鱼眼和系统状态灯，隔离上层任务与
 * 串联方向、像素序号等硬件细节。
 */
#ifndef BSP_LED_HANDLER_H
#define BSP_LED_HANDLER_H

#include <stdint.h>

#include "bsp_led_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 普通 USB 相机及其指示灯数量。 */
#define BSP_LED_CAMERA_COUNT 5U

/** @brief 五路普通相机的逻辑编号。 */
typedef enum
{
    BSP_LED_CAMERA_1 = 0,
    BSP_LED_CAMERA_2,
    BSP_LED_CAMERA_3,
    BSP_LED_CAMERA_4,
    BSP_LED_CAMERA_5
} bsp_led_camera_id_t;

/** @brief LED Handler层函数返回状态。 */
typedef enum
{
    HANDLER_OK             = 0,    /**< Operation completed successfully. */
    HANDLER_ERROR          = 1,    /**< General runtime error. */
    HANDLER_ERRORTIMEOUT   = 2,    /**< Operation timed out. */
    HANDLER_ERRORRESOURCE  = 3,    /**< Required resource is unavailable. */
    HANDLER_ERRORPARAMETER = 4,    /**< Invalid parameter. */
    HANDLER_ERRORNOMEMORY  = 5,    /**< Memory allocation failed. */
    HANDLER_ERRORISR       = 6,    /**< Operation is not allowed in ISR context. */
    HANDLER_RESERVED       = 0xFF  /**< Reserved status. */
} led_handler_status_t;

/**
 * @brief 初始化 LED driver，并向灯链发送一帧全灭数据。
 * @retval HANDLER_OK 初始化及首帧发送成功。
 * @retval HANDLER_ERRORRESOURCE 系统时钟或底层资源不满足要求。
 * @retval HANDLER_ERROR 其他底层错误。
 */
led_handler_status_t bsp_led_handler_init(void);

/**
 * @brief 设置一路普通相机指示灯的缓存颜色。
 * @param camera 相机逻辑编号。
 * @param color 目标颜色。
 * @retval HANDLER_OK 设置成功。
 * @retval HANDLER_ERRORPARAMETER 相机编号非法。
 * @note 本函数不立即刷新灯珠，需调用 bsp_led_handler_commit()。
 */
led_handler_status_t bsp_led_handler_set_camera(bsp_led_camera_id_t camera,
                                                 bsp_led_color_t color);

/**
 * @brief 将五路普通相机指示灯设置为相同的缓存颜色。
 * @param color 目标颜色。
 * @retval HANDLER_OK 五路缓存均设置成功。
 * @retval HANDLER_ERRORPARAMETER 参数错误。
 * @retval HANDLER_ERROR 其他底层错误。
 * @note 本函数不立即刷新灯珠。
 */
led_handler_status_t bsp_led_handler_set_all_cameras(bsp_led_color_t color);

/** @brief 设置鱼眼相机灯缓存；调用 commit 后生效。 */
led_handler_status_t bsp_led_handler_set_fisheye(bsp_led_color_t color);

/** @brief 设置系统状态灯缓存；调用 commit 后生效。 */
led_handler_status_t bsp_led_handler_set_system(bsp_led_color_t color);

/**
 * @brief 将 handler 层设置的全部颜色提交到物理灯链。
 * @retval HANDLER_OK 发送成功。
 * @retval HANDLER_ERRORRESOURCE 系统时钟或底层资源不满足要求。
 * @retval HANDLER_ERROR 其他底层错误。
 */
led_handler_status_t bsp_led_handler_commit(void);

/**
 * @brief 显示当前固件定义的启动完成状态。
 * @details LED1~LED5 低亮绿色常亮，LED6 鱼眼灯与 LED7 系统灯熄灭。
 * @retval HANDLER_OK 状态设置并发送成功。
 * @retval HANDLER_ERRORPARAMETER 参数设置失败。
 * @retval HANDLER_ERRORRESOURCE 底层资源不满足要求。
 * @retval HANDLER_ERROR 其他底层错误。
 */
led_handler_status_t bsp_led_handler_show_startup_state(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_HANDLER_H */
