/**
 * @file    i2c.c
 * @brief   I2C1（OLED、AT24C02、DS3231）CubeMX 初始化。
 * @details 100 kHz 适配当前模块和杜邦线台架接线；各设备访问超时在 BSP 层定义，不能由
 *          初始化配置替代。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-UI-001、REQ-STORAGE-001、REQ-TIME-001。
 * 关联设计：DES-I2C-001。
 */
#include "i2c.h"

I2C_HandleTypeDef hi2c1;

/** 初始化 I2C1 为 7 位地址、标准模式，SCL/PB6 与 SDA/PB7 为开漏复用输出。 */
void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000; /* 100 kHz：优先提高多模块台架可靠性。 */
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

/** 配置 I2C1 时钟和 PB6/PB7；外部上拉电阻仍需由实物接线确认。 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (i2cHandle->Instance == I2C1)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

/** 反初始化仅供 HAL 生命周期使用，正常门锁运行不调用。 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2cHandle)
{
  if (i2cHandle->Instance == I2C1)
  {
    __HAL_RCC_I2C1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
  }
}
