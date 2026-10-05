/**
 * @file    time_utils.h
 * @brief   32位毫秒时基工具。
 * @details 所有超时均使用无符号减法，保证HAL_GetTick()回绕后仍能正确判断。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-SAF-003。
 * 关联设计：DES-TIME-001。
 */
#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 判断从start到now是否已经经过interval毫秒。
 * @param[in] now 当前毫秒时基。
 * @param[in] start 起始毫秒时基。
 * @param[in] interval 等待时长，单位ms。
 * @return 已到期返回true；本函数支持32位时基回绕。
 */
static inline bool TimeUtils_Elapsed(uint32_t now, uint32_t start, uint32_t interval)
{
  return (uint32_t)(now - start) >= interval;
}

#endif
