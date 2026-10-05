/**
 * @file    bsp_oled.h
 * @brief   128×64 I2C OLED帧缓冲显示驱动。
 * @details 使用1024字节静态帧缓冲，任务函数每轮仅刷新一页以限制I2C阻塞时间。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-FUN-001。
 * 关联设计：DES-OLED-001。
 */
#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 探测0x3C并初始化SSD1315兼容控制器；失败时进入无显示降级模式。 */
bool BspOled_Init(void);
/** @brief 查询当前显示设备是否仍可用。 */
bool BspOled_IsAvailable(void);
/** @brief 清空RAM帧缓冲并请求异步分页刷新。 */
void BspOled_Clear(void);
/** @brief 在指定页绘制ASCII字符串；越过128列的字符自动截断。 */
void BspOled_DrawString(uint8_t x, uint8_t page, const char *text);
/** @brief 标记整屏待刷新，不在本函数中传输I2C数据。 */
void BspOled_Present(void);
/** @brief 每次调用最多发送一页显示数据；不得在中断中调用。 */
void BspOled_Task(void);

#endif
