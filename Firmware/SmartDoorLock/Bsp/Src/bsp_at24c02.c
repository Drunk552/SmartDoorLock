/**
 * @file    bsp_at24c02.c
 * @brief   AT24C02 页读写和写周期轮询驱动。
 * @details 所有地址范围先检查；写入在页边界分段，并使用有限 ACK 轮询等待内部写周期完成。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-STORAGE-001。
 * 关联设计：DES-STORAGE-001。
 */
#include "bsp_at24c02.h"
#include "i2c.h"
#include "time_utils.h"

#include <stddef.h>
#include <string.h>

/** 7 位 I2C 地址左移一位，符合 STM32 HAL 参数格式。 */
#define AT24C02_ADDRESS_HAL       (0x50U << 1U)
/** AT24C02 的可寻址容量和页写边界。 */
#define AT24C02_SIZE_BYTES        256U
#define AT24C02_PAGE_SIZE         8U
/** 单次总线事务和内部写周期的最大等待时间，单位毫秒。 */
#define AT24C02_IO_TIMEOUT_MS     20U
#define AT24C02_WRITE_TIMEOUT_MS  20U

/** 使用减法形式检查 [address, address + length)，避免加法溢出绕过边界。 */
static bool RangeIsValid(uint16_t address, size_t length)
{
  return (length <= AT24C02_SIZE_BYTES) &&
         (address < AT24C02_SIZE_BYTES) &&
         (length <= (size_t)(AT24C02_SIZE_BYTES - address));
}

/** 探测 EEPROM ACK；失败由上层决定是否使用默认配置。 */
bool BspAt24c02_IsReady(void)
{
  return HAL_I2C_IsDeviceReady(&hi2c1,
                               AT24C02_ADDRESS_HAL,
                               2U,
                               AT24C02_IO_TIMEOUT_MS) == HAL_OK;
}

/** 有界随机读，禁止零长度、空指针和超出 256 字节地址空间的请求。 */
bool BspAt24c02_Read(uint16_t address, uint8_t *data, size_t length)
{
  if ((data == NULL) || (length == 0U) || !RangeIsValid(address, length) ||
      (length > UINT16_MAX))
  {
    return false;
  }
  return HAL_I2C_Mem_Read(&hi2c1,
                          AT24C02_ADDRESS_HAL,
                          address,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          (uint16_t)length,
                          AT24C02_IO_TIMEOUT_MS) == HAL_OK;
}

/**
 * @brief 按页写入并等待每页内部写周期完成。
 * @details 不跨页发送写命令，防止 AT24C02 页回绕覆盖；ACK 轮询有 20 ms 超时，不会无限等待。
 */
bool BspAt24c02_Write(uint16_t address, const uint8_t *data, size_t length)
{
  size_t offset = 0U;

  if ((data == NULL) || (length == 0U) || !RangeIsValid(address, length))
  {
    return false;
  }

  while (offset < length)
  {
    uint8_t pageBuffer[AT24C02_PAGE_SIZE];
    uint16_t currentAddress = (uint16_t)(address + offset);
    size_t pageRemaining = AT24C02_PAGE_SIZE - (currentAddress % AT24C02_PAGE_SIZE);
    size_t chunk = (length - offset) < pageRemaining ? (length - offset) : pageRemaining;
    uint32_t startedAt;

    memcpy(pageBuffer, &data[offset], chunk);
    if (HAL_I2C_Mem_Write(&hi2c1,
                          AT24C02_ADDRESS_HAL,
                          currentAddress,
                          I2C_MEMADD_SIZE_8BIT,
                          pageBuffer,
                          (uint16_t)chunk,
                          AT24C02_IO_TIMEOUT_MS) != HAL_OK)
    {
      return false;
    }

    startedAt = HAL_GetTick();
    while (HAL_I2C_IsDeviceReady(&hi2c1, AT24C02_ADDRESS_HAL, 1U, 2U) != HAL_OK)
    {
      if (TimeUtils_Elapsed(HAL_GetTick(), startedAt, AT24C02_WRITE_TIMEOUT_MS))
      {
        return false;
      }
    }
    offset += chunk;
  }
  return true;
}
