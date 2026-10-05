/**
 * @file    as608_protocol.c
 * @brief   AS608 串口数据包编码与应答解析。
 * @details 本文件不操作 UART；字节流由 BSP 层投递，协议层只完成有界解析、长度
 *          校验和校验和验证，避免异常输入越界。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FINGER-001。
 * 关联设计：DES-FINGER-001。
 */
#include "as608_protocol.h"

#include <string.h>

/** 包头、地址、包标识和长度字段在数据区前的固定字节数。 */
#define AS608_PREFIX_LENGTH 9U
/** 数据区至少应包含确认码和 2 字节校验和。 */
#define AS608_MIN_DATA_LENGTH 3U

/** 重置解析器，在丢包、校验失败或开始下一帧前调用。 */
void As608Protocol_Init(As608Parser *parser)
{
  if (parser != NULL)
  {
    memset(parser, 0, sizeof(*parser));
  }
}

/**
 * @brief 编码一帧 AS608 命令包。
 * @details 在写入前先验证输出容量和参数指针，返回 0 表示未向不完整缓冲区写入有效帧。
 */
size_t As608Protocol_EncodeCommand(uint8_t command,
                                  const uint8_t *parameters,
                                  uint8_t parameterLength,
                                  uint8_t *output,
                                  size_t outputCapacity)
{
  uint16_t dataLength = (uint16_t)parameterLength + 3U;
  size_t totalLength = AS608_PREFIX_LENGTH + dataLength;
  uint16_t checksum;
  uint8_t index;

  if ((output == NULL) || ((parameterLength > 0U) && (parameters == NULL)) ||
      (totalLength > outputCapacity) || (totalLength > AS608_PROTOCOL_PACKET_MAX))
  {
    return 0U;
  }
  output[0] = 0xEFU;
  output[1] = 0x01U;
  output[2] = 0xFFU;
  output[3] = 0xFFU;
  output[4] = 0xFFU;
  output[5] = 0xFFU;
  output[6] = 0x01U;
  output[7] = (uint8_t)(dataLength >> 8U);
  output[8] = (uint8_t)dataLength;
  output[9] = command;
  for (index = 0U; index < parameterLength; ++index)
  {
    output[10U + index] = parameters[index];
  }
  checksum = (uint16_t)(output[6] + output[7] + output[8] + command);
  for (index = 0U; index < parameterLength; ++index)
  {
    checksum = (uint16_t)(checksum + parameters[index]);
  }
  output[10U + parameterLength] = (uint8_t)(checksum >> 8U);
  output[11U + parameterLength] = (uint8_t)checksum;
  return totalLength;
}

/**
 * @brief 向流式解析器输入一个串口字节。
 * @details 仅同步帧头并记录预期长度；超过固定缓冲区或非法长度立即复位，抗噪声输入。
 */
As608ParseStatus As608Protocol_Feed(As608Parser *parser, uint8_t byte)
{
  uint16_t dataLength;
  if (parser == NULL)
  {
    return AS608_PARSE_ERROR;
  }
  if ((parser->length == 0U) && (byte != 0xEFU))
  {
    return AS608_PARSE_NONE;
  }
  if ((parser->length == 1U) && (byte != 0x01U))
  {
    parser->length = (byte == 0xEFU) ? 1U : 0U;
    return AS608_PARSE_NONE;
  }
  if (parser->length >= AS608_PROTOCOL_PACKET_MAX)
  {
    As608Protocol_Init(parser);
    return AS608_PARSE_ERROR;
  }
  parser->data[parser->length] = byte;
  ++parser->length;
  if (parser->length == AS608_PREFIX_LENGTH)
  {
    dataLength = (uint16_t)(((uint16_t)parser->data[7] << 8U) | parser->data[8]);
    if ((dataLength < AS608_MIN_DATA_LENGTH) ||
        ((uint16_t)AS608_PREFIX_LENGTH + dataLength >
         (uint16_t)AS608_PROTOCOL_PACKET_MAX))
    {
      As608Protocol_Init(parser);
      return AS608_PARSE_ERROR;
    }
    parser->expectedLength = (uint8_t)(AS608_PREFIX_LENGTH + dataLength);
  }
  if ((parser->expectedLength != 0U) && (parser->length == parser->expectedLength))
  {
    return AS608_PARSE_READY;
  }
  return AS608_PARSE_NONE;
}

/**
 * @brief 校验并取出完整确认包。
 * @details 校验和覆盖包标识至有效载荷；无论成功或失败都会复位解析器，避免旧帧被重复消费。
 */
bool As608Protocol_TakeAck(As608Parser *parser, As608Ack *ack)
{
  uint16_t calculated = 0U;
  uint16_t received;
  uint8_t index;
  uint8_t payloadLength;
  bool valid;

  if ((parser == NULL) || (ack == NULL) || (parser->expectedLength == 0U) ||
      (parser->length != parser->expectedLength))
  {
    return false;
  }
  for (index = 6U; index < (uint8_t)(parser->length - 2U); ++index)
  {
    calculated = (uint16_t)(calculated + parser->data[index]);
  }
  received = (uint16_t)(((uint16_t)parser->data[parser->length - 2U] << 8U) |
                        parser->data[parser->length - 1U]);
  payloadLength = (uint8_t)(parser->length - 12U);
  valid = (parser->data[2] == 0xFFU) && (parser->data[3] == 0xFFU) &&
          (parser->data[4] == 0xFFU) && (parser->data[5] == 0xFFU) &&
          (parser->data[6] == 0x07U) && (calculated == received) &&
          (payloadLength <= AS608_PROTOCOL_PARAM_MAX);
  if (valid)
  {
    ack->confirmation = parser->data[9];
    ack->parameterLength = payloadLength;
    if (payloadLength > 0U)
    {
      memcpy(ack->parameters, &parser->data[10], payloadLength);
    }
  }
  As608Protocol_Init(parser);
  return valid;
}
