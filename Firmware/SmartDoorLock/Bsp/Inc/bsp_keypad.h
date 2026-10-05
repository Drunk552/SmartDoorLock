/**
 * @file    bsp_keypad.h
 * @brief   4×4矩阵键盘非阻塞扫描与消抖。
 * @details 每次任务仅扫描一行，20ms稳定后生成一次按下事件，避免在主循环使用延时。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-002。
 * 关联设计：DES-KEYPAD-001。
 */
#ifndef BSP_KEYPAD_H
#define BSP_KEYPAD_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 初始化行列输出、扫描状态和按键事件缓存。 */
void BspKeypad_Init(void);
/** @brief 每1ms最多扫描一行；now使用HAL_GetTick()毫秒时基。 */
void BspKeypad_Task(uint32_t now);
/** @brief 取走一枚已消抖的按键；无待处理事件时返回false。 */
bool BspKeypad_GetKey(char *key);

#endif
