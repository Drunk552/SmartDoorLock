/**
 * @file    bsp_rc522.h
 * @brief   MFRC522/RC522 SPI基础读卡驱动。
 * @details V1仅完成4字节UID的REQA和单级防冲突，不支持7/10字节UID。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-005。
 * 关联设计：DES-RC522-001。
 */
#ifndef BSP_RC522_H
#define BSP_RC522_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 复位并配置RC522定时器和天线；版本寄存器异常时返回false。 */
bool BspRc522_Init(void);
/** @brief 尝试读取一张4字节UID卡；未检测到卡或通信异常均返回false。 */
bool BspRc522_ReadUid(uint8_t uid[4]);
/** @brief 读取版本寄存器；I/O失败时返回0。 */
uint8_t BspRc522_GetVersion(void);

#endif
