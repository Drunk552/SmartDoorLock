/**
 * @file    bsp_log.c
 * @brief   USART1 有界调试日志输出。
 * @details 日志仅用于台架诊断，单条长度和发送时间均受限，不能成为认证或门锁状态机的
 *          无限阻塞点。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-DEBUG-001。
 * 关联设计：DES-LOG-001。
 */
#include "bsp_log.h"
#include "usart.h"

#include <stddef.h>
#include <stdint.h>

/** 防止错误的未终止字符串导致越界扫描或长时间 UART 占用。 */
#define BSP_LOG_MAX_LENGTH 160U
/** 单条同步调试日志的最长期限，单位毫秒。 */
#define BSP_LOG_TIMEOUT_MS 20U

/** 发送以 NUL 终止的短日志；超时或发送失败仅丢弃本条，不能影响门锁控制。 */
void BspLog_Write(const char *message)
{
  uint16_t length = 0U;

  if (message == NULL)
  {
    return;
  }

  while ((length < BSP_LOG_MAX_LENGTH) && (message[length] != '\0'))
  {
    ++length;
  }

  if (length > 0U)
  {
    (void)HAL_UART_Transmit(&huart1,
                           (const uint8_t *)message,
                           length,
                           BSP_LOG_TIMEOUT_MS);
  }
}
