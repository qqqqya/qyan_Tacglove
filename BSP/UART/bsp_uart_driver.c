/**
 * @file bsp_uart_driver.c
 * @brief STM32F042 USART2 DMA底层驱动实现。
 */
#include "bsp_uart_driver.h"

#include "usart.h"

/**
 * @brief RX DMA循环缓冲区大小。
 * @details 115200 baud下约可缓存22 ms连续数据，可完整容纳三条协议最大帧。
 * 参考工程使用2048字节，但F042仅有6 KB RAM，因此使用256字节。
 */
#define UART_RX_DMA_BUFFER_SIZE 256U

/** @brief DMA持续写入的循环缓冲区。 */
static volatile uint8_t s_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];

/** @brief 软件下一次读取DMA缓冲区的位置。 */
static size_t s_rx_read_index;

/** @brief Driver已经成功启动RX DMA。 */
static bool s_uart_initialized;

uart_driver_status_t bsp_uart_driver_init(void)
{
    if ((NULL == huart2.hdmarx) || (NULL == huart2.hdmatx) ||
        (DMA_CIRCULAR != huart2.hdmarx->Init.Mode))
    {
        return UART_DRIVER_ERROR_RESOURCE;
    }

    s_rx_read_index = 0U;
    s_uart_initialized = false;

    const HAL_StatusTypeDef hal_status = HAL_UART_Receive_DMA(
        &huart2,
        (uint8_t *)(uintptr_t)s_rx_dma_buffer,
        UART_RX_DMA_BUFFER_SIZE);
    if (HAL_OK != hal_status)
    {
        return UART_DRIVER_ERROR;
    }

    s_uart_initialized = true;
    return UART_DRIVER_OK;
}

size_t bsp_uart_driver_read(uint8_t *destination, size_t capacity)
{
    if ((!s_uart_initialized) || (NULL == destination) || (0U == capacity))
    {
        return 0U;
    }

    size_t dma_write_index = UART_RX_DMA_BUFFER_SIZE -
        (size_t)__HAL_DMA_GET_COUNTER(huart2.hdmarx);
    if (dma_write_index >= UART_RX_DMA_BUFFER_SIZE)
    {
        dma_write_index = 0U;
    }

    size_t read_count = 0U;
    while ((s_rx_read_index != dma_write_index) && (read_count < capacity))
    {
        destination[read_count] = s_rx_dma_buffer[s_rx_read_index];
        ++read_count;
        s_rx_read_index = (s_rx_read_index + 1U) % UART_RX_DMA_BUFFER_SIZE;
    }

    return read_count;
}

uart_driver_status_t bsp_uart_driver_transmit_dma(const uint8_t *data,
                                                   size_t length)
{
    if (!s_uart_initialized)
    {
        return UART_DRIVER_ERROR_RESOURCE;
    }

    if ((NULL == data) || (0U == length) || (length > UINT16_MAX))
    {
        return UART_DRIVER_ERROR_PARAMETER;
    }

    if (HAL_UART_STATE_READY != huart2.gState)
    {
        return UART_DRIVER_ERROR_BUSY;
    }

    const HAL_StatusTypeDef hal_status = HAL_UART_Transmit_DMA(
        &huart2, (const uint8_t *)(uintptr_t)data, (uint16_t)length);

    if (HAL_BUSY == hal_status)
    {
        return UART_DRIVER_ERROR_BUSY;
    }

    return (HAL_OK == hal_status) ? UART_DRIVER_OK : UART_DRIVER_ERROR;
}

bool bsp_uart_driver_tx_busy(void)
{
    if (HAL_UART_STATE_READY == huart2.gState)
    {
        return false;
    }

    /*
     * 正常情况下，TX DMA完成中断会开启USART TC中断，随后HAL将gState恢复
     * 为READY。若目标板上的TC中断收尾没有执行，但硬件TC标志已经置位，
     * 说明最后一个停止位确实已经发送完成。此时只终止TX并恢复HAL状态；
     * HAL_UART_AbortTransmit()不会终止独立运行的RX循环DMA。
     *
     * 该兜底仍使用DMA搬运发送数据，只把“完成确认”从纯中断扩展为任务轮询，
     * 避免一次完成中断丢失后永久卡在HAL_UART_STATE_BUSY_TX。
     */
    if ((HAL_UART_STATE_BUSY_TX == huart2.gState) &&
        (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) != RESET))
    {
        if (HAL_OK == HAL_UART_AbortTransmit(&huart2))
        {
            return false;
        }
    }

    return true;
}
