/**
 * @file    crc.h
 * @brief   AT24C02配置和凭据记录的完整性校验。
 * @details CRC用于发现存储损坏，不承担密码保密或认证职责。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-DATA-001。
 * 关联设计：DES-STORAGE-001。
 */
#ifndef CRC_H
#define CRC_H

#include <stddef.h>
#include <stdint.h>

/** @brief 计算CRC-16/CCITT-FALSE，标准测试串"123456789"结果为0x29B1。 */
uint16_t Crc16Ccitt(const uint8_t *data, size_t length);

/** @brief 计算CRC-8，供短卡片和指纹映射记录使用。 */
uint8_t Crc8(const uint8_t *data, size_t length);

#endif
