/**
 * @file    lock_service.c
 * @brief   非阻塞开锁保持和自动上锁服务。
 * @details 服务只通过回调表达“锁定/开锁”逻辑状态；实际 GPIO、MOSFET 或继电器由 BSP
 *          适配，避免业务层直接耦合 12 V 门锁硬件。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-LOCK-001。
 * 关联设计：DES-LOCK-001。
 */
#include "lock_service.h"
#include "time_utils.h"

#include <stddef.h>

/** 防止错误配置成几乎不可见的脉冲；单位为毫秒。 */
#define LOCK_DURATION_MIN_MS 100U
/** 限制一次授权的最长保持时间，避免软件错误造成长期未上锁。 */
#define LOCK_DURATION_MAX_MS 30000U

/** 初始化时立即请求安全锁定输出，再允许任何认证流程工作。 */
void LockService_Init(LockService *service,
                      LockServiceOutputFn output,
                      void *outputContext)
{
  if (service == NULL)
  {
    return;
  }
  service->output = output;
  service->outputContext = outputContext;
  service->unlockStartedAt = 0U;
  service->unlockDurationMs = 0U;
  service->unlocked = false;
  if (service->output != NULL)
  {
    service->output(false, service->outputContext);
  }
}

/**
 * @brief 请求一次限时开锁。
 * @details 不延时；调用方必须在主循环持续调用 LockService_Task() 才能自动上锁。
 */
bool LockService_RequestUnlock(LockService *service,
                               uint32_t now,
                               uint32_t durationMs)
{
  if ((service == NULL) ||
      (durationMs < LOCK_DURATION_MIN_MS) ||
      (durationMs > LOCK_DURATION_MAX_MS))
  {
    return false;
  }
  service->unlockStartedAt = now;
  service->unlockDurationMs = durationMs;
  service->unlocked = true;
  if (service->output != NULL)
  {
    service->output(true, service->outputContext);
  }
  return true;
}

/** 用于超时、故障或初始化路径的立即上锁，重复调用保持幂等。 */
void LockService_ForceLock(LockService *service)
{
  if (service == NULL)
  {
    return;
  }
  service->unlocked = false;
  service->unlockDurationMs = 0U;
  if (service->output != NULL)
  {
    service->output(false, service->outputContext);
  }
}

/** 到达保持时长时自动上锁，比较函数处理系统节拍回绕。 */
void LockService_Task(LockService *service, uint32_t now)
{
  if ((service != NULL) && service->unlocked &&
      TimeUtils_Elapsed(now, service->unlockStartedAt, service->unlockDurationMs))
  {
    LockService_ForceLock(service);
  }
}

/** 查询逻辑开锁状态，供应用层决定 OLED 状态显示。 */
bool LockService_IsUnlocked(const LockService *service)
{
  return (service != NULL) && service->unlocked;
}
