/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
/**
 * @file    usart.h
 * @brief   USART1 日志和 USART2 AS608 通信接口。
 * @details USART2 句柄由指纹 BSP 用于有限超时发送和 1 字节中断接收；业务层不得直接访问。
 * @version 0.1.0
 * @date    2026-10-05
 * 关联需求：REQ-DEBUG-001、REQ-FINGER-001。
 * 关联设计：DES-UART-001。
 */

/* USER CODE END Includes */

/** USART1：台架调试日志；USART2：AS608 指纹模块。 */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/** 初始化日志串口和指纹模块串口。 */
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

