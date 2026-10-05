/**
 * @file    credential_service.h
 * @brief   RFID UID与AS608模板ID的授权映射服务。
 * @details 只管理授权记录，不决定是否开锁；卡片和指纹认证结果必须回到应用层统一处理。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-005、REQ-FUN-006。
 * 关联设计：DES-CREDENTIAL-001。
 */
#ifndef CREDENTIAL_SERVICE_H
#define CREDENTIAL_SERVICE_H

#include "storage_service.h"

#include <stdbool.h>
#include <stdint.h>

#define CREDENTIAL_CARD_LIMIT 8U /**< AT24C02 V1可保存的4字节UID数量。 */
#define CREDENTIAL_FINGER_LIMIT 8U /**< AT24C02 V1可保存的模板ID数量。 */

/** 凭据表操作结果；I/O错误时内存表不得更新。 */
typedef enum
{
  CREDENTIAL_OK = 0,
  CREDENTIAL_EXISTS,
  CREDENTIAL_NOT_FOUND,
  CREDENTIAL_FULL,
  CREDENTIAL_INVALID,
  CREDENTIAL_IO_ERROR
} CredentialResult;

/** EEPROM中授权表的运行时缓存。 */
typedef struct
{
  uint8_t cardUid[CREDENTIAL_CARD_LIMIT][4]; /**< 仅支持4字节UID的V1记录。 */
  bool cardUsed[CREDENTIAL_CARD_LIMIT]; /**< 对应卡槽是否已授权。 */
  uint16_t fingerId[CREDENTIAL_FINGER_LIMIT]; /**< AS608模板ID，范围0～299。 */
  bool fingerUsed[CREDENTIAL_FINGER_LIMIT]; /**< 对应指纹映射是否有效。 */
} CredentialService;

/** @brief 清空RAM凭据缓存，不修改EEPROM。 */
void CredentialService_Init(CredentialService *service);
/** @brief 从EEPROM加载所有卡片和指纹映射；单条CRC失败时忽略该记录。 */
bool CredentialService_Load(CredentialService *service, const StorageBackend *backend);
/** @brief 查询4字节UID是否在授权表中。 */
bool CredentialService_IsCardAuthorized(const CredentialService *service, const uint8_t uid[4]);
/** @brief 新增一张卡并持久化，检查重复与容量上限。 */
CredentialResult CredentialService_AddCard(CredentialService *service,
                                           const StorageBackend *backend,
                                           const uint8_t uid[4]);
/** @brief 删除指定卡片的授权记录。 */
CredentialResult CredentialService_RemoveCard(CredentialService *service,
                                              const StorageBackend *backend,
                                              const uint8_t uid[4]);
/** @brief 查询AS608模板ID是否被授权。 */
bool CredentialService_IsFingerAuthorized(const CredentialService *service, uint16_t templateId);
/** @brief 保存模板ID映射；模板内容仍由AS608内部Flash保存。 */
CredentialResult CredentialService_AddFinger(CredentialService *service,
                                             const StorageBackend *backend,
                                             uint16_t templateId);
/** @brief 删除模板ID映射，不直接发送AS608删除命令。 */
CredentialResult CredentialService_RemoveFinger(CredentialService *service,
                                                const StorageBackend *backend,
                                                uint16_t templateId);

#endif
