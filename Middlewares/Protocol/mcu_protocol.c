/**
 * @file mcu_protocol.c
 * @brief 轻量二进制帧协议编解码实现。
 */
#include "mcu_protocol.h"

#include <stdbool.h>
#include <string.h>

enum
{
    PARSER_WAIT_SOF_0 = 0,
    PARSER_WAIT_SOF_1,
    PARSER_VERSION,
    PARSER_MESSAGE_TYPE,
    PARSER_SEQUENCE_LOW,
    PARSER_SEQUENCE_HIGH,
    PARSER_LENGTH_LOW,
    PARSER_LENGTH_HIGH,
    PARSER_PAYLOAD,
    PARSER_CRC_LOW,
    PARSER_CRC_HIGH
};

/** @brief 将一个字节加入CRC16-CCITT-FALSE滚动计算。 */
static uint16_t protocol_crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        crc = ((crc & 0x8000U) != 0U)
            ? (uint16_t)((crc << 1U) ^ 0x1021U)
            : (uint16_t)(crc << 1U);
    }
    return crc;
}

/** @brief 回到等待帧头状态，但保留frame内容供调用方读取。 */
static void protocol_parser_wait_for_sof(mcu_protocol_parser_t *parser)
{
    parser->state = PARSER_WAIT_SOF_0;
    parser->payload_index = 0U;
    parser->crc_calculated = 0xFFFFU;
    parser->crc_received = 0U;
}

/**
 * @brief 格式错误后重同步，并把当前字节保留为潜在的下一帧SOF0。
 * @details 例如字节流AA 55 AA 55中，第3个AA既是非法Version，也可能是
 * 下一帧的SOF0；保留它可避免丢失紧随其后的合法帧。
 */
static void protocol_parser_resync_from_byte(mcu_protocol_parser_t *parser,
                                             uint8_t byte)
{
    protocol_parser_wait_for_sof(parser);
    if (MCU_PROTOCOL_SOF_0 == byte)
    {
        parser->state = PARSER_WAIT_SOF_1;
    }
}

uint16_t mcu_protocol_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    if ((NULL == data) && (length > 0U))
    {
        return crc;
    }

    for (size_t index = 0U; index < length; ++index)
    {
        crc = protocol_crc16_update(crc, data[index]);
    }
    return crc;
}

mcu_protocol_status_t mcu_protocol_encode(
    uint8_t message_type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    if ((NULL == output) || (NULL == output_length) ||
        ((payload_length > 0U) && (NULL == payload)))
    {
        return MCU_PROTOCOL_ERROR_PARAMETER;
    }

    const size_t frame_length =
        MCU_PROTOCOL_FIXED_SIZE + (size_t)payload_length;
    if ((payload_length > MCU_PROTOCOL_MAX_PAYLOAD_SIZE) ||
        (output_capacity < frame_length))
    {
        return MCU_PROTOCOL_ERROR_CAPACITY;
    }

    output[0] = MCU_PROTOCOL_SOF_0;
    output[1] = MCU_PROTOCOL_SOF_1;
    output[2] = MCU_PROTOCOL_VERSION;
    output[3] = message_type;
    output[4] = (uint8_t)(sequence & 0xFFU);
    output[5] = (uint8_t)(sequence >> 8U);
    output[6] = (uint8_t)(payload_length & 0xFFU);
    output[7] = (uint8_t)(payload_length >> 8U);

    if (payload_length > 0U)
    {
        (void)memcpy(&output[8], payload, payload_length);
    }

    const uint16_t crc =
        mcu_protocol_crc16(&output[2], 6U + payload_length);
    output[8U + payload_length] = (uint8_t)(crc & 0xFFU);
    output[9U + payload_length] = (uint8_t)(crc >> 8U);
    *output_length = frame_length;
    return MCU_PROTOCOL_OK;
}

void mcu_protocol_parser_init(mcu_protocol_parser_t *parser)
{
    if (NULL == parser)
    {
        return;
    }

    (void)memset(parser, 0, sizeof(*parser));
    protocol_parser_wait_for_sof(parser);
}

mcu_protocol_parse_result_t mcu_protocol_parser_consume(
    mcu_protocol_parser_t *parser,
    uint8_t byte)
{
    if (NULL == parser)
    {
        return MCU_PROTOCOL_PARSE_FORMAT_ERROR;
    }

    switch (parser->state)
    {
        case PARSER_WAIT_SOF_0:
            if (MCU_PROTOCOL_SOF_0 == byte)
            {
                parser->state = PARSER_WAIT_SOF_1;
            }
            break;

        case PARSER_WAIT_SOF_1:
            if (MCU_PROTOCOL_SOF_1 == byte)
            {
                parser->state = PARSER_VERSION;
                parser->crc_calculated = 0xFFFFU;
            }
            else if (MCU_PROTOCOL_SOF_0 != byte)
            {
                parser->state = PARSER_WAIT_SOF_0;
            }
            break;

        case PARSER_VERSION:
            if (MCU_PROTOCOL_VERSION != byte)
            {
                protocol_parser_resync_from_byte(parser, byte);
                return MCU_PROTOCOL_PARSE_FORMAT_ERROR;
            }
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            parser->state = PARSER_MESSAGE_TYPE;
            break;

        case PARSER_MESSAGE_TYPE:
            parser->frame.message_type = byte;
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            parser->state = PARSER_SEQUENCE_LOW;
            break;

        case PARSER_SEQUENCE_LOW:
            parser->frame.sequence = byte;
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            parser->state = PARSER_SEQUENCE_HIGH;
            break;

        case PARSER_SEQUENCE_HIGH:
            parser->frame.sequence |= (uint16_t)byte << 8U;
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            parser->state = PARSER_LENGTH_LOW;
            break;

        case PARSER_LENGTH_LOW:
            parser->frame.payload_length = byte;
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            parser->state = PARSER_LENGTH_HIGH;
            break;

        case PARSER_LENGTH_HIGH:
            parser->frame.payload_length |= (uint16_t)byte << 8U;
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            if (parser->frame.payload_length > MCU_PROTOCOL_MAX_PAYLOAD_SIZE)
            {
                protocol_parser_resync_from_byte(parser, byte);
                return MCU_PROTOCOL_PARSE_FORMAT_ERROR;
            }
            parser->payload_index = 0U;
            parser->state = (0U == parser->frame.payload_length)
                ? PARSER_CRC_LOW
                : PARSER_PAYLOAD;
            break;

        case PARSER_PAYLOAD:
            parser->frame.payload[parser->payload_index++] = byte;
            parser->crc_calculated =
                protocol_crc16_update(parser->crc_calculated, byte);
            if (parser->payload_index >= parser->frame.payload_length)
            {
                parser->state = PARSER_CRC_LOW;
            }
            break;

        case PARSER_CRC_LOW:
            parser->crc_received = byte;
            parser->state = PARSER_CRC_HIGH;
            break;

        case PARSER_CRC_HIGH:
        {
            parser->crc_received |= (uint16_t)byte << 8U;
            const bool crc_matches =
                (parser->crc_received == parser->crc_calculated);
            protocol_parser_wait_for_sof(parser);
            return crc_matches
                ? MCU_PROTOCOL_PARSE_FRAME_READY
                : MCU_PROTOCOL_PARSE_CRC_ERROR;
        }

        default:
            protocol_parser_resync_from_byte(parser, byte);
            return MCU_PROTOCOL_PARSE_FORMAT_ERROR;
    }

    return MCU_PROTOCOL_PARSE_NONE;
}
