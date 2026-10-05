/**
 * @file    auth_service.h
 * @brief   多认证方式共享的失败计数和临时锁定服务。
 * @details 密码、RFID和指纹必须共同使用本服务，避免切换认证方式绕过锁定策略。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-SEC-001。
 * 关联设计：DES-AUTH-001。
 */
#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/** 连续失败保护上下文，仅允许auth_service.c修改。 */
typedef struct
{
  uint32_t lockedAt; /**< 锁定起点，单位ms。 */
  uint32_t lockoutMs; /**< 锁定持续时间，单位ms。 */
  uint8_t failureCount; /**< 当前连续失败次数，成功后清零。 */
  uint8_t failureLimit; /**< 触发锁定的失败次数阈值。 */
  bool locked; /**< true时必须拒绝所有普通认证开锁请求。 */
} AuthService;

/** @brief 依据EEPROM配置初始化失败限制和锁定时长。 */
void AuthService_Init(AuthService *service, uint8_t failureLimit, uint16_t lockoutSeconds);
/** @brief 到期自动解除临时锁定；使用回绕安全的毫秒时基。 */
void AuthService_Task(AuthService *service, uint32_t now);
/** @brief 查询当前是否允许开始新的认证尝试。 */
bool AuthService_CanAttempt(const AuthService *service);
/** @brief 记录认证成功并清零连续失败次数。 */
void AuthService_RecordSuccess(AuthService *service);
/** @brief 记录一次失败；返回true表示本次失败已触发或处于锁定。 */
bool AuthService_RecordFailure(AuthService *service, uint32_t now);
/** @brief 获取剩余锁定时间，单位ms；未锁定时返回0。 */
uint32_t AuthService_RemainingMs(const AuthService *service, uint32_t now);
/** @brief 获取连续失败次数，供OLED或日志展示。 */
uint8_t AuthService_FailureCount(const AuthService *service);

#endif
