/**
 * @file uart_test_task.c
 * @brief USART2 DMA阶段1线路验证任务。
 * @details PC发送以换行结尾的HELLO或PING命令，MCU通过TX DMA返回READY或
 * PONG。该任务只用于验证CP210x、UART、DMA和WSL链路，不包含ROS 2逻辑。
 */
#include "uart_test_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_uart_driver.h"

#ifndef FIRMWARE_BUILD_TAG
#error "FIRMWARE_BUILD_TAG must be defined by the top-level CMakeLists.txt"
#endif

/** @brief CMake版本标签与阶段1握手响应共用，防止文件名和固件版本不一致。 */
#define UART_TEST_READY_RESPONSE \
    "READY,TACGLOVE_UART_DMA_" FIRMWARE_BUILD_TAG ",115200\r\n"

#define UART_TEST_TASK_STACK_WORDS 64U                    /**< 64 words，即256字节。 */
#define UART_TEST_TASK_PRIORITY    (tskIDLE_PRIORITY + 2U) /**< 与按键扫描任务同级。 */
#define UART_TEST_POLL_PERIOD_MS   2U                     /**< DMA缓冲轮询周期。 */
#define UART_TEST_TX_TIMEOUT_MS    100U                   /**< 单帧DMA发送超时。 */
#define UART_TEST_LINE_SIZE        32U                    /**< 最大输入命令长度。 */
#define UART_TEST_TX_SIZE          48U                    /**< 最大响应长度。 */

/** @brief 当前正在接收的一行ASCII命令。 */
static uint8_t s_line_buffer[UART_TEST_LINE_SIZE];
static size_t s_line_length;

/** @brief TX DMA专用静态缓冲区，发送完成前不会被改写。 */
static uint8_t s_tx_buffer[UART_TEST_TX_SIZE];

/**
 * @brief 使用TX DMA发送一段静态缓冲数据并等待完成。
 * @param data 待发送数据。
 * @param length 数据长度。
 * @return true表示DMA在超时前发送完成。
 */
static bool uart_test_send(const uint8_t *data, size_t length)
{
    const TickType_t start_tick = xTaskGetTickCount();

    while (bsp_uart_driver_tx_busy())
    {
        if ((xTaskGetTickCount() - start_tick) >=
            pdMS_TO_TICKS(UART_TEST_TX_TIMEOUT_MS))
        {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    if (UART_DRIVER_OK != bsp_uart_driver_transmit_dma(data, length))
    {
        return false;
    }

    while (bsp_uart_driver_tx_busy())
    {
        if ((xTaskGetTickCount() - start_tick) >=
            pdMS_TO_TICKS(UART_TEST_TX_TIMEOUT_MS))
        {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    return true;
}

/** @brief 发送固定字符串，字符串内容会先复制到DMA专用RAM缓冲区。 */
static bool uart_test_send_text(const char *text)
{
    const size_t length = strlen(text);
    if (length > sizeof(s_tx_buffer))
    {
        return false;
    }

    (void)memcpy(s_tx_buffer, text, length);
    return uart_test_send(s_tx_buffer, length);
}

/**
 * @brief 检查当前命令是否为合法PING，并返回序号起点。
 * @param[out] sequence_offset PING序号在行缓冲区中的偏移。
 * @return true表示格式为PING,加1～10位十进制数字。
 */
static bool uart_test_parse_ping(size_t *sequence_offset)
{
    static const uint8_t prefix[] = {'P', 'I', 'N', 'G', ','};

    if ((NULL == sequence_offset) ||
        (s_line_length <= sizeof(prefix)) ||
        (s_line_length > (sizeof(prefix) + 10U)) ||
        (0 != memcmp(s_line_buffer, prefix, sizeof(prefix))))
    {
        return false;
    }

    for (size_t index = sizeof(prefix); index < s_line_length; ++index)
    {
        if ((s_line_buffer[index] < (uint8_t)'0') ||
            (s_line_buffer[index] > (uint8_t)'9'))
        {
            return false;
        }
    }

    *sequence_offset = sizeof(prefix);
    return true;
}

/** @brief 处理一条已经去除换行符的命令。 */
static void uart_test_process_line(void)
{
    static const uint8_t hello[] = {'H', 'E', 'L', 'L', 'O'};
    static const uint8_t pong_prefix[] = {'P', 'O', 'N', 'G', ','};

    if ((s_line_length == sizeof(hello)) &&
        (0 == memcmp(s_line_buffer, hello, sizeof(hello))))
    {
        (void)uart_test_send_text(UART_TEST_READY_RESPONSE);
        return;
    }

    size_t sequence_offset = 0U;
    if (uart_test_parse_ping(&sequence_offset))
    {
        const size_t sequence_length = s_line_length - sequence_offset;
        size_t tx_length = 0U;

        (void)memcpy(&s_tx_buffer[tx_length], pong_prefix, sizeof(pong_prefix));
        tx_length += sizeof(pong_prefix);
        (void)memcpy(&s_tx_buffer[tx_length],
                     &s_line_buffer[sequence_offset],
                     sequence_length);
        tx_length += sequence_length;
        s_tx_buffer[tx_length++] = (uint8_t)'\r';
        s_tx_buffer[tx_length++] = (uint8_t)'\n';

        (void)uart_test_send(s_tx_buffer, tx_length);
        return;
    }

    (void)uart_test_send_text("ERR,BAD_COMMAND\r\n");
}

/** @brief 将DMA收到的一个字节送入换行命令解析器。 */
static void uart_test_process_byte(uint8_t byte)
{
    if ((uint8_t)'\r' == byte)
    {
        return;
    }

    if ((uint8_t)'\n' == byte)
    {
        if (s_line_length > 0U)
        {
            uart_test_process_line();
        }
        s_line_length = 0U;
        return;
    }

    if (s_line_length < sizeof(s_line_buffer))
    {
        s_line_buffer[s_line_length++] = byte;
    }
    else
    {
        s_line_length = 0U;
        (void)uart_test_send_text("ERR,LINE_TOO_LONG\r\n");
    }
}

/** @brief USART2 DMA测试任务入口。 */
static void uart_test_task_entry(void *argument)
{
    (void)argument;

    if (UART_DRIVER_OK != bsp_uart_driver_init())
    {
        /* 常见原因：CubeMX中的USART2_RX DMA仍为Normal而不是Circular。 */
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    s_line_length = 0U;

    /*
     * 不主动发送READY。只有收到HELLO才应答，避免PC把上电时残留在串口
     * 缓冲区中的主动帧误判成一次成功握手。
     */

    for (;;)
    {
        uint8_t received[16];
        const size_t received_length =
            bsp_uart_driver_read(received, sizeof(received));

        for (size_t index = 0U; index < received_length; ++index)
        {
            uart_test_process_byte(received[index]);
        }

        vTaskDelay(pdMS_TO_TICKS(UART_TEST_POLL_PERIOD_MS));
    }
}

task_status_t uart_test_task_create(void)
{
    const BaseType_t result = xTaskCreate(uart_test_task_entry,
                                          "uart_test",
                                          UART_TEST_TASK_STACK_WORDS,
                                          NULL,
                                          UART_TEST_TASK_PRIORITY,
                                          NULL);
    return (pdPASS == result) ? TASK_OK : TASK_ERROR_NO_MEMORY;
}
