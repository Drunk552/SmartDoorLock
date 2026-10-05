/**
 * @file    bsp_log.h
 * @brief   USART1有限长度调试日志输出。
 * @details 日志失败不得影响认证与上锁状态机；最长发送160字节且有HAL超时。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-001。
 * 关联设计：DES-LOG-001。
 */
#ifndef BSP_LOG_H
#define BSP_LOG_H

/** @brief 输出以NUL结束的ASCII日志；NULL或超长文本会被安全忽略/截断。 */
void BspLog_Write(const char *message);

#endif
