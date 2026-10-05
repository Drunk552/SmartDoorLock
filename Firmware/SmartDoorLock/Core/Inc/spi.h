/**
 * @file    spi.h
 * @brief   SPI1（RC522）接口。
 * @details 片选不由 HAL NSS 管理，必须由 RC522 BSP 在每笔受限传输前后控制。
 * @version 0.1.0
 * @date    2026-10-05
 * 关联需求：REQ-CARD-001。
 * 关联设计：DES-SPI-001。
 */
#ifndef __SPI_H__
#define __SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** SPI1 HAL 句柄，仅 RC522 BSP 使用。 */
extern SPI_HandleTypeDef hspi1;
/** 初始化兼容 RC522 的 SPI 模式 0。 */
void MX_SPI1_Init(void);

#ifdef __cplusplus
}
#endif

#endif
