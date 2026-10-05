/**
 * @file    iwdg.h
 * @brief   独立看门狗接口。
 * @details 仅在正常主循环末尾刷新；中断或外设回调不得调用刷新函数。
 * @version 0.1.0
 * @date    2026-10-05
 * 关联需求：REQ-SEC-001。
 * 关联设计：DES-WDG-001。
 */
#ifndef __IWDG_H__
#define __IWDG_H__

/** 初始化约 2 s 的独立看门狗。 */
void MX_IWDG_Init(void);
/** 在正常主循环末尾刷新看门狗。 */
void Iwdg_Refresh(void);

#endif
