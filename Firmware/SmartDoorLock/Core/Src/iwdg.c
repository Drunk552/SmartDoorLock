/**
 * @file    iwdg.c
 * @brief   独立看门狗寄存器级初始化与刷新。
 * @details 以 LSI 标称频率估算约 2 s 超时；LSI 存在器件误差，因此该值用于异常恢复而
 *          不能作为精确计时基准。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-SEC-001。
 * 关联设计：DES-WDG-001。
 */
#include "iwdg.h"

#include "stm32f1xx.h"

/** IWDG 硬件规定的启动、刷新和配置访问键值。 */
#define IWDG_KEY_ENABLE 0xCCCCU
#define IWDG_KEY_RELOAD 0xAAAAU
#define IWDG_KEY_WRITE_ACCESS 0x5555U
/** LSI 标称约 40 kHz 下的 /64 分频与约 2 s 重装载值。 */
#define IWDG_PRESCALER_DIV64 0x04U
#define IWDG_RELOAD_TWO_SECONDS 1249U

/** 写访问必须先解锁；首次装载后才使能，避免以未知重装载值启动看门狗。 */
void MX_IWDG_Init(void)
{
  IWDG->KR = IWDG_KEY_WRITE_ACCESS;
  IWDG->PR = IWDG_PRESCALER_DIV64;
  IWDG->RLR = IWDG_RELOAD_TWO_SECONDS;
  IWDG->KR = IWDG_KEY_RELOAD;
  IWDG->KR = IWDG_KEY_ENABLE;
}

/** 由主循环末尾调用，不能在中断中刷新以免掩盖主循环卡死。 */
void Iwdg_Refresh(void)
{
  IWDG->KR = IWDG_KEY_RELOAD;
}
