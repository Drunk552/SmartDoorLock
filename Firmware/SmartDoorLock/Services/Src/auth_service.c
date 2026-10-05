/**
 * @file    auth_service.c
 * @brief   连续认证失败计数和临时锁定策略。
 * @details 所有认证入口共用本服务，避免密码、卡或指纹入口绕开锁定限制。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-SEC-001。
 * 关联设计：DES-AUTH-001。
 */
#include "auth_service.h"

#include "time_utils.h"

#include <stddef.h>

/** 初始化计数器并将配置中的秒数转换为内部毫秒单位。 */
void AuthService_Init(AuthService *service, uint8_t failureLimit, uint16_t lockoutSeconds)
{
  if (service == NULL)
  {
    return;
  }
  service->lockedAt = 0U;
  service->lockoutMs = (uint32_t)lockoutSeconds * 1000U;
  service->failureCount = 0U;
  service->failureLimit = failureLimit;
  service->locked = false;
}

/** 到期后解除临时锁定并清零失败次数；时间比较可跨 HAL_GetTick() 回绕。 */
void AuthService_Task(AuthService *service, uint32_t now)
{
  if ((service != NULL) && service->locked &&
      TimeUtils_Elapsed(now, service->lockedAt, service->lockoutMs))
  {
    service->locked = false;
    service->failureCount = 0U;
  }
}

/** 返回当前是否允许开始一次新的认证。 */
bool AuthService_CanAttempt(const AuthService *service)
{
  return (service != NULL) && !service->locked;
}

/** 成功认证清零累计失败次数，但不允许绕过仍处于锁定中的状态。 */
void AuthService_RecordSuccess(AuthService *service)
{
  if ((service != NULL) && !service->locked)
  {
    service->failureCount = 0U;
  }
}

/**
 * @brief 记录一次失败并在达到阈值时开始锁定。
 * @details 计数饱和于 uint8_t 最大值，避免异常重复调用发生回绕而降低安全性。
 */
bool AuthService_RecordFailure(AuthService *service, uint32_t now)
{
  if ((service == NULL) || service->locked)
  {
    return service != NULL && service->locked;
  }
  if (service->failureCount < UINT8_MAX)
  {
    ++service->failureCount;
  }
  if ((service->failureLimit > 0U) && (service->failureCount >= service->failureLimit))
  {
    service->locked = true;
    service->lockedAt = now;
  }
  return service->locked;
}

/** 返回锁定剩余时间；仅供界面显示，不作为额外的认证状态来源。 */
uint32_t AuthService_RemainingMs(const AuthService *service, uint32_t now)
{
  uint32_t elapsed;
  if ((service == NULL) || !service->locked)
  {
    return 0U;
  }
  elapsed = now - service->lockedAt;
  return (elapsed >= service->lockoutMs) ? 0U : service->lockoutMs - elapsed;
}

/** 返回当前失败次数，空指针按零处理以方便故障显示。 */
uint8_t AuthService_FailureCount(const AuthService *service)
{
  return (service == NULL) ? 0U : service->failureCount;
}
