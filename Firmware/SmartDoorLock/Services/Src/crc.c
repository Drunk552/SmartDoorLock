/**
 * @file    crc.c
 * @brief   EEPROM 记录完整性校验的无硬件依赖实现。
 * @details 采用逐位算法换取极小代码体积和可移植性；配置写入不频繁，不需要额外 CRC 外设。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-STORAGE-001。
 * 关联设计：DES-STORAGE-001。
 */
#include "crc.h"

/** 计算 CRC-16/CCITT，用于检测配置记录的掉电写入或位翻转损坏。 */
uint16_t Crc16Ccitt(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFFU;
  size_t index;

  if (data == NULL)
  {
    return 0U;
  }
  for (index = 0U; index < length; ++index)
  {
    uint8_t bit;
    crc ^= (uint16_t)data[index] << 8U;
    for (bit = 0U; bit < 8U; ++bit)
    {
      uint32_t shifted = (uint32_t)crc << 1U;
      if ((crc & 0x8000U) != 0U)
      {
        shifted ^= 0x1021U;
      }
      crc = (uint16_t)shifted;
    }
  }
  return crc;
}

/** 计算 CRC-8，用于短小凭据记录的低开销完整性检查。 */
uint8_t Crc8(const uint8_t *data, size_t length)
{
  uint8_t crc = 0U;
  size_t index;

  if (data == NULL)
  {
    return 0U;
  }
  for (index = 0U; index < length; ++index)
  {
    uint8_t bit;
    crc ^= data[index];
    for (bit = 0U; bit < 8U; ++bit)
    {
      uint32_t shifted = (uint32_t)crc << 1U;
      if ((crc & 0x80U) != 0U)
      {
        shifted ^= 0x07U;
      }
      crc = (uint8_t)shifted;
    }
  }
  return crc;
}
