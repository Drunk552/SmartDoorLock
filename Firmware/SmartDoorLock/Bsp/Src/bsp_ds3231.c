/**
 * @file    bsp_ds3231.c
 * @brief   DS3231 离线时间读取与有效性校验。
 * @details 仅当 I2C 事务、振荡器停止标志和 BCD 字段均有效时才返回时间，避免将失效
 *          RTC 时间写入界面或安全日志。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-TIME-001。
 * 关联设计：DES-RTC-001。
 */
#include "bsp_ds3231.h"
#include "i2c.h"

#include <stddef.h>

/** DS3231 的 7 位地址转为 STM32 HAL 使用的左移地址。 */
#define DS3231_ADDRESS_HAL   (0x68U << 1U)
/** 每次 I2C 读事务最多等待 20 ms。 */
#define DS3231_TIMEOUT_MS    20U
/** 状态寄存器和振荡器停止标志；置位说明时间可信度未知。 */
#define DS3231_STATUS_REG    0x0FU
#define DS3231_OSF_BIT       0x80U

/** 将已验证的 BCD 字节转换为自然数。 */
static uint8_t BcdToBinary(uint8_t value)
{
  return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

/** 屏蔽控制位后检查 BCD 半字节和业务上限。 */
static bool IsValidBcd(uint8_t value, uint8_t mask, uint8_t maximum)
{
  uint8_t masked = value & mask;
  return ((masked & 0x0FU) <= 9U) && (((masked >> 4U) & 0x0FU) <= 9U) &&
         (BcdToBinary(masked) <= maximum);
}

/** 探测 DS3231 是否在 I2C 总线上应答。 */
bool BspDs3231_IsReady(void)
{
  return HAL_I2C_IsDeviceReady(&hi2c1,
                               DS3231_ADDRESS_HAL,
                               2U,
                               DS3231_TIMEOUT_MS) == HAL_OK;
}

/**
 * @brief 读取并规范化 DS3231 日期时间。
 * @details 同时兼容 12/24 小时寄存器格式；不尝试“修复”无效日期，失败交由上层显示占位符。
 */
bool BspDs3231_Read(BspDs3231DateTime *dateTime)
{
  uint8_t registers[7];
  uint8_t status;
  uint8_t hour;

  if (dateTime == NULL)
  {
    return false;
  }
  if (HAL_I2C_Mem_Read(&hi2c1,
                       DS3231_ADDRESS_HAL,
                       0x00U,
                       I2C_MEMADD_SIZE_8BIT,
                       registers,
                       (uint16_t)sizeof(registers),
                       DS3231_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }
  if (HAL_I2C_Mem_Read(&hi2c1,
                       DS3231_ADDRESS_HAL,
                       DS3231_STATUS_REG,
                       I2C_MEMADD_SIZE_8BIT,
                       &status,
                       1U,
                       DS3231_TIMEOUT_MS) != HAL_OK ||
      (status & DS3231_OSF_BIT) != 0U)
  {
    return false;
  }

  if (!IsValidBcd(registers[0], 0x7FU, 59U) ||
      !IsValidBcd(registers[1], 0x7FU, 59U) ||
      !IsValidBcd(registers[4], 0x3FU, 31U) ||
      !IsValidBcd(registers[5], 0x1FU, 12U) ||
      !IsValidBcd(registers[6], 0xFFU, 99U))
  {
    return false;
  }

  if ((registers[2] & 0x40U) != 0U)
  {
    uint8_t hour12 = BcdToBinary(registers[2] & 0x1FU);
    if ((hour12 == 0U) || (hour12 > 12U))
    {
      return false;
    }
    hour = (uint8_t)(hour12 % 12U);
    if ((registers[2] & 0x20U) != 0U)
    {
      hour = (uint8_t)(hour + 12U);
    }
  }
  else
  {
    if (!IsValidBcd(registers[2], 0x3FU, 23U))
    {
      return false;
    }
    hour = BcdToBinary(registers[2] & 0x3FU);
  }

  dateTime->second = BcdToBinary(registers[0] & 0x7FU);
  dateTime->minute = BcdToBinary(registers[1] & 0x7FU);
  dateTime->hour = hour;
  dateTime->weekday = registers[3] & 0x07U;
  dateTime->day = BcdToBinary(registers[4] & 0x3FU);
  dateTime->month = BcdToBinary(registers[5] & 0x1FU);
  dateTime->year = (uint16_t)(2000U + BcdToBinary(registers[6]));
  return (dateTime->weekday >= 1U) && (dateTime->weekday <= 7U) &&
         (dateTime->day >= 1U) && (dateTime->month >= 1U);
}
