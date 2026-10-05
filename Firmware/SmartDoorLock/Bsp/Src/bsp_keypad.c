/**
 * @file    bsp_keypad.c
 * @brief   4×4 矩阵键盘轮询扫描与按下沿消抖。
 * @details 每 1 ms 仅扫描一行，候选状态稳定 20 ms 后才投递一次按键；不在中断中读取
 *          键盘，也不使用阻塞延时。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-KEYPAD-001。
 * 关联设计：DES-KEYPAD-001。
 */
#include "bsp_keypad.h"
#include "main.h"
#include "time_utils.h"

#include <stddef.h>

/** 行、列与按键数必须与 GPIOB 8--15 接线保持一致。 */
#define KEYPAD_ROW_COUNT   4U
#define KEYPAD_COL_COUNT   4U
#define KEYPAD_KEY_COUNT   16U
/** 行扫描周期和候选状态最短稳定时间，单位毫秒。 */
#define KEYPAD_SCAN_MS     1U
#define KEYPAD_DEBOUNCE_MS 20U

/** 行线逐行拉低，列线使用上拉输入，因此按下表现为低电平。 */
static const uint16_t s_rowPins[KEYPAD_ROW_COUNT] = {
  GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10, GPIO_PIN_11
};
static const uint16_t s_colPins[KEYPAD_COL_COUNT] = {
  GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};
static const char s_keyMap[KEYPAD_KEY_COUNT] = {
  '1','2','3','A', '4','5','6','B',
  '7','8','9','C', '*','0','#','D'
};

/** 每个按键保留候选/稳定状态和变化时刻；pendingKey 是单事件邮箱。 */
static bool s_candidate[KEYPAD_KEY_COUNT];
static bool s_stable[KEYPAD_KEY_COUNT];
static uint32_t s_candidateSince[KEYPAD_KEY_COUNT];
static uint32_t s_lastScan;
static uint8_t s_activeRow;
static char s_pendingKey;

/** 初始化为全部行高电平，避免上电时误将多行同时选通。 */
void BspKeypad_Init(void)
{
  uint8_t keyIndex;

  HAL_GPIO_WritePin(GPIOB,
                    GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11,
                    GPIO_PIN_SET);
  for (keyIndex = 0U; keyIndex < KEYPAD_KEY_COUNT; ++keyIndex)
  {
    s_candidate[keyIndex] = false;
    s_stable[keyIndex] = false;
    s_candidateSince[keyIndex] = 0U;
  }
  s_lastScan = 0U;
  s_activeRow = 0U;
  s_pendingKey = '\0';
}

/**
 * @brief 扫描一行并更新该行的消抖状态。
 * @details 只在未投递事件时记录新按下沿；这使长按不会重复输入密码字符。
 */
void BspKeypad_Task(uint32_t now)
{
  uint8_t column;

  if (!TimeUtils_Elapsed(now, s_lastScan, KEYPAD_SCAN_MS))
  {
    return;
  }
  s_lastScan = now;
  HAL_GPIO_WritePin(GPIOB,
                    GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11,
                    GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB, s_rowPins[s_activeRow], GPIO_PIN_RESET);

  for (column = 0U; column < KEYPAD_COL_COUNT; ++column)
  {
    uint8_t keyIndex = (uint8_t)((s_activeRow * KEYPAD_COL_COUNT) + column);
    bool pressed = HAL_GPIO_ReadPin(GPIOB, s_colPins[column]) == GPIO_PIN_RESET;

    if (pressed != s_candidate[keyIndex])
    {
      s_candidate[keyIndex] = pressed;
      s_candidateSince[keyIndex] = now;
    }
    else if ((pressed != s_stable[keyIndex]) &&
             TimeUtils_Elapsed(now, s_candidateSince[keyIndex], KEYPAD_DEBOUNCE_MS))
    {
      s_stable[keyIndex] = pressed;
      if (pressed && (s_pendingKey == '\0'))
      {
        s_pendingKey = s_keyMap[keyIndex];
      }
    }
  }
  s_activeRow = (uint8_t)((s_activeRow + 1U) % KEYPAD_ROW_COUNT);
}

/** 取走一个已消抖的按键事件，调用者必须在下一循环及时消费。 */
bool BspKeypad_GetKey(char *key)
{
  if ((key == NULL) || (s_pendingKey == '\0'))
  {
    return false;
  }
  *key = s_pendingKey;
  s_pendingKey = '\0';
  return true;
}
