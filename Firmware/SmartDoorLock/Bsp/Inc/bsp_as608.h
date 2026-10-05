/**
 * @file    bsp_as608.h
 * @brief   AS608 UART硬件驱动与非阻塞命令接口。
 * @details USART2中断只入队，所有协议解析和业务状态转换均在主循环任务中完成。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-006。
 * 关联设计：DES-FINGER-001。
 */
#ifndef BSP_AS608_H
#define BSP_AS608_H

#include <stdbool.h>
#include <stdint.h>

/** 当前命令的完成原因。 */
typedef enum
{
  AS608_RESULT_ACK = 0,
  AS608_RESULT_TIMEOUT,
  AS608_RESULT_PROTOCOL_ERROR
} BspAs608ResultType;

/** 已完成命令的结果；仅由BspAs608_TakeResult()输出。 */
typedef struct
{
  BspAs608ResultType type; /**< 应答、超时或协议错误。 */
  uint8_t command; /**< 本次完成的AS608命令码。 */
  uint8_t confirmation; /**< 模块确认码；无有效应答时为0xFF。 */
  uint16_t templateId; /**< 搜索成功时返回的模板ID。 */
  uint16_t score; /**< 搜索成功时返回的匹配得分。 */
} BspAs608Result;

/** @brief 初始化静态接收队列并启动USART2单字节中断接收。 */
void BspAs608_Init(void);
/** @brief 消费有限数量的接收字节并执行2秒命令超时判断。 */
void BspAs608_Task(uint32_t now);
/** @brief 查询是否有命令正在等待AS608应答。 */
bool BspAs608_IsBusy(void);
/** @brief 取走一次完成结果；无结果时返回false。 */
bool BspAs608_TakeResult(BspAs608Result *result);
/** @brief 使用指定32位口令验证模块；V1默认口令由应用层集中定义。 */
bool BspAs608_StartVerifyPassword(uint32_t password, uint32_t now);
/** @brief 请求采集图像；无手指通常由模块确认码0x02表示。 */
bool BspAs608_StartGetImage(uint32_t now);
/** @brief 将当前图像转换为bufferId(1或2)中的特征。 */
bool BspAs608_StartImageToChar(uint8_t bufferId, uint32_t now);
/** @brief 在0～299号模板中高速搜索指定特征缓冲区。 */
bool BspAs608_StartSearch(uint8_t bufferId, uint32_t now);
/** @brief 合并两次采集的特征，生成可保存模板。 */
bool BspAs608_StartCreateModel(uint32_t now);
/** @brief 将特征缓冲区保存到0～299号AS608内部模板位。 */
bool BspAs608_StartStore(uint8_t bufferId, uint16_t templateId, uint32_t now);
/** @brief 删除一个模板位；应用层负责同步删除EEPROM映射。 */
bool BspAs608_StartDelete(uint16_t templateId, uint32_t now);

#endif
