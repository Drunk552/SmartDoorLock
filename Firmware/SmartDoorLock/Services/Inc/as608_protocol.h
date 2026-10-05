/**
 * @file    as608_protocol.h
 * @brief   AS608串口数据包编码和应答解析。
 * @details 仅处理协议字节流，不访问UART；UART缓冲与超时由bsp_as608负责。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-006。
 * 关联设计：DES-FINGER-001。
 */
#ifndef AS608_PROTOCOL_H
#define AS608_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AS608_PROTOCOL_PACKET_MAX 32U /**< V1命令/应答缓冲区上限，单位字节。 */
#define AS608_PROTOCOL_PARAM_MAX 16U /**< 应答确认码后的可保留参数上限。 */

/** 字节流解析结果；READY后必须调用As608Protocol_TakeAck()取走结果。 */
typedef enum
{
  AS608_PARSE_NONE = 0,
  AS608_PARSE_READY,
  AS608_PARSE_ERROR
} As608ParseStatus;

/** 逐字节解析上下文，调用者负责在同一命令内持续保存。 */
typedef struct
{
  uint8_t data[AS608_PROTOCOL_PACKET_MAX]; /**< 当前接收的数据包。 */
  uint8_t length; /**< 已接收字节数。 */
  uint8_t expectedLength; /**< 由协议长度字段得到的总字节数，0表示未知。 */
} As608Parser;

/** 经地址、包标识、长度和校验和验证后的应答内容。 */
typedef struct
{
  uint8_t confirmation; /**< AS608确认码，0表示模块执行成功。 */
  uint8_t parameters[AS608_PROTOCOL_PARAM_MAX]; /**< 可选返回参数。 */
  uint8_t parameterLength; /**< parameters的有效字节数。 */
} As608Ack;

/** @brief 清空解析上下文，可用于启动新命令或处理错误后恢复同步。 */
void As608Protocol_Init(As608Parser *parser);
/** @brief 编码地址为0xFFFFFFFF的AS608命令包；返回0表示参数或输出缓冲区非法。 */
size_t As608Protocol_EncodeCommand(uint8_t command,
                                  const uint8_t *parameters,
                                  uint8_t parameterLength,
                                  uint8_t *output,
                                  size_t outputCapacity);
/** @brief 输入一个UART字节；完成整包时返回AS608_PARSE_READY。 */
As608ParseStatus As608Protocol_Feed(As608Parser *parser, uint8_t byte);
/** @brief 验证完整应答包的地址、类型、长度和校验和后输出确认码与参数。 */
bool As608Protocol_TakeAck(As608Parser *parser, As608Ack *ack);

#endif
