/**
 * @file    bsp_board.h
 * @brief   开发板状态灯、锁模拟LED和蜂鸣器的最小板级抽象。
 * @details PA8在阶段0～6只允许接LED模拟，真实12V锁必须经阶段7驱动验证。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-SAF-001、REQ-SAF-002。
 * 关联设计：DES-BSP-BOARD-001。
 */
#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include <stdbool.h>

/** @brief 设置PC13低有效板载状态灯的逻辑开关。 */
void BspBoard_SetStatusLed(bool on);
/** @brief 翻转PC13状态灯，供主循环心跳使用。 */
void BspBoard_ToggleStatusLed(void);
/** @brief 设置PA8锁模拟输出；不得将其视为真实锁功率输出。 */
void BspBoard_SetLockSimulation(bool unlocked);
/** @brief 设置PB1蜂鸣器逻辑输出；有效电平需由实物测试确认。 */
void BspBoard_SetBuzzer(bool on);

#endif
