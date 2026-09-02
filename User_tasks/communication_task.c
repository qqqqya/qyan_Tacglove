/**
 * @file communication_task.c
 * @brief USART2 DMA上的阶段3业务通信任务。
 * @details 本任务是UART BSP的唯一应用层使用者，负责字节流解析、命令分发、
 * 按键事件上报、响应编码和通信统计；不包含ROS 2 API。
 */
#include "communication_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_uart_driver.h"
#include "mcu_protocol.h"
#include "system_event.h"
#include "system_status.h"

#ifndef FIRMWARE_BUILD_TAG
#error "FIRMWARE_BUILD_TAG must be defined by the top-level CMakeLists.txt"
#endif

#define COMM_TASK_STACK_WORDS    80U                    /**< 80 words，即320字节。 */
#define COMM_TASK_PRIORITY       (tskIDLE_PRIORITY + 2U)
#define COMM_POLL_PERIOD_MS      2U
#define COMM_TX_TIMEOUT_MS       100U
#define COMM_RX_CHUNK_SIZE       24U
#define COMM_DEVICE_TYPE         1U                     /**< TacGlove采集控制板。 */

/** @brief HELLO能力位。 */
enum
{
    COMM_CAPABILITY_PING = 1U << 0,
    COMM_CAPABILITY_STATUS = 1U << 1,
    COMM_CAPABILITY_CRC16 = 1U << 2,
    COMM_CAPABILITY_SEQUENCE = 1U << 3,
    COMM_CAPABILITY_STREAM_RESYNC = 1U << 4,
    COMM_CAPABILITY_COMMAND = 1U << 5,
    COMM_CAPABILITY_BUTTON_EVENT = 1U << 6
};

#define COMM_CAPABILITIES                                             \
    (COMM_CAPABILITY_PING | COMM_CAPABILITY_STATUS |                  \
     COMM_CAPABILITY_CRC16 | COMM_CAPABILITY_SEQUENCE |              \
     COMM_CAPABILITY_STREAM_RESYNC | COMM_CAPABILITY_COMMAND |       \
     COMM_CAPABILITY_BUTTON_EVENT)

/** @brief 通信统计，STATUS响应按固定小端字段发送。 */
typedef struct
{
    uint32_t rx_bytes;
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t crc_errors;
    uint32_t format_errors;
    uint32_t tx_errors;
} communication_counters_t;

static mcu_protocol_parser_t s_parser;
static uint8_t s_tx_frame[MCU_PROTOCOL_MAX_FRAME_SIZE];
static communication_counters_t s_counters;
static uint16_t s_button_event_sequence;
static const uint8_t s_firmware_tag[] = FIRMWARE_BUILD_TAG;

_Static_assert((sizeof(s_firmware_tag) - 1U) <=
                   (MCU_PROTOCOL_MAX_PAYLOAD_SIZE - 9U),
               "FIRMWARE_BUILD_TAG is too long for HELLO response");

/** @brief 把16位整数按小端写入Payload。 */
static void communication_put_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)(value >> 8U);
}

/** @brief 把32位整数按小端写入Payload。 */
static void communication_put_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[3] = (uint8_t)(value >> 24U);
}

/** @brief 从Payload按小端读取32位整数。 */
static uint32_t communication_get_u32(const uint8_t *source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

/**
 * @brief 等待TX可用、启动DMA并等待实际发送完成。
 * @return true表示整帧在超时前发送完成。
 */
static bool communication_transmit(size_t frame_length)
{
    const TickType_t start_tick = xTaskGetTickCount();

    while (bsp_uart_driver_tx_busy())
    {
        if ((xTaskGetTickCount() - start_tick) >=
            pdMS_TO_TICKS(COMM_TX_TIMEOUT_MS))
        {
            ++s_counters.tx_errors;
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    if (UART_DRIVER_OK !=
        bsp_uart_driver_transmit_dma(s_tx_frame, frame_length))
    {
        ++s_counters.tx_errors;
        return false;
    }

    while (bsp_uart_driver_tx_busy())
    {
        if ((xTaskGetTickCount() - start_tick) >=
            pdMS_TO_TICKS(COMM_TX_TIMEOUT_MS))
        {
            ++s_counters.tx_errors;
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    ++s_counters.tx_frames;
    return true;
}

/** @brief 编码并发送一条响应帧。 */
static bool communication_send_frame(uint8_t message_type,
                                     uint16_t sequence,
                                     const uint8_t *payload,
                                     uint16_t payload_length)
{
    size_t frame_length = 0U;
    if (MCU_PROTOCOL_OK != mcu_protocol_encode(message_type,
                                                sequence,
                                                payload,
                                                payload_length,
                                                s_tx_frame,
                                                sizeof(s_tx_frame),
                                                &frame_length))
    {
        ++s_counters.tx_errors;
        return false;
    }

    return communication_transmit(frame_length);
}

/** @brief 对格式正确但命令非法的请求返回错误码。 */
static void communication_send_command_error(const mcu_protocol_frame_t *frame,
                                             mcu_protocol_error_code_t error)
{
    const uint8_t payload[2] = {(uint8_t)error, frame->message_type};
    (void)communication_send_frame(MCU_MSG_ERROR_RESPONSE,
                                   frame->sequence,
                                   payload,
                                   sizeof(payload));
}

/** @brief 构造HELLO能力响应。 */
static void communication_handle_hello(const mcu_protocol_frame_t *frame)
{
    if (0U != frame->payload_length)
    {
        communication_send_command_error(frame, MCU_PROTOCOL_ERROR_BAD_PAYLOAD);
        return;
    }

    uint8_t payload[9U + sizeof(s_firmware_tag) - 1U];
    payload[0] = MCU_PROTOCOL_VERSION;
    payload[1] = COMM_DEVICE_TYPE;
    communication_put_u16(&payload[2], MCU_PROTOCOL_MAX_PAYLOAD_SIZE);
    communication_put_u32(&payload[4], COMM_CAPABILITIES);
    payload[8] = (uint8_t)(sizeof(s_firmware_tag) - 1U);
    (void)memcpy(&payload[9], s_firmware_tag, sizeof(s_firmware_tag) - 1U);

    (void)communication_send_frame(MCU_MSG_HELLO_RESPONSE,
                                   frame->sequence,
                                   payload,
                                   sizeof(payload));
}

/** @brief 构造系统状态和通信统计响应。 */
static void communication_handle_status(const mcu_protocol_frame_t *frame)
{
    if (0U != frame->payload_length)
    {
        communication_send_command_error(frame, MCU_PROTOCOL_ERROR_BAD_PAYLOAD);
        return;
    }

    const system_status_snapshot_t status = system_status_get();
    uint8_t payload[36] = {0U};
    payload[0] = (uint8_t)status.state;
    payload[1] = 0U;
    communication_put_u16(&payload[2], status.fault_code);

    const uint32_t uptime_ms =
        (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
    communication_put_u32(&payload[4], uptime_ms);
    communication_put_u32(&payload[8], s_counters.rx_frames);
    communication_put_u32(&payload[12], s_counters.tx_frames);
    communication_put_u32(&payload[16], s_counters.crc_errors);
    communication_put_u32(&payload[20], s_counters.format_errors);
    communication_put_u32(&payload[24], s_counters.tx_errors);
    communication_put_u32(&payload[28], s_counters.rx_bytes);
    communication_put_u32(&payload[32], (uint32_t)xPortGetFreeHeapSize());

    (void)communication_send_frame(MCU_MSG_STATUS_RESPONSE,
                                   frame->sequence,
                                   payload,
                                   sizeof(payload));
}

/**
 * @brief 校验并投递一条PC业务命令，然后返回接收结果。
 * @details ACK只表示命令已进入LED状态机队列；实际状态由后续STATUS确认。
 */
static void communication_handle_command(const mcu_protocol_frame_t *frame)
{
    if ((6U != frame->payload_length) || (0U != frame->payload[1]))
    {
        communication_send_command_error(frame, MCU_PROTOCOL_ERROR_BAD_PAYLOAD);
        return;
    }

    const uint8_t command = frame->payload[0];
    const uint32_t command_id = communication_get_u32(&frame->payload[2]);
    const system_status_snapshot_t status = system_status_get();
    system_event_t event = {
        .type = SYSTEM_EVENT_NONE,
        .timestamp_ms =
            (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS};
    mcu_protocol_command_result_t result = MCU_COMMAND_RESULT_ACCEPTED;

    switch (command)
    {
        case MCU_COMMAND_CAPTURE_START:
            if (SYSTEM_STATE_IDLE == status.state)
            {
                event.type = SYSTEM_EVENT_CAPTURE_START_REQUEST;
            }
            else if ((SYSTEM_STATE_CAPTURE_PREPARING == status.state) ||
                     (SYSTEM_STATE_CAPTURING == status.state))
            {
                result = MCU_COMMAND_RESULT_ALREADY_IN_STATE;
            }
            else
            {
                result = MCU_COMMAND_RESULT_BUSY;
            }
            break;

        case MCU_COMMAND_CAPTURE_STOP:
            if (SYSTEM_STATE_CAPTURING == status.state)
            {
                event.type = SYSTEM_EVENT_CAPTURE_STOP_REQUEST;
            }
            else if (SYSTEM_STATE_IDLE == status.state)
            {
                result = MCU_COMMAND_RESULT_ALREADY_IN_STATE;
            }
            else
            {
                result = MCU_COMMAND_RESULT_BUSY;
            }
            break;

        case MCU_COMMAND_BEEP_SHORT:
            if (SYSTEM_STATE_FAULT == status.state)
            {
                result = MCU_COMMAND_RESULT_BUSY;
            }
            else
            {
                event.type = SYSTEM_EVENT_BEEP_SHORT_REQUEST;
            }
            break;

        default:
            result = MCU_COMMAND_RESULT_UNSUPPORTED;
            break;
    }

    if ((SYSTEM_EVENT_NONE != event.type) &&
        (TASK_OK != system_event_publish_control(&event, 0U)))
    {
        result = MCU_COMMAND_RESULT_QUEUE_FULL;
    }

    uint8_t payload[9U] = {0U};
    payload[0] = command;
    payload[1] = (uint8_t)result;
    communication_put_u32(&payload[2], command_id);
    payload[6] = (uint8_t)status.state;
    communication_put_u16(&payload[7], status.fault_code);
    (void)communication_send_frame(MCU_MSG_COMMAND_RESPONSE,
                                   frame->sequence,
                                   payload,
                                   sizeof(payload));
}

/** @brief 将PB6产生的开始/停止请求作为异步事件发送给PC。 */
static void communication_publish_button_event(const system_event_t *event)
{
    uint8_t button_event = 0U;
    uint8_t state_before = (uint8_t)SYSTEM_STATE_SELF_TEST;

    if (SYSTEM_EVENT_CAPTURE_START_REQUEST == event->type)
    {
        button_event = (uint8_t)MCU_BUTTON_EVENT_CAPTURE_START;
        state_before = (uint8_t)SYSTEM_STATE_IDLE;
    }
    else if (SYSTEM_EVENT_CAPTURE_STOP_REQUEST == event->type)
    {
        button_event = (uint8_t)MCU_BUTTON_EVENT_CAPTURE_STOP;
        state_before = (uint8_t)SYSTEM_STATE_CAPTURING;
    }
    else
    {
        return;
    }

    ++s_button_event_sequence;
    if (0U == s_button_event_sequence)
    {
        ++s_button_event_sequence;
    }

    uint8_t payload[7U] = {0U};
    payload[0] = 1U; /**< PB6采集按键ID。 */
    payload[1] = button_event;
    communication_put_u32(&payload[2], event->timestamp_ms);
    payload[6] = state_before;
    (void)communication_send_frame(MCU_MSG_BUTTON_EVENT,
                                   s_button_event_sequence,
                                   payload,
                                   sizeof(payload));
}

/** @brief 分发一条通过CRC检查的请求帧。 */
static void communication_process_frame(const mcu_protocol_frame_t *frame)
{
    ++s_counters.rx_frames;

    switch (frame->message_type)
    {
        case MCU_MSG_HELLO_REQUEST:
            communication_handle_hello(frame);
            break;

        case MCU_MSG_PING_REQUEST:
            (void)communication_send_frame(MCU_MSG_PING_RESPONSE,
                                           frame->sequence,
                                           frame->payload,
                                           frame->payload_length);
            break;

        case MCU_MSG_STATUS_REQUEST:
            communication_handle_status(frame);
            break;

        case MCU_MSG_COMMAND_REQUEST:
            communication_handle_command(frame);
            break;

        default:
            communication_send_command_error(
                frame, MCU_PROTOCOL_ERROR_UNSUPPORTED_MESSAGE);
            break;
    }
}

/** @brief 阶段3二进制业务通信任务入口。 */
static void communication_task_entry(void *argument)
{
    (void)argument;
    (void)memset(&s_counters, 0, sizeof(s_counters));
    s_button_event_sequence = 0U;
    mcu_protocol_parser_init(&s_parser);

    if (UART_DRIVER_OK != bsp_uart_driver_init())
    {
        system_status_set(SYSTEM_STATE_FAULT,
                          SYSTEM_FAULT_COMMUNICATION_INIT);
        for (;;)
        {
            /* LED任务可能更新全局状态，因此持续保持通信初始化故障。 */
            system_status_set(SYSTEM_STATE_FAULT,
                              SYSTEM_FAULT_COMMUNICATION_INIT);
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }

    for (;;)
    {
        uint8_t received[COMM_RX_CHUNK_SIZE];
        const size_t received_length =
            bsp_uart_driver_read(received, sizeof(received));
        s_counters.rx_bytes += (uint32_t)received_length;

        for (size_t index = 0U; index < received_length; ++index)
        {
            const mcu_protocol_parse_result_t result =
                mcu_protocol_parser_consume(&s_parser, received[index]);

            if (MCU_PROTOCOL_PARSE_FRAME_READY == result)
            {
                communication_process_frame(&s_parser.frame);
            }
            else if (MCU_PROTOCOL_PARSE_CRC_ERROR == result)
            {
                ++s_counters.crc_errors;
            }
            else if (MCU_PROTOCOL_PARSE_FORMAT_ERROR == result)
            {
                ++s_counters.format_errors;
            }
            else
            {
                /* 尚未形成完整帧，继续输入后续字节。 */
            }
        }

        system_event_t observed_event = {.type = SYSTEM_EVENT_NONE,
                                         .timestamp_ms = 0U};
        if (TASK_OK == system_event_observe(&observed_event, 0U))
        {
            communication_publish_button_event(&observed_event);
        }

        vTaskDelay(pdMS_TO_TICKS(COMM_POLL_PERIOD_MS));
    }
}

task_status_t communication_task_create(void)
{
    const BaseType_t result = xTaskCreate(communication_task_entry,
                                          "comm",
                                          COMM_TASK_STACK_WORDS,
                                          NULL,
                                          COMM_TASK_PRIORITY,
                                          NULL);
    return (pdPASS == result) ? TASK_OK : TASK_ERROR_NO_MEMORY;
}
