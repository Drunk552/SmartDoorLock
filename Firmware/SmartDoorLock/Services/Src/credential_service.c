/**
 * @file    credential_service.c
 * @brief   RFID UID 和指纹模板授权记录服务。
 * @details 内存缓存只在 EEPROM 写入成功后更新；每条短记录带 CRC，可在重启时忽略
 *          损坏项而继续使用其他有效凭据。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-CARD-001、REQ-FINGER-001、REQ-STORAGE-001。
 * 关联设计：DES-CREDENTIAL-001。
 */
#include "credential_service.h"
#include "crc.h"
#include "storage_layout.h"

#include <stddef.h>
#include <string.h>

/** 与存储布局及 AS608 管理菜单一致的合法模板 ID 上限。 */
#define FINGER_ID_MAX       299U

/** 清除 RAM 缓存；持久化记录需随后由 CredentialService_Load() 装载。 */
void CredentialService_Init(CredentialService *service)
{
  if (service != NULL)
  {
    memset(service, 0, sizeof(*service));
  }
}

/**
 * @brief 从 EEPROM 重建授权缓存。
 * @details 单条记录读失败会继续扫描其余记录并返回 false，让应用层保留故障日志证据。
 */
bool CredentialService_Load(CredentialService *service, const StorageBackend *backend)
{
  uint8_t index;
  bool ioOk = true;

  if ((service == NULL) || (backend == NULL) || (backend->read == NULL))
  {
    return false;
  }
  CredentialService_Init(service);
  for (index = 0U; index < CREDENTIAL_CARD_LIMIT; ++index)
  {
    uint8_t record[STORAGE_CARD_RECORD_SIZE];
    uint16_t address = (uint16_t)(STORAGE_CARD_BASE_ADDRESS +
                                  ((uint16_t)index * STORAGE_CARD_RECORD_SIZE));
    if (!backend->read(address, record, sizeof(record), backend->context))
    {
      ioOk = false;
    }
    else if ((record[0] == 1U) && (record[5] == Crc8(record, 5U)))
    {
      service->cardUsed[index] = true;
      memcpy(service->cardUid[index], &record[1], 4U);
    }
  }
  for (index = 0U; index < CREDENTIAL_FINGER_LIMIT; ++index)
  {
    uint8_t record[STORAGE_FINGER_RECORD_SIZE];
    uint16_t address = (uint16_t)(STORAGE_FINGER_BASE_ADDRESS +
                                  ((uint16_t)index * STORAGE_FINGER_RECORD_SIZE));
    if (!backend->read(address, record, sizeof(record), backend->context))
    {
      ioOk = false;
    }
    else if ((record[0] == 1U) && (record[3] == Crc8(record, 3U)))
    {
      uint16_t id = (uint16_t)((uint16_t)record[1] | ((uint16_t)record[2] << 8U));
      if (id <= FINGER_ID_MAX)
      {
        service->fingerUsed[index] = true;
        service->fingerId[index] = id;
      }
    }
  }
  return ioOk;
}

/** 在固定上限表中精确比较 4 字节 UID；本项目 RC522 接口只授权该 UID 形式。 */
bool CredentialService_IsCardAuthorized(const CredentialService *service, const uint8_t uid[4])
{
  uint8_t index;
  if ((service == NULL) || (uid == NULL))
  {
    return false;
  }
  for (index = 0U; index < CREDENTIAL_CARD_LIMIT; ++index)
  {
    if (service->cardUsed[index] && (memcmp(service->cardUid[index], uid, 4U) == 0))
    {
      return true;
    }
  }
  return false;
}

/** 先写 EEPROM 并校验驱动返回值，再修改缓存，防止掉电造成“内存已授权”的假象。 */
CredentialResult CredentialService_AddCard(CredentialService *service,
                                           const StorageBackend *backend,
                                           const uint8_t uid[4])
{
  uint8_t index;
  uint8_t freeIndex = CREDENTIAL_CARD_LIMIT;
  uint8_t record[STORAGE_CARD_RECORD_SIZE];
  uint16_t address;

  if ((service == NULL) || (backend == NULL) || (backend->write == NULL) || (uid == NULL))
  {
    return CREDENTIAL_INVALID;
  }
  for (index = 0U; index < CREDENTIAL_CARD_LIMIT; ++index)
  {
    if (service->cardUsed[index] && (memcmp(service->cardUid[index], uid, 4U) == 0))
    {
      return CREDENTIAL_EXISTS;
    }
    if (!service->cardUsed[index] && (freeIndex == CREDENTIAL_CARD_LIMIT))
    {
      freeIndex = index;
    }
  }
  if (freeIndex == CREDENTIAL_CARD_LIMIT)
  {
    return CREDENTIAL_FULL;
  }
  record[0] = 1U;
  memcpy(&record[1], uid, 4U);
  record[5] = Crc8(record, 5U);
  address = (uint16_t)(STORAGE_CARD_BASE_ADDRESS +
                       ((uint16_t)freeIndex * STORAGE_CARD_RECORD_SIZE));
  if (!backend->write(address, record, sizeof(record), backend->context))
  {
    return CREDENTIAL_IO_ERROR;
  }
  service->cardUsed[freeIndex] = true;
  memcpy(service->cardUid[freeIndex], uid, 4U);
  return CREDENTIAL_OK;
}

/** 删除卡记录时先将持久化槽清零；写失败时保持 RAM 缓存不变。 */
CredentialResult CredentialService_RemoveCard(CredentialService *service,
                                              const StorageBackend *backend,
                                              const uint8_t uid[4])
{
  uint8_t index;
  uint8_t empty[STORAGE_CARD_RECORD_SIZE] = {0};
  if ((service == NULL) || (backend == NULL) || (backend->write == NULL) || (uid == NULL))
  {
    return CREDENTIAL_INVALID;
  }
  for (index = 0U; index < CREDENTIAL_CARD_LIMIT; ++index)
  {
    if (service->cardUsed[index] && (memcmp(service->cardUid[index], uid, 4U) == 0))
    {
      uint16_t address = (uint16_t)(STORAGE_CARD_BASE_ADDRESS +
                                    ((uint16_t)index * STORAGE_CARD_RECORD_SIZE));
      if (!backend->write(address, empty, sizeof(empty), backend->context))
      {
        return CREDENTIAL_IO_ERROR;
      }
      service->cardUsed[index] = false;
      memset(service->cardUid[index], 0, 4U);
      return CREDENTIAL_OK;
    }
  }
  return CREDENTIAL_NOT_FOUND;
}

/** 只接受同时存在于 AS608 搜索结果和本地授权表中的模板 ID。 */
bool CredentialService_IsFingerAuthorized(const CredentialService *service, uint16_t templateId)
{
  uint8_t index;
  if (service == NULL)
  {
    return false;
  }
  for (index = 0U; index < CREDENTIAL_FINGER_LIMIT; ++index)
  {
    if (service->fingerUsed[index] && (service->fingerId[index] == templateId))
    {
      return true;
    }
  }
  return false;
}

/** 保存已成功录入 AS608 的模板 ID；调用者负责在此前完成模块录入流程。 */
CredentialResult CredentialService_AddFinger(CredentialService *service,
                                             const StorageBackend *backend,
                                             uint16_t templateId)
{
  uint8_t index;
  uint8_t freeIndex = CREDENTIAL_FINGER_LIMIT;
  uint8_t record[STORAGE_FINGER_RECORD_SIZE];
  uint16_t address;
  if ((service == NULL) || (backend == NULL) || (backend->write == NULL) ||
      (templateId > FINGER_ID_MAX))
  {
    return CREDENTIAL_INVALID;
  }
  for (index = 0U; index < CREDENTIAL_FINGER_LIMIT; ++index)
  {
    if (service->fingerUsed[index] && (service->fingerId[index] == templateId))
    {
      return CREDENTIAL_EXISTS;
    }
    if (!service->fingerUsed[index] && (freeIndex == CREDENTIAL_FINGER_LIMIT))
    {
      freeIndex = index;
    }
  }
  if (freeIndex == CREDENTIAL_FINGER_LIMIT)
  {
    return CREDENTIAL_FULL;
  }
  record[0] = 1U;
  record[1] = (uint8_t)templateId;
  record[2] = (uint8_t)(templateId >> 8U);
  record[3] = Crc8(record, 3U);
  address = (uint16_t)(STORAGE_FINGER_BASE_ADDRESS +
                       ((uint16_t)freeIndex * STORAGE_FINGER_RECORD_SIZE));
  if (!backend->write(address, record, sizeof(record), backend->context))
  {
    return CREDENTIAL_IO_ERROR;
  }
  service->fingerUsed[freeIndex] = true;
  service->fingerId[freeIndex] = templateId;
  return CREDENTIAL_OK;
}

/** 删除本地授权记录；应用层另行删除 AS608 模板并处理两侧失败回滚。 */
CredentialResult CredentialService_RemoveFinger(CredentialService *service,
                                                const StorageBackend *backend,
                                                uint16_t templateId)
{
  uint8_t index;
  uint8_t empty[STORAGE_FINGER_RECORD_SIZE] = {0};
  if ((service == NULL) || (backend == NULL) || (backend->write == NULL) ||
      (templateId > FINGER_ID_MAX))
  {
    return CREDENTIAL_INVALID;
  }
  for (index = 0U; index < CREDENTIAL_FINGER_LIMIT; ++index)
  {
    if (service->fingerUsed[index] && (service->fingerId[index] == templateId))
    {
      uint16_t address = (uint16_t)(STORAGE_FINGER_BASE_ADDRESS +
                                    ((uint16_t)index * STORAGE_FINGER_RECORD_SIZE));
      if (!backend->write(address, empty, sizeof(empty), backend->context))
      {
        return CREDENTIAL_IO_ERROR;
      }
      service->fingerUsed[index] = false;
      service->fingerId[index] = 0U;
      return CREDENTIAL_OK;
    }
  }
  return CREDENTIAL_NOT_FOUND;
}
