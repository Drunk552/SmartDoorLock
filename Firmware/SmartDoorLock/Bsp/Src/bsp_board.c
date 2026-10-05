/**
 * @file    bsp_board.c
 * @brief   板级 LED、蜂鸣器和门锁模拟输出。
 * @details 阶段 0--6 的 LOCK_SIM 仅是低压 LED 指示。真实 12 V 电磁锁必须经过独立
 *          MOSFET/继电器和续流保护验证后，才能在后续阶段替换该适配。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-LOCK-001、REQ-UI-001。
 * 关联设计：DES-BSP-BOARD-001。
 */
#include "bsp_board.h"
#include "main.h"

/** 状态 LED 为低电平有效，故逻辑 on 映射为 GPIO RESET。 */
void BspBoard_SetStatusLed(bool on)
{
  HAL_GPIO_WritePin(STATUS_LED_GPIO_Port,
                    STATUS_LED_Pin,
                    on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/** 心跳指示仅改变 LED，不影响门锁安全输出。 */
void BspBoard_ToggleStatusLed(void)
{
  HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin);
}

/** LED 模拟开锁状态；不得据此推断真实锁驱动已通过台架测试。 */
void BspBoard_SetLockSimulation(bool unlocked)
{
  HAL_GPIO_WritePin(LOCK_SIM_GPIO_Port,
                    LOCK_SIM_Pin,
                    unlocked ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** 蜂鸣器有效电平由 GPIO 初始化和实物接线验证后统一在此适配。 */
void BspBoard_SetBuzzer(bool on)
{
  HAL_GPIO_WritePin(BUZZER_GPIO_Port,
                    BUZZER_Pin,
                    on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
