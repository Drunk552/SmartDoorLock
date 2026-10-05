/**
 * @file    lock_service.h
 * @brief   非阻塞门锁模拟输出服务。
 * @details 服务只向注入的输出回调发出开/关请求，绝不直接驱动真实12V电磁锁。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-003、REQ-SAF-001。
 * 关联设计：DES-LOCK-001。
 */
#ifndef LOCK_SERVICE_H
#define LOCK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 由板级层实现的安全输出回调；unlocked=true仅代表逻辑开锁请求。 */
typedef void (*LockServiceOutputFn)(bool unlocked, void *context);

/** 门锁状态上下文，仅允许lock_service.c修改。 */
typedef struct
{
  LockServiceOutputFn output; /**< 板级输出回调，可为NULL。 */
  void *outputContext; /**< 传给输出回调的私有上下文。 */
  uint32_t unlockStartedAt; /**< 最近一次开锁起点，单位ms。 */
  uint32_t unlockDurationMs; /**< 自动上锁时长，范围100～30000ms。 */
  bool unlocked; /**< 当前逻辑状态；不等同于真实锁机械状态。 */
} LockService;

/** @brief 初始化服务并立即请求安全上锁状态。 */
void LockService_Init(LockService *service,
                      LockServiceOutputFn output,
                      void *outputContext);
/** @brief 请求非阻塞开锁；成功后必须由LockService_Task()按时自动上锁。 */
bool LockService_RequestUnlock(LockService *service,
                               uint32_t now,
                               uint32_t durationMs);
/** @brief 无条件撤销开锁请求，用于启动安全状态或故障恢复。 */
void LockService_ForceLock(LockService *service);
/** @brief 检查开锁时长是否到期；不得在中断中调用。 */
void LockService_Task(LockService *service, uint32_t now);
/** @brief 获取逻辑开锁状态。 */
bool LockService_IsUnlocked(const LockService *service);

#endif
