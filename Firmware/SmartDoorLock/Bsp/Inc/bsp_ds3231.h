/**
 * @file    bsp_ds3231.h
 * @brief   DS3231离线RTC读取驱动。
 * @details 驱动检查振荡器停止标志和BCD范围；时间无效时由应用层降级显示。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-004。
 * 关联设计：DES-RTC-001。
 */
#ifndef BSP_DS3231_H
#define BSP_DS3231_H

#include <stdbool.h>
#include <stdint.h>

/** 已转换为二进制的RTC时间，year范围2000～2099。 */
typedef struct
{
  uint16_t year; /**< 年份，范围2000～2099。 */
  uint8_t month; /**< 月份，范围1～12。 */
  uint8_t day; /**< 日期，范围1～31，未包含闰年校验。 */
  uint8_t weekday; /**< 星期寄存器值，范围1～7。 */
  uint8_t hour; /**< 24小时制小时，范围0～23。 */
  uint8_t minute; /**< 分钟，范围0～59。 */
  uint8_t second; /**< 秒，范围0～59。 */
} BspDs3231DateTime;

/** @brief 探测DS3231的7位I2C地址0x68。 */
bool BspDs3231_IsReady(void);
/** @brief 读取并校验当前时间；OSF置位、I2C失败或BCD异常时返回false。 */
bool BspDs3231_Read(BspDs3231DateTime *dateTime);

#endif
