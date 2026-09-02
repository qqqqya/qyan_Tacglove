/**
 * @file mcu_protocol.h
 * @brief MCU与PC之间的轻量二进制帧协议。
 * @details 本模块只负责帧编解码、CRC和字节流重同步，不依赖HAL、FreeRTOS、
 * UART或ROS 2。所有多字节整数均使用小端字节序。
 */
#ifndef MCU_PROTOCOL_H
#define MCU_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCU_PROTOCOL_SOF_0            0xAAU /**< 帧头第1字节。 */
#define MCU_PROTOCOL_SOF_1            0x55U /**< 帧头第2字节。 */
#define MCU_PROTOCOL_VERSION          0x01U /**< 当前协议版本。 */
#define MCU_PROTOCOL_MAX_PAYLOAD_SIZE 64U   /**< 单帧最大Payload。 */
#define MCU_PROTOCOL_FIXED_SIZE       10U   /**< 无Payload时的完整帧长度。 */
#define MCU_PROTOCOL_MAX_FRAME_SIZE   \
    (MCU_PROTOCOL_FIXED_SIZE + MCU_PROTOCOL_MAX_PAYLOAD_SIZE)

/** @brief UART协议消息类型；阶段3在基础请求/响应上增加命令和异步事件。 */
typedef enum
{
    MCU_MSG_HELLO_REQUEST = 0x01,  /**< 查询协议和固件能力。 */
    MCU_MSG_PING_REQUEST = 0x02,   /**< 链路测试，Payload原样回显。 */
    MCU_MSG_STATUS_REQUEST = 0x03, /**< 查询系统状态和通信统计。 */
    MCU_MSG_COMMAND_REQUEST = 0x10, /**< 下发采集或蜂鸣业务命令。 */

    MCU_MSG_BUTTON_EVENT = 0x40, /**< MCU主动上报PB6按键业务事件。 */

    MCU_MSG_HELLO_RESPONSE = 0x81,  /**< HELLO响应。 */
    MCU_MSG_PING_RESPONSE = 0x82,   /**< PING响应。 */
    MCU_MSG_STATUS_RESPONSE = 0x83, /**< STATUS响应。 */
    MCU_MSG_COMMAND_RESPONSE = 0x90, /**< 命令接收结果。 */
    MCU_MSG_ERROR_RESPONSE = 0xFF   /**< 合法帧的命令级错误响应。 */
} mcu_protocol_message_type_t;

/** @brief COMMAND请求Payload中的业务命令。 */
typedef enum
{
    MCU_COMMAND_CAPTURE_START = 1, /**< 请求进入采集。 */
    MCU_COMMAND_CAPTURE_STOP = 2,  /**< 请求结束采集并回到待机。 */
    MCU_COMMAND_BEEP_SHORT = 3     /**< 请求蜂鸣器短鸣一次。 */
} mcu_protocol_command_t;

/** @brief COMMAND响应Payload中的接收结果。 */
typedef enum
{
    MCU_COMMAND_RESULT_ACCEPTED = 0,         /**< 已进入控制事件队列。 */
    MCU_COMMAND_RESULT_ALREADY_IN_STATE = 1, /**< 已处于目标状态。 */
    MCU_COMMAND_RESULT_BUSY = 2,             /**< 当前状态不允许执行。 */
    MCU_COMMAND_RESULT_QUEUE_FULL = 3,       /**< 控制事件队列已满。 */
    MCU_COMMAND_RESULT_UNSUPPORTED = 4       /**< 不支持该命令值。 */
} mcu_protocol_command_result_t;

/** @brief BUTTON_EVENT Payload中的按键业务事件。 */
typedef enum
{
    MCU_BUTTON_EVENT_CAPTURE_START = 1, /**< PB6请求开始采集。 */
    MCU_BUTTON_EVENT_CAPTURE_STOP = 2   /**< PB6请求停止采集。 */
} mcu_protocol_button_event_t;

/** @brief ERROR响应Payload中的错误码。 */
typedef enum
{
    MCU_PROTOCOL_ERROR_UNSUPPORTED_MESSAGE = 1, /**< 不支持该消息类型。 */
    MCU_PROTOCOL_ERROR_BAD_PAYLOAD = 2          /**< Payload长度或内容非法。 */
} mcu_protocol_error_code_t;

/** @brief 协议函数返回状态。 */
typedef enum
{
    MCU_PROTOCOL_OK = 0,
    MCU_PROTOCOL_ERROR_PARAMETER = 1,
    MCU_PROTOCOL_ERROR_CAPACITY = 2
} mcu_protocol_status_t;

/** @brief 流式解析器每输入一个字节后的结果。 */
typedef enum
{
    MCU_PROTOCOL_PARSE_NONE = 0,         /**< 尚未形成完整帧。 */
    MCU_PROTOCOL_PARSE_FRAME_READY = 1,  /**< 一帧通过版本、长度和CRC检查。 */
    MCU_PROTOCOL_PARSE_CRC_ERROR = 2,    /**< 收到完整帧但CRC不匹配。 */
    MCU_PROTOCOL_PARSE_FORMAT_ERROR = 3  /**< 版本或Payload长度非法。 */
} mcu_protocol_parse_result_t;

/** @brief 一条已经通过校验的协议帧。 */
typedef struct
{
    uint8_t message_type; /**< mcu_protocol_message_type_t。 */
    uint16_t sequence;    /**< 请求/响应匹配序号。 */
    uint16_t payload_length;
    uint8_t payload[MCU_PROTOCOL_MAX_PAYLOAD_SIZE];
} mcu_protocol_frame_t;

/**
 * @brief 无动态内存的字节流解析器上下文。
 * @note 字段属于模块内部状态，调用方只应通过公开函数操作。
 */
typedef struct
{
    uint8_t state;
    uint16_t payload_index;
    uint16_t crc_calculated;
    uint16_t crc_received;
    mcu_protocol_frame_t frame;
} mcu_protocol_parser_t;

/**
 * @brief 计算CRC16-CCITT-FALSE。
 * @param data 输入字节。
 * @param length 输入长度。
 * @return CRC值；多字节写入帧时使用小端顺序。
 * @details 参数为poly=0x1021、init=0xFFFF、refin=false、refout=false、
 * xorout=0x0000；标准测试串"123456789"结果为0x29B1。
 */
uint16_t mcu_protocol_crc16(const uint8_t *data, size_t length);

/**
 * @brief 把消息编码为一条完整二进制帧。
 * @param message_type 消息类型。
 * @param sequence 16位序号。
 * @param payload Payload；长度为0时允许为NULL。
 * @param payload_length Payload长度，最大64字节。
 * @param[out] output 输出缓冲区。
 * @param output_capacity 输出容量。
 * @param[out] output_length 实际帧长度。
 * @retval MCU_PROTOCOL_OK 编码成功。
 * @retval MCU_PROTOCOL_ERROR_PARAMETER 参数非法。
 * @retval MCU_PROTOCOL_ERROR_CAPACITY Payload或输出容量超限。
 */
mcu_protocol_status_t mcu_protocol_encode(
    uint8_t message_type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

/** @brief 初始化或清空流式解析器。 */
void mcu_protocol_parser_init(mcu_protocol_parser_t *parser);

/**
 * @brief 向流式解析器输入一个字节。
 * @param parser 解析器上下文。
 * @param byte 新收到的字节。
 * @return 解析进度或错误结果。
 * @note 返回FRAME_READY时，完整帧保存在parser->frame中，应在继续输入前处理。
 */
mcu_protocol_parse_result_t mcu_protocol_parser_consume(
    mcu_protocol_parser_t *parser,
    uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* MCU_PROTOCOL_H */
