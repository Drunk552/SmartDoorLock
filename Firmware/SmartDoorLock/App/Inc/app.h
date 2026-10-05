/**
 * @file    app.h
 * @brief   智能门锁应用层入口。
 * @details 负责调度认证、菜单、显示和外设服务；门锁GPIO只能通过LockService间接控制。
 *
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-001～007、REQ-SEC-001、REQ-SAF-002。
 * 关联设计：DES-APP-001。
 */
#ifndef APP_H
#define APP_H

/** @brief 初始化应用状态机及所有业务服务。不得在中断上下文调用。 */
void App_Init(void);

/** @brief 执行一次非阻塞应用调度；主循环每轮调用后才允许喂看门狗。 */
void App_Task(void);

#endif
