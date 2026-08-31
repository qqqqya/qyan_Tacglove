/**
 * @file bsp_led_driver.h
 * @brief SK6805-EC15 灯链底层驱动接口。
 * @details
 * 本层只负责物理像素缓存、GRB 数据编码和 PA4 波形发送，不包含
 * “相机灯”“系统灯”等业务含义，也不依赖 FreeRTOS。
 */
#ifndef BSP_LED_DRIVER_H
#define BSP_LED_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 原理图中串联的 SK6805 灯珠总数。 */
#define BSP_LED_PIXEL_COUNT 7U

/** @brief RGB 颜色，三个分量的有效范围均为 0~255。 */
typedef struct
{
    uint8_t red;   /**< 红色亮度。 */
    uint8_t green; /**< 绿色亮度。 */
    uint8_t blue;  /**< 蓝色亮度。 */
} bsp_led_color_t;

/** @brief LED Driver 层函数返回状态。 */
typedef enum
{
    BSP_LED_STATUS_OK = 0,             /**< 操作成功。 */
    BSP_LED_STATUS_INVALID_ARGUMENT,   /**< 像素序号等输入参数非法。 */
    BSP_LED_STATUS_UNSUPPORTED_CLOCK   /**< 系统时钟不是驱动标定的 48 MHz。 */
} bsp_led_status_t;

/**
 * @brief 初始化 SK6805 底层驱动和软件帧缓存。
 * @details 将 PA4 数据输出拉低，并将 7 颗灯的软件缓存全部清零。
 *          本函数只更新缓存，不会主动发送一帧数据。
 */
void bsp_led_driver_init(void);

/**
 * @brief 修改一个物理灯珠的缓存颜色。
 * @param pixel_index 数据流中的物理序号，0 表示最先接收数据的灯珠。
 * @param color 要写入的 RGB 颜色。
 * @retval BSP_LED_STATUS_OK 写入成功。
 * @retval BSP_LED_STATUS_INVALID_ARGUMENT pixel_index 超出 0~6。
 * @note 调用后必须再调用 bsp_led_driver_show()，灯珠才会更新。
 */
bsp_led_status_t bsp_led_driver_set_pixel(uint8_t pixel_index, bsp_led_color_t color);

/**
 * @brief 将全部物理灯珠的缓存颜色清零。
 * @note 本函数不发送数据，需随后调用 bsp_led_driver_show()。
 */
void bsp_led_driver_clear(void);

/**
 * @brief 把完整的 7 像素缓存发送到 SK6805 灯链。
 * @retval BSP_LED_STATUS_OK 发送完成。
 * @retval BSP_LED_STATUS_UNSUPPORTED_CLOCK 当前系统时钟不是 48 MHz。
 * @warning 发送期间会短暂关闭中断，以保证单总线脉宽不被抢占破坏。
 */
bsp_led_status_t bsp_led_driver_show(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_DRIVER_H */
