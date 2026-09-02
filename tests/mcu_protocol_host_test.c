/**
 * @file mcu_protocol_host_test.c
 * @brief 不依赖STM32硬件的UART协议编解码单元测试。
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mcu_protocol.h"

static void consume_frame(mcu_protocol_parser_t *parser,
                          const uint8_t *data,
                          size_t length,
                          mcu_protocol_parse_result_t expected)
{
    mcu_protocol_parse_result_t result = MCU_PROTOCOL_PARSE_NONE;
    for (size_t index = 0U; index < length; ++index)
    {
        result = mcu_protocol_parser_consume(parser, data[index]);
    }
    assert(expected == result);
}

int main(void)
{
    static const uint8_t crc_vector[] = "123456789";
    assert(0x29B1U ==
           mcu_protocol_crc16(crc_vector, sizeof(crc_vector) - 1U));

    static const uint8_t payload[] = {0x00U, 0xAAU, 0x55U, 0xFFU};
    uint8_t encoded[MCU_PROTOCOL_MAX_FRAME_SIZE] = {0U};
    size_t encoded_length = 0U;
    assert(MCU_PROTOCOL_OK ==
           mcu_protocol_encode(MCU_MSG_PING_REQUEST,
                               0x1234U,
                               payload,
                               sizeof(payload),
                               encoded,
                               sizeof(encoded),
                               &encoded_length));
    assert((MCU_PROTOCOL_FIXED_SIZE + sizeof(payload)) == encoded_length);

    mcu_protocol_parser_t parser;
    mcu_protocol_parser_init(&parser);
    consume_frame(&parser,
                  encoded,
                  encoded_length,
                  MCU_PROTOCOL_PARSE_FRAME_READY);
    assert(MCU_MSG_PING_REQUEST == parser.frame.message_type);
    assert(0x1234U == parser.frame.sequence);
    assert(sizeof(payload) == parser.frame.payload_length);
    assert(0 == memcmp(payload, parser.frame.payload, sizeof(payload)));

    static const uint8_t command_payload[] = {
        MCU_COMMAND_CAPTURE_START, 0x00U, 0x78U, 0x56U, 0x34U, 0x12U};
    assert(MCU_PROTOCOL_OK ==
           mcu_protocol_encode(MCU_MSG_COMMAND_REQUEST,
                               0x2201U,
                               command_payload,
                               sizeof(command_payload),
                               encoded,
                               sizeof(encoded),
                               &encoded_length));
    consume_frame(&parser,
                  encoded,
                  encoded_length,
                  MCU_PROTOCOL_PARSE_FRAME_READY);
    assert(MCU_MSG_COMMAND_REQUEST == parser.frame.message_type);
    assert(0x2201U == parser.frame.sequence);
    assert(0 == memcmp(command_payload,
                       parser.frame.payload,
                       sizeof(command_payload)));

    uint8_t corrupted[MCU_PROTOCOL_MAX_FRAME_SIZE] = {0U};
    (void)memcpy(corrupted, encoded, encoded_length);
    corrupted[encoded_length - 1U] ^= 0x80U;
    consume_frame(&parser,
                  corrupted,
                  encoded_length,
                  MCU_PROTOCOL_PARSE_CRC_ERROR);

    static const uint8_t invalid_length_header[] = {
        MCU_PROTOCOL_SOF_0, MCU_PROTOCOL_SOF_1,
        MCU_PROTOCOL_VERSION, MCU_MSG_PING_REQUEST,
        0x01U, 0x00U, 0xFFU, 0xFFU};
    consume_frame(&parser,
                  invalid_length_header,
                  sizeof(invalid_length_header),
                  MCU_PROTOCOL_PARSE_FORMAT_ERROR);

    static const uint8_t noise[] = {0x00U, 0xAAU, 0x00U, 0x7FU};
    for (size_t index = 0U; index < sizeof(noise); ++index)
    {
        assert(MCU_PROTOCOL_PARSE_NONE ==
               mcu_protocol_parser_consume(&parser, noise[index]));
    }
    consume_frame(&parser,
                  encoded,
                  encoded_length,
                  MCU_PROTOCOL_PARSE_FRAME_READY);

    /* 前一组AA55后紧跟下一帧AA55，非法Version中的AA必须用于重同步。 */
    static const uint8_t misleading_header[] = {
        MCU_PROTOCOL_SOF_0, MCU_PROTOCOL_SOF_1};
    for (size_t index = 0U; index < sizeof(misleading_header); ++index)
    {
        (void)mcu_protocol_parser_consume(&parser, misleading_header[index]);
    }
    consume_frame(&parser,
                  encoded,
                  encoded_length,
                  MCU_PROTOCOL_PARSE_FRAME_READY);

    puts("mcu_protocol_host_test PASS");
    return 0;
}
