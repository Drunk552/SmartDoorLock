/**
 * @file    i2c.h
 * @brief   I2C1 共享总线接口。
 * @details OLED、AT24C02 与 DS3231 共用此句柄；访问串行化和超时由各 BSP 驱动负责。
 * @version 0.1.0
 * @date    2026-10-05
 * 关联需求：REQ-UI-001、REQ-STORAGE-001、REQ-TIME-001。
 * 关联设计：DES-I2C-001。
 */
#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** I2C1 的 CubeMX HAL 句柄，仅 BSP 驱动使用。 */
extern I2C_HandleTypeDef hi2c1;

/** 初始化 100 kHz I2C1。 */
void MX_I2C1_Init(void);

#ifdef __cplusplus
}
#endif

#endif
