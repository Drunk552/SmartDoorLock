/**
 * @file    bsp_at24c02.h
 * @brief   AT24C02 I2C页读写驱动。
 * @details V1按256字节、8字节页处理；地址布局由storage_layout.h集中定义。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-DATA-001。
 * 关联设计：DES-STORAGE-001。
 */
#ifndef BSP_AT24C02_H
#define BSP_AT24C02_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 探测7位I2C地址0x50是否响应。 */
bool BspAt24c02_IsReady(void);
/** @brief 读取非零长度数据；address+length必须不超过256字节。 */
bool BspAt24c02_Read(uint16_t address, uint8_t *data, size_t length);
/** @brief 按8字节页边界拆分写入，并在每页后进行有限ACK轮询。 */
bool BspAt24c02_Write(uint16_t address, const uint8_t *data, size_t length);

#endif
