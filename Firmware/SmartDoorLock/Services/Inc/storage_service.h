/**
 * @file    storage_service.h
 * @brief   配置双副本、CRC校验和掉电恢复服务。
 * @details 本层通过抽象后端访问EEPROM，因而可由主机内存后端进行自动测试。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-DATA-001。
 * 关联设计：DES-STORAGE-001。
 */
#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYSTEM_PIN_MAX_LENGTH 6U /**< PIN允许4～6位十进制字符。 */

/** @brief 存储读取回调；address和length必须在AT24C02范围内。 */
typedef bool (*StorageReadFn)(uint16_t address, uint8_t *data, size_t length, void *context);
/** @brief 存储写入回调；底层负责页边界和写周期处理。 */
typedef bool (*StorageWriteFn)(uint16_t address, const uint8_t *data, size_t length, void *context);

/** 存储后端依赖注入接口。 */
typedef struct
{
  StorageReadFn read; /**< 读取实现，不得为NULL。 */
  StorageWriteFn write; /**< 写入实现，不得为NULL。 */
  void *context; /**< 后端私有上下文。 */
} StorageBackend;

/** V1持久化配置的内存表示。PIN明文限制见Docs/storage-layout.md。 */
typedef struct
{
  uint32_t sequence; /**< 双副本序号，较新的有效记录优先。 */
  uint8_t userPin[SYSTEM_PIN_MAX_LENGTH]; /**< 普通用户PIN，未使用字节为0。 */
  uint8_t adminPin[SYSTEM_PIN_MAX_LENGTH]; /**< 管理员PIN，未使用字节为0。 */
  uint8_t userPinLength; /**< 普通PIN有效长度，范围4～6。 */
  uint8_t adminPinLength; /**< 管理员PIN有效长度，范围4～6。 */
  uint8_t failureLimit; /**< 连续失败阈值，范围1～10。 */
  uint8_t unlockSeconds; /**< 模拟开锁时长，范围1～30秒。 */
  uint16_t lockoutSeconds; /**< 临时锁定时长，范围10～3600秒。 */
} SystemConfig;

typedef enum
{
  STORAGE_LOAD_DEFAULT = 0,
  STORAGE_LOAD_COPY_A,
  STORAGE_LOAD_COPY_B
} StorageLoadResult;

/** @brief 生成安全可运行的默认配置；本函数不写EEPROM。 */
void StorageService_DefaultConfig(SystemConfig *config);
/** @brief 加载两个配置副本，自动选择CRC有效且序号较新的记录。 */
StorageLoadResult StorageService_LoadConfig(const StorageBackend *backend,
                                            SystemConfig *config);
/** @brief 写入较旧副本并回读验证；失败时恢复调用者原有sequence。 */
bool StorageService_SaveConfig(const StorageBackend *backend, SystemConfig *config);
/** @brief 校验PIN长度、字符范围和所有安全配置范围。 */
bool StorageService_ConfigIsValid(const SystemConfig *config);

#endif
