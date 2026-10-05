/**
 * @file    bsp_rc522.c
 * @brief   RC522 SPI 通信与 ISO14443A 4 字节 UID 读取。
 * @details 每次事务均有 SPI 和命令级双重超时；当前只实现一级防冲突 UID，7/10 字节卡号
 *          不应被误认为已支持。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-CARD-001。
 * 关联设计：DES-RC522-001。
 */
#include "bsp_rc522.h"
#include "main.h"
#include "spi.h"
#include "time_utils.h"

#include <stddef.h>

/** 单次 SPI 全双工访问和 MFRC522 命令完成的最大等待时间，单位毫秒。 */
#define RC522_SPI_TIMEOUT_MS 10U
#define RC522_COMMAND_TIMEOUT_MS 30U

#define REG_COMMAND       0x01U
#define REG_COM_IRQ       0x04U
#define REG_ERROR         0x06U
#define REG_FIFO_DATA     0x09U
#define REG_FIFO_LEVEL    0x0AU
#define REG_CONTROL       0x0CU
#define REG_BIT_FRAMING   0x0DU
#define REG_MODE          0x11U
#define REG_TX_CONTROL    0x14U
#define REG_TX_ASK        0x15U
#define REG_T_MODE        0x2AU
#define REG_T_PRESCALER   0x2BU
#define REG_T_RELOAD_H    0x2CU
#define REG_T_RELOAD_L    0x2DU
#define REG_VERSION       0x37U

#define CMD_IDLE          0x00U
#define CMD_TRANSCEIVE    0x0CU
#define CMD_SOFT_RESET    0x0FU
#define PICC_REQA         0x26U
#define PICC_ANTICOLL_CL1 0x93U

/** RC522 的 NSS 为低有效；每次传输都必须成对拉低/拉高片选。 */
static void Select(bool selected)
{
  HAL_GPIO_WritePin(RC522_CS_GPIO_Port,
                    RC522_CS_Pin,
                    selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/** 封装有界 SPI 传输，任何 HAL 失败后都会释放片选。 */
static bool Transfer(uint8_t *tx, uint8_t *rx, uint16_t length)
{
  bool ok;
  Select(true);
  ok = HAL_SPI_TransmitReceive(&hspi1,
                               tx,
                               rx,
                               length,
                               RC522_SPI_TIMEOUT_MS) == HAL_OK;
  Select(false);
  return ok;
}

/** 按 MFRC522 SPI 地址格式写单个寄存器。 */
static bool WriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t tx[2] = {(uint8_t)(((uint32_t)reg << 1U) & 0x7EU), value};
  uint8_t rx[2];
  return Transfer(tx, rx, (uint16_t)sizeof(tx));
}

/** 按 MFRC522 SPI 地址格式读单个寄存器。 */
static bool ReadRegister(uint8_t reg, uint8_t *value)
{
  uint8_t tx[2] = {(uint8_t)((((uint32_t)reg << 1U) & 0x7EU) | 0x80U), 0U};
  uint8_t rx[2];
  if ((value == NULL) || !Transfer(tx, rx, (uint16_t)sizeof(tx)))
  {
    return false;
  }
  *value = rx[1];
  return true;
}

/** 读改写置位，供 FIFO 清空、天线和启动位控制使用。 */
static bool SetBits(uint8_t reg, uint8_t mask)
{
  uint8_t value;
  return ReadRegister(reg, &value) && WriteRegister(reg, (uint8_t)(value | mask));
}

/** 读改写清位，确保收发完成后退出 StartSend 状态。 */
static bool ClearBits(uint8_t reg, uint8_t mask)
{
  uint8_t value;
  return ReadRegister(reg, &value) && WriteRegister(reg, (uint8_t)(value & (uint8_t)~mask));
}

/**
 * @brief 向 RC522 FIFO 发送一帧并有限等待响应。
 * @details 接收前核对 FIFO 长度不超过调用方容量，并将定时器超时和协议错误统一返回 false。
 */
static bool Transceive(const uint8_t *sendData,
                       uint8_t sendLength,
                       uint8_t txLastBits,
                       uint8_t *receiveData,
                       uint8_t *receiveLength)
{
  uint8_t irq;
  uint8_t error;
  uint8_t fifoLevel;
  uint8_t index;
  uint32_t startedAt;

  if ((sendData == NULL) || (sendLength == 0U) ||
      (receiveData == NULL) || (receiveLength == NULL))
  {
    return false;
  }
  if (!WriteRegister(REG_COMMAND, CMD_IDLE) ||
      !WriteRegister(REG_COM_IRQ, 0x7FU) ||
      !SetBits(REG_FIFO_LEVEL, 0x80U) ||
      !WriteRegister(REG_BIT_FRAMING, txLastBits & 0x07U))
  {
    return false;
  }
  for (index = 0U; index < sendLength; ++index)
  {
    if (!WriteRegister(REG_FIFO_DATA, sendData[index]))
    {
      return false;
    }
  }
  if (!WriteRegister(REG_COMMAND, CMD_TRANSCEIVE) || !SetBits(REG_BIT_FRAMING, 0x80U))
  {
    return false;
  }

  startedAt = HAL_GetTick();
  do
  {
    if (!ReadRegister(REG_COM_IRQ, &irq))
    {
      return false;
    }
    if ((irq & 0x01U) != 0U)
    {
      return false;
    }
  } while (((irq & 0x30U) == 0U) &&
           !TimeUtils_Elapsed(HAL_GetTick(), startedAt, RC522_COMMAND_TIMEOUT_MS));
  (void)ClearBits(REG_BIT_FRAMING, 0x80U);

  if ((irq & 0x30U) == 0U || !ReadRegister(REG_ERROR, &error) ||
      ((error & 0x1BU) != 0U) || !ReadRegister(REG_FIFO_LEVEL, &fifoLevel) ||
      (fifoLevel == 0U) || (fifoLevel > *receiveLength))
  {
    return false;
  }
  for (index = 0U; index < fifoLevel; ++index)
  {
    if (!ReadRegister(REG_FIFO_DATA, &receiveData[index]))
    {
      return false;
    }
  }
  *receiveLength = fifoLevel;
  return true;
}

/** 读取版本寄存器，供初始化后的接线诊断；0 或 0xFF 通常表示通信失败。 */
uint8_t BspRc522_GetVersion(void)
{
  uint8_t version = 0U;
  (void)ReadRegister(REG_VERSION, &version);
  return version;
}

/**
 * @brief 复位并配置 RC522 的定时器、ASK 和天线输出。
 * @details 软复位完成等待有 30 ms 上限；版本检查不能替代真实刷卡台架测试。
 */
bool BspRc522_Init(void)
{
  uint8_t command;
  uint32_t startedAt;

  HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
  if (!WriteRegister(REG_COMMAND, CMD_SOFT_RESET))
  {
    return false;
  }
  startedAt = HAL_GetTick();
  do
  {
    if (!ReadRegister(REG_COMMAND, &command))
    {
      return false;
    }
  } while (((command & 0x10U) != 0U) &&
           !TimeUtils_Elapsed(HAL_GetTick(), startedAt, RC522_COMMAND_TIMEOUT_MS));

  if ((command & 0x10U) != 0U ||
      !WriteRegister(REG_T_MODE, 0x8DU) ||
      !WriteRegister(REG_T_PRESCALER, 0x3EU) ||
      !WriteRegister(REG_T_RELOAD_L, 30U) ||
      !WriteRegister(REG_T_RELOAD_H, 0U) ||
      !WriteRegister(REG_TX_ASK, 0x40U) ||
      !WriteRegister(REG_MODE, 0x3DU) ||
      !SetBits(REG_TX_CONTROL, 0x03U))
  {
    return false;
  }
  command = BspRc522_GetVersion();
  return (command != 0U) && (command != 0xFFU);
}

/**
 * @brief 请求卡片并读取一级防冲突的 4 字节 UID。
 * @details 校验 BCC 后才返回 UID；不支持级联 UID 的卡会返回 false，由上层按未识别处理。
 */
bool BspRc522_ReadUid(uint8_t uid[4])
{
  uint8_t request[] = {PICC_REQA};
  uint8_t antiCollision[] = {PICC_ANTICOLL_CL1, 0x20U};
  uint8_t response[5];
  uint8_t length = (uint8_t)sizeof(response);
  uint8_t bcc;

  if (uid == NULL)
  {
    return false;
  }
  if (!Transceive(request, 1U, 7U, response, &length) || (length != 2U))
  {
    return false;
  }
  length = (uint8_t)sizeof(response);
  if (!Transceive(antiCollision, 2U, 0U, response, &length) || (length != 5U))
  {
    return false;
  }
  bcc = (uint8_t)(response[0] ^ response[1] ^ response[2] ^ response[3]);
  if (bcc != response[4])
  {
    return false;
  }
  uid[0] = response[0];
  uid[1] = response[1];
  uid[2] = response[2];
  uid[3] = response[3];
  return true;
}
