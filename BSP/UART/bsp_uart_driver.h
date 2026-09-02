/**
 * @file bsp_uart_driver.h
 * @brief USART2 DMA循环接收和DMA发送的底层驱动接口。
 * @details 本层只处理USART2、DMA缓冲区和HAL状态，不包含测试协议、ROS或
 * FreeRTOS任务逻辑。PA2为TX，PA3为RX，通信参数由CubeMX配置为115200 8N1。
 */
#ifndef BSP_UART_DRIVER_H
#define BSP_UART_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief UART Driver函数返回状态。 */
typedef enum
{
    UART_DRIVER_OK = 0,             /**< 操作成功。 */
    UART_DRIVER_ERROR = 1,          /**< HAL返回一般错误。 */
    UART_DRIVER_ERROR_BUSY = 2,     /**< 上一次DMA发送尚未完成。 */
    UART_DRIVER_ERROR_RESOURCE = 3, /**< UART或DMA资源未正确配置。 */
    UART_DRIVER_ERROR_PARAMETER = 4 /**< 空指针或长度非法。 */
} uart_driver_status_t;

/**
 * @brief 启动USART2 RX DMA循环接收。
 * @retval UART_DRIVER_OK DMA循环接收已启动。
 * @retval UART_DRIVER_ERROR_RESOURCE USART2 RX DMA不是Circular模式。
 * @retval UART_DRIVER_ERROR HAL拒绝启动DMA。
 */
uart_driver_status_t bsp_uart_driver_init(void);

/**
 * @brief 从DMA循环缓冲区取出当前已经接收的数据。
 * @param[out] destination 保存数据的目标缓冲区。
 * @param capacity 目标缓冲区最多可保存的字节数。
 * @return 本次实际取出的字节数；参数非法或尚未初始化时返回0。
 * @note 本函数不会阻塞。若暂时没有新字节，立即返回0。
 */
size_t bsp_uart_driver_read(uint8_t *destination, size_t capacity);

/**
 * @brief 启动一次USART2 TX DMA发送。
 * @param data 待发送数据；DMA完成前该内存必须保持有效且内容不变。
 * @param length 发送长度，范围1～65535字节。
 * @retval UART_DRIVER_OK DMA发送已启动。
 * @retval UART_DRIVER_ERROR_BUSY 上一次发送尚未完成。
 * @retval UART_DRIVER_ERROR_RESOURCE Driver尚未初始化。
 * @retval UART_DRIVER_ERROR_PARAMETER 参数非法。
 * @retval UART_DRIVER_ERROR HAL拒绝启动DMA。
 */
uart_driver_status_t bsp_uart_driver_transmit_dma(const uint8_t *data,
                                                   size_t length);

/**
 * @brief 查询USART2是否仍在执行DMA发送。
 * @return true表示发送未完成，false表示可以启动下一次发送。
 * @details 优先使用HAL中断完成状态；若硬件TC已经置位而HAL仍为BUSY_TX，
 * 函数会只终止TX并恢复HAL状态，RX循环DMA不受影响。
 */
bool bsp_uart_driver_tx_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UART_DRIVER_H */
