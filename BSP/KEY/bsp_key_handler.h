/**
 * @file bsp_key_handler.h
 * @brief 数据采集按键的板级读取接口。
 * @details PB6配置为输入上拉，按键按下时PB6被接到GND，因此按下为低电平。
 * 本模块直接封装现有GPIO，不再增加只有一层转发作用的Driver文件。
 */
#ifndef BSP_KEY_HANDLER_H
#define BSP_KEY_HANDLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 按键Handler函数返回状态。 */
typedef enum
{
    KEY_HANDLER_OK = 0,             /**< 操作成功。 */
    KEY_HANDLER_ERROR = 1,          /**< 一般运行错误。 */
    KEY_HANDLER_ERROR_RESOURCE = 2, /**< GPIO等底层资源尚未就绪。 */
    KEY_HANDLER_ERROR_PARAMETER = 3 /**< 传入了空指针等非法参数。 */
} key_handler_status_t;

/**
 * @brief 初始化按键Handler。
 * @details GPIO方向和内部上拉已经由MX_GPIO_Init()配置，本函数只建立
 * Handler内部状态，不重复修改CubeMX生成的GPIO配置。
 * @retval KEY_HANDLER_OK 初始化成功。
 */
key_handler_status_t bsp_key_handler_init(void);

/**
 * @brief 读取数据采集按键的当前电平状态。
 * @param[out] pressed 返回true表示PB6为低电平、按键当前被按下。
 * @retval KEY_HANDLER_OK 读取成功。
 * @retval KEY_HANDLER_ERROR_RESOURCE 尚未初始化Handler。
 * @retval KEY_HANDLER_ERROR_PARAMETER pressed为空指针。
 * @note 本函数只读取原始状态，机械按键消抖由按键任务完成。
 */
key_handler_status_t bsp_key_handler_is_pressed(bool *pressed);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_HANDLER_H */
