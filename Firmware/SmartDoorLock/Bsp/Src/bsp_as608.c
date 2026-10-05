/**
 * @file    bsp_as608.c
 * @brief   AS608 的 USART2 异步传输适配。
 * @details 中断只将收到字节放入环形队列；协议解析、超时和结果处理全部在主循环任务中
 *          完成，避免在 ISR 中执行耗时业务逻辑。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FINGER-001。
 * 关联设计：DES-FINGER-001。
 */
#include "bsp_as608.h"

#include "as608_protocol.h"
#include "time_utils.h"
#include "usart.h"

#include <stddef.h>
#include <string.h>

/** 单次命令帧发送的 HAL 阻塞上限，单位毫秒。 */
#define AS608_UART_TIMEOUT_MS 20U
/** 等待模块确认包的总上限，防止外设断线卡住业务状态机。 */
#define AS608_COMMAND_TIMEOUT_MS 2000U
/** 单次任务最多解析的字节数，保证 OLED、键盘等任务仍有运行机会。 */
#define AS608_RX_BUDGET 24U
/** 模块命令允许的模板编号范围为 0 至此值减一。 */
#define AS608_TEMPLATE_COUNT 300U
/** 2 的幂环形缓冲区大小，掩码索引无需除法。 */
#define AS608_RX_QUEUE_SIZE 64U
#define AS608_RX_QUEUE_MASK (AS608_RX_QUEUE_SIZE - 1U)

/** 以下状态仅由本 BSP 拥有；head/tail/overflow 会被中断和主循环共同访问。 */
static As608Parser s_parser;
static BspAs608Result s_result;
static uint32_t s_commandTick;
static uint8_t s_activeCommand;
static bool s_busy;
static bool s_resultReady;
static uint8_t s_rxQueue[AS608_RX_QUEUE_SIZE];
static uint8_t s_uartRxByte;
static volatile uint8_t s_rxHead;
static volatile uint8_t s_rxTail;
static volatile bool s_rxOverflow;

/**
 * @brief 发送一条命令并登记待处理状态。
 * @details 同一时刻只允许一个请求；发送失败不改变 busy 状态，使应用层可安全重试。
 */
static bool StartCommand(uint8_t command,
                         const uint8_t *parameters,
                         uint8_t parameterLength,
                         uint32_t now)
{
  uint8_t packet[AS608_PROTOCOL_PACKET_MAX];
  size_t length;
  if (s_busy || s_resultReady)
  {
    return false;
  }
  s_rxTail = s_rxHead;
  s_rxOverflow = false;
  length = As608Protocol_EncodeCommand(command,
                                      parameters,
                                      parameterLength,
                                      packet,
                                      sizeof(packet));
  if ((length == 0U) ||
      (HAL_UART_Transmit(&huart2, packet, (uint16_t)length, AS608_UART_TIMEOUT_MS) != HAL_OK))
  {
    return false;
  }
  As608Protocol_Init(&s_parser);
  s_activeCommand = command;
  s_commandTick = now;
  s_busy = true;
  return true;
}

/** 初始化解析、队列和 USART2 单字节接收中断。 */
void BspAs608_Init(void)
{
  As608Protocol_Init(&s_parser);
  memset(&s_result, 0, sizeof(s_result));
  s_busy = false;
  s_resultReady = false;
  s_rxHead = 0U;
  s_rxTail = 0U;
  s_rxOverflow = false;
  (void)HAL_UART_Receive_IT(&huart2, &s_uartRxByte, 1U);
}

/**
 * @brief 在主循环中有界消费串口队列并生成一次性结果。
 * @details 溢出、协议错误和超时都转为结果对象，调用方无需永久等待 busy 清除。
 */
void BspAs608_Task(uint32_t now)
{
  uint8_t budget = AS608_RX_BUDGET;
  if (!s_busy)
  {
    s_rxTail = s_rxHead;
    s_rxOverflow = false;
    return;
  }
  if (s_rxOverflow)
  {
    s_result.type = AS608_RESULT_PROTOCOL_ERROR;
    s_result.command = s_activeCommand;
    s_result.confirmation = 0xFFU;
    s_result.templateId = 0U;
    s_result.score = 0U;
    s_busy = false;
    s_resultReady = true;
    s_rxTail = s_rxHead;
    s_rxOverflow = false;
    As608Protocol_Init(&s_parser);
    return;
  }
  while ((budget > 0U) && (s_rxTail != s_rxHead))
  {
    uint8_t byte = s_rxQueue[s_rxTail];
    As608ParseStatus status = As608Protocol_Feed(&s_parser, byte);
    s_rxTail = (uint8_t)((s_rxTail + 1U) & AS608_RX_QUEUE_MASK);
    --budget;
    if (status == AS608_PARSE_READY)
    {
      As608Ack ack;
      s_result.type = AS608_RESULT_PROTOCOL_ERROR;
      s_result.command = s_activeCommand;
      s_result.confirmation = 0xFFU;
      s_result.templateId = 0U;
      s_result.score = 0U;
      if (As608Protocol_TakeAck(&s_parser, &ack))
      {
        s_result.type = AS608_RESULT_ACK;
        s_result.confirmation = ack.confirmation;
        if ((s_activeCommand == 0x1BU) && (ack.confirmation == 0U) &&
            (ack.parameterLength >= 4U))
        {
          s_result.templateId = (uint16_t)(((uint16_t)ack.parameters[0] << 8U) |
                                           ack.parameters[1]);
          s_result.score = (uint16_t)(((uint16_t)ack.parameters[2] << 8U) |
                                      ack.parameters[3]);
        }
      }
      s_busy = false;
      s_resultReady = true;
      return;
    }
    if (status == AS608_PARSE_ERROR)
    {
      s_result.type = AS608_RESULT_PROTOCOL_ERROR;
      s_result.command = s_activeCommand;
      s_result.confirmation = 0xFFU;
      s_result.templateId = 0U;
      s_result.score = 0U;
      s_busy = false;
      s_resultReady = true;
      return;
    }
  }
  if (TimeUtils_Elapsed(now, s_commandTick, AS608_COMMAND_TIMEOUT_MS))
  {
    s_result.type = AS608_RESULT_TIMEOUT;
    s_result.command = s_activeCommand;
    s_result.confirmation = 0xFFU;
    s_result.templateId = 0U;
    s_result.score = 0U;
    s_busy = false;
    s_resultReady = true;
    As608Protocol_Init(&s_parser);
  }
}

/**
 * @brief USART2 接收完成回调。
 * @details 偏离通用回调“仅 HAL 分发”的约定：HAL 只提供全局回调入口。此处严格限于
 *          入队和重新挂接 1 字节接收，禁止解析协议、刷新 OLED 或调用认证服务。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if ((uart != NULL) && (uart->Instance == USART2))
  {
    uint8_t next = (uint8_t)((s_rxHead + 1U) & AS608_RX_QUEUE_MASK);
    if (next == s_rxTail)
    {
      s_rxOverflow = true;
    }
    else
    {
      s_rxQueue[s_rxHead] = s_uartRxByte;
      s_rxHead = next;
    }
    (void)HAL_UART_Receive_IT(&huart2, &s_uartRxByte, 1U);
  }
}

/** USART2 错误只标记队列异常并重新接收，主循环负责生成可见的失败结果。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if ((uart != NULL) && (uart->Instance == USART2))
  {
    s_rxOverflow = true;
    __HAL_UART_CLEAR_OREFLAG(uart);
    (void)HAL_UART_Receive_IT(&huart2, &s_uartRxByte, 1U);
  }
}

/** 查询是否已有命令在等待确认包。 */
bool BspAs608_IsBusy(void)
{
  return s_busy;
}

/** 取走一次命令结果；读取后清除 ready 标志，避免重复处理。 */
bool BspAs608_TakeResult(BspAs608Result *result)
{
  if (!s_resultReady || (result == NULL))
  {
    return false;
  }
  *result = s_result;
  s_resultReady = false;
  return true;
}

/** 发起模块认证命令；认证参数由上层配置管理，禁止在日志或注释中暴露。 */
bool BspAs608_StartVerifyPassword(uint32_t password, uint32_t now)
{
  uint8_t parameters[4] = {(uint8_t)(password >> 24U),
                           (uint8_t)(password >> 16U),
                           (uint8_t)(password >> 8U),
                           (uint8_t)password};
  return StartCommand(0x13U, parameters, (uint8_t)sizeof(parameters), now);
}

/** 发起指纹图像采集。 */
bool BspAs608_StartGetImage(uint32_t now)
{
  return StartCommand(0x01U, NULL, 0U, now);
}

/** 将图像转为 CharBuffer1 或 CharBuffer2 特征，其他缓冲区号一律拒绝。 */
bool BspAs608_StartImageToChar(uint8_t bufferId, uint32_t now)
{
  return ((bufferId == 1U) || (bufferId == 2U)) &&
         StartCommand(0x02U, &bufferId, 1U, now);
}

/** 在模块完整模板范围搜索指定特征缓冲区。 */
bool BspAs608_StartSearch(uint8_t bufferId, uint32_t now)
{
  uint8_t parameters[5] = {bufferId, 0U, 0U,
                           (uint8_t)(AS608_TEMPLATE_COUNT >> 8U),
                           (uint8_t)AS608_TEMPLATE_COUNT};
  return ((bufferId == 1U) || (bufferId == 2U)) &&
         StartCommand(0x1BU, parameters, (uint8_t)sizeof(parameters), now);
}

/** 合并两个特征缓冲区；仅在双次采集均成功后由应用层调用。 */
bool BspAs608_StartCreateModel(uint32_t now)
{
  return StartCommand(0x05U, NULL, 0U, now);
}

/** 将模型保存到有界模板 ID，防止错误参数覆盖模块的未知区域。 */
bool BspAs608_StartStore(uint8_t bufferId, uint16_t templateId, uint32_t now)
{
  uint8_t parameters[3] = {bufferId, (uint8_t)(templateId >> 8U), (uint8_t)templateId};
  return ((bufferId == 1U) || (bufferId == 2U)) && (templateId < AS608_TEMPLATE_COUNT) &&
         StartCommand(0x06U, parameters, (uint8_t)sizeof(parameters), now);
}

/** 删除单个有界模板 ID；数量参数固定为 1。 */
bool BspAs608_StartDelete(uint16_t templateId, uint32_t now)
{
  uint8_t parameters[4] = {(uint8_t)(templateId >> 8U), (uint8_t)templateId, 0U, 1U};
  return (templateId < AS608_TEMPLATE_COUNT) &&
         StartCommand(0x0CU, parameters, (uint8_t)sizeof(parameters), now);
}
