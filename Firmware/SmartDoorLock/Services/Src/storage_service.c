/**
 * @file    storage_service.c
 * @brief   AT24C02 配置双副本、版本和 CRC 管理。
 * @details 两个固定长度记录采用“写较旧副本、读回校验”的策略；掉电或单副本损坏时可
 *          从另一有效副本恢复。物理 I2C 读写由调用方提供的后端完成。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-STORAGE-001。
 * 关联设计：DES-STORAGE-001。
 */
#include "storage_service.h"
#include "crc.h"
#include "storage_layout.h"

#include <stddef.h>
#include <string.h>

/** 序列化字段调整时必须递增，旧布局不会被误解为当前配置。 */
#define CONFIG_SCHEMA_VERSION 1U

/** 配置记录的魔数，用于区分未初始化或非本项目 EEPROM 内容。 */
static const uint8_t s_magic[4] = {'S','D','L','K'};

/** 以显式小端字节序写入，避免结构体填充和编译器 ABI 影响 EEPROM 布局。 */
static void WriteU16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
}

/** 同上，序列号固定为小端 32 位。 */
static void WriteU32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
  data[2] = (uint8_t)(value >> 16U);
  data[3] = (uint8_t)(value >> 24U);
}

/** 从 EEPROM 固定小端布局读取 16 位字段。 */
static uint16_t ReadU16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

/** 从 EEPROM 固定小端布局读取 32 位序列号。 */
static uint32_t ReadU32(const uint8_t *data)
{
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) |
         ((uint32_t)data[3] << 24U);
}

/** PIN 仅允许 4 至最大长度位数字，避免配置损坏后参与认证比较。 */
static bool PinIsValid(const uint8_t *pin, uint8_t length)
{
  uint8_t index;
  if ((length < 4U) || (length > SYSTEM_PIN_MAX_LENGTH))
  {
    return false;
  }
  for (index = 0U; index < length; ++index)
  {
    if ((pin[index] < (uint8_t)'0') || (pin[index] > (uint8_t)'9'))
    {
      return false;
    }
  }
  return true;
}

/** 验证所有范围；该函数同时保护默认值、加载值和写入值。 */
bool StorageService_ConfigIsValid(const SystemConfig *config)
{
  return (config != NULL) &&
         PinIsValid(config->userPin, config->userPinLength) &&
         PinIsValid(config->adminPin, config->adminPinLength) &&
         (config->failureLimit >= 1U) && (config->failureLimit <= 10U) &&
         (config->unlockSeconds >= 1U) && (config->unlockSeconds <= 30U) &&
         (config->lockoutSeconds >= 10U) && (config->lockoutSeconds <= 3600U);
}

/**
 * @brief 建立可启动的默认配置。
 * @details 默认值仅在两个副本均无效或后端不可用时使用；部署前应通过管理流程修改凭据。
 */
void StorageService_DefaultConfig(SystemConfig *config)
{
  static const uint8_t userPin[SYSTEM_PIN_MAX_LENGTH] = {'1','2','3','4','5','6'};
  static const uint8_t adminPin[SYSTEM_PIN_MAX_LENGTH] = {'0','0','0','0','0','0'};
  if (config == NULL)
  {
    return;
  }
  memset(config, 0, sizeof(*config));
  memcpy(config->userPin, userPin, sizeof(userPin));
  memcpy(config->adminPin, adminPin, sizeof(adminPin));
  config->userPinLength = SYSTEM_PIN_MAX_LENGTH;
  config->adminPinLength = SYSTEM_PIN_MAX_LENGTH;
  config->failureLimit = 5U;
  config->unlockSeconds = 5U;
  config->lockoutSeconds = 60U;
}

/** 按 storage_layout.h 的字段偏移编码记录，CRC 覆盖 CRC 字段之前的全部内容。 */
static void Encode(const SystemConfig *config, uint8_t record[STORAGE_CONFIG_RECORD_SIZE])
{
  uint16_t crc;
  memset(record, 0, STORAGE_CONFIG_RECORD_SIZE);
  memcpy(&record[0], s_magic, sizeof(s_magic));
  WriteU16(&record[4], CONFIG_SCHEMA_VERSION);
  WriteU16(&record[6], STORAGE_CONFIG_RECORD_SIZE);
  WriteU32(&record[8], config->sequence);
  memcpy(&record[12], config->userPin, SYSTEM_PIN_MAX_LENGTH);
  memcpy(&record[18], config->adminPin, SYSTEM_PIN_MAX_LENGTH);
  record[24] = config->userPinLength;
  record[25] = config->adminPinLength;
  record[26] = config->failureLimit;
  record[27] = config->unlockSeconds;
  WriteU16(&record[28], config->lockoutSeconds);
  crc = Crc16Ccitt(record, 30U);
  WriteU16(&record[30], crc);
}

/** 只有魔数、版本、长度、CRC 和业务范围均正确的记录才可成为运行配置。 */
static bool Decode(const uint8_t record[STORAGE_CONFIG_RECORD_SIZE], SystemConfig *config)
{
  if ((memcmp(record, s_magic, sizeof(s_magic)) != 0) ||
      (ReadU16(&record[4]) != CONFIG_SCHEMA_VERSION) ||
      (ReadU16(&record[6]) != STORAGE_CONFIG_RECORD_SIZE) ||
      (ReadU16(&record[30]) != Crc16Ccitt(record, 30U)))
  {
    return false;
  }
  memset(config, 0, sizeof(*config));
  config->sequence = ReadU32(&record[8]);
  memcpy(config->userPin, &record[12], SYSTEM_PIN_MAX_LENGTH);
  memcpy(config->adminPin, &record[18], SYSTEM_PIN_MAX_LENGTH);
  config->userPinLength = record[24];
  config->adminPinLength = record[25];
  config->failureLimit = record[26];
  config->unlockSeconds = record[27];
  config->lockoutSeconds = ReadU16(&record[28]);
  return StorageService_ConfigIsValid(config);
}

/**
 * @brief 从两个副本选择最新有效配置。
 * @details 序列号差值按有符号数比较，以容忍 32 位序列号自然回绕。
 */
StorageLoadResult StorageService_LoadConfig(const StorageBackend *backend,
                                            SystemConfig *config)
{
  uint8_t recordA[STORAGE_CONFIG_RECORD_SIZE];
  uint8_t recordB[STORAGE_CONFIG_RECORD_SIZE];
  SystemConfig configA = {0};
  SystemConfig configB = {0};
  bool validA;
  bool validB;

  if ((backend == NULL) || (backend->read == NULL) || (config == NULL))
  {
    if (config != NULL)
    {
      StorageService_DefaultConfig(config);
    }
    return STORAGE_LOAD_DEFAULT;
  }
  validA = backend->read(STORAGE_CONFIG_COPY_A_ADDRESS, recordA, sizeof(recordA), backend->context) &&
           Decode(recordA, &configA);
  validB = backend->read(STORAGE_CONFIG_COPY_B_ADDRESS, recordB, sizeof(recordB), backend->context) &&
           Decode(recordB, &configB);
  if (!validA && !validB)
  {
    StorageService_DefaultConfig(config);
    return STORAGE_LOAD_DEFAULT;
  }
  if (validA && (!validB || ((int32_t)(configA.sequence - configB.sequence) > 0)))
  {
    *config = configA;
    return STORAGE_LOAD_COPY_A;
  }
  *config = configB;
  return STORAGE_LOAD_COPY_B;
}

/**
 * @brief 安全保存配置并读回验证。
 * @details 优先覆盖无效副本，否则写入较旧副本；任何写后校验失败都会恢复 RAM 中的序列号。
 */
bool StorageService_SaveConfig(const StorageBackend *backend, SystemConfig *config)
{
  uint8_t recordA[STORAGE_CONFIG_RECORD_SIZE];
  uint8_t recordB[STORAGE_CONFIG_RECORD_SIZE];
  uint8_t encoded[STORAGE_CONFIG_RECORD_SIZE];
  uint8_t verify[STORAGE_CONFIG_RECORD_SIZE];
  SystemConfig configA = {0};
  SystemConfig configB = {0};
  SystemConfig verified;
  bool validA;
  bool validB;
  uint16_t target;

  if ((backend == NULL) || (backend->read == NULL) || (backend->write == NULL) ||
      !StorageService_ConfigIsValid(config))
  {
    return false;
  }
  validA = backend->read(STORAGE_CONFIG_COPY_A_ADDRESS, recordA, sizeof(recordA), backend->context) &&
           Decode(recordA, &configA);
  validB = backend->read(STORAGE_CONFIG_COPY_B_ADDRESS, recordB, sizeof(recordB), backend->context) &&
           Decode(recordB, &configB);
  if (!validA)
  {
    target = STORAGE_CONFIG_COPY_A_ADDRESS;
  }
  else if (!validB)
  {
    target = STORAGE_CONFIG_COPY_B_ADDRESS;
  }
  else
  {
    target = ((int32_t)(configA.sequence - configB.sequence) > 0)
               ? STORAGE_CONFIG_COPY_B_ADDRESS : STORAGE_CONFIG_COPY_A_ADDRESS;
  }

  ++config->sequence;
  Encode(config, encoded);
  if (!backend->write(target, encoded, sizeof(encoded), backend->context) ||
      !backend->read(target, verify, sizeof(verify), backend->context) ||
      !Decode(verify, &verified) || (verified.sequence != config->sequence))
  {
    --config->sequence;
    return false;
  }
  return true;
}
