/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/**
 * @file    main.h
 * @brief   CubeMX 公共定义及本项目板级引脚约束。
 * @details PA13、PA14 未定义为业务引脚，始终保留 SWD；LOCK_SIM 为阶段 0--6 的 LED
 *          模拟输出，不得直接连接 12 V 电磁锁。
 * @version 0.1.0
 * @date    2026-10-05
 * 关联需求：REQ-HW-001、REQ-LOCK-001。
 * 关联设计：DES-GPIO-001。
 */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
__NO_RETURN void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/** 低压 LED 模拟门锁输出，真实锁必须通过已验证的外部驱动级。 */
#define LOCK_SIM_Pin GPIO_PIN_8
#define LOCK_SIM_GPIO_Port GPIOA
/** 板载状态 LED，硬件为低电平有效。 */
#define STATUS_LED_Pin GPIO_PIN_13
#define STATUS_LED_GPIO_Port GPIOC
/** RC522 软件片选和硬件复位脚。 */
#define RC522_CS_Pin GPIO_PIN_4
#define RC522_CS_GPIO_Port GPIOA
#define RC522_RST_Pin GPIO_PIN_0
#define RC522_RST_GPIO_Port GPIOB
/** 蜂鸣器控制输出；其有效电平须以实物接线为准。 */
#define BUZZER_Pin GPIO_PIN_1
#define BUZZER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
