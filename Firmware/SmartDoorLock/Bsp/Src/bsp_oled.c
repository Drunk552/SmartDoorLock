/**
 * @file    bsp_oled.c
 * @brief   SSD1306 兼容 OLED 的帧缓冲显示驱动。
 * @details 采用 128×64 单色帧缓冲（1024 B SRAM），每次任务仅发送一页，避免完整刷新
 *          长时间独占 I2C 总线。当前字库为受限 ASCII，中文文案由上层转为英文短语显示。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-UI-001。
 * 关联设计：DES-OLED-001。
 */
#include "bsp_oled.h"
#include "i2c.h"

#include <stddef.h>
#include <string.h>

/** I2C 地址按 HAL 约定左移一位；128×64 屏幕共有 8 个页。 */
#define OLED_ADDRESS_HAL       (0x3CU << 1U)
#define OLED_WIDTH             128U
#define OLED_PAGE_COUNT        8U
/** 单页命令或数据发送的最大等待时间，单位毫秒。 */
#define OLED_I2C_TIMEOUT_MS    20U
/** 5×7 字符列宽及字符间空列，需与 s_font 数据一致。 */
#define OLED_GLYPH_WIDTH       5U
#define OLED_CHARACTER_SPACING 1U

/** 帧缓冲由本模块独占；刷新标志让绘图接口无需直接执行 I2C 传输。 */
static uint8_t s_framebuffer[OLED_WIDTH * OLED_PAGE_COUNT];
static bool s_available;
static bool s_refreshPending;
static uint8_t s_refreshPage;

static const uint8_t s_font[][OLED_GLYPH_WIDTH] = {
  {0x00,0x00,0x00,0x00,0x00}, /* space */
  {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
  {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
  {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
  {0x00,0x36,0x36,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}
};

/** 将支持的 ASCII 映射到字库索引，未知字符显示为空格以保持数组边界安全。 */
static uint8_t GlyphIndex(char character)
{
  if ((character >= 'A') && (character <= 'Z'))
  {
    return (uint8_t)(1 + (character - 'A'));
  }
  if ((character >= 'a') && (character <= 'z'))
  {
    return (uint8_t)(1 + (character - 'a'));
  }
  if ((character >= '0') && (character <= '9'))
  {
    return (uint8_t)(27 + (character - '0'));
  }
  if (character == ':')
  {
    return 37U;
  }
  if (character == '-')
  {
    return 38U;
  }
  return 0U;
}

/** 在命令前加入 SSD1306 控制字节 0x00；局部缓冲限制单次命令长度。 */
static bool WriteCommands(const uint8_t *commands, uint8_t count)
{
  uint8_t buffer[32];

  if ((commands == NULL) || (count == 0U) || (count >= sizeof(buffer)))
  {
    return false;
  }

  buffer[0] = 0x00U;
  memcpy(&buffer[1], commands, count);
  return HAL_I2C_Master_Transmit(&hi2c1,
                                 OLED_ADDRESS_HAL,
                                 buffer,
                                 (uint16_t)(count + 1U),
                                 OLED_I2C_TIMEOUT_MS) == HAL_OK;
}

/** 探测显示器并执行固定初始化序列；失败后上层仍可通过 USART1 诊断系统。 */
bool BspOled_Init(void)
{
  static const uint8_t initCommands[] = {
    0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,
    0x8D,0x14,0x20,0x02,0xA1,0xC8,0xDA,0x12,
    0x81,0x7F,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,
    0xAF
  };

  s_available = false;
  BspOled_Clear();
  if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_ADDRESS_HAL, 2U, OLED_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }

  s_available = WriteCommands(initCommands, (uint8_t)sizeof(initCommands));
  if (s_available)
  {
    BspOled_Present();
  }
  return s_available;
}

/** 返回最近一次 I2C 传输后的显示器可用状态。 */
bool BspOled_IsAvailable(void)
{
  return s_available;
}

/** 清空帧缓冲并登记异步页刷新，而非立即循环发送 8 页。 */
void BspOled_Clear(void)
{
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
  BspOled_Present();
}

/** 在指定页绘制受支持字符串；到右边界即停止，防止帧缓冲越界。 */
void BspOled_DrawString(uint8_t x, uint8_t page, const char *text)
{
  if ((text == NULL) || (page >= OLED_PAGE_COUNT))
  {
    return;
  }

  while ((*text != '\0') && ((uint16_t)x + OLED_GLYPH_WIDTH <= OLED_WIDTH))
  {
    uint8_t column;
    uint8_t glyph = GlyphIndex(*text);
    for (column = 0U; column < OLED_GLYPH_WIDTH; ++column)
    {
      s_framebuffer[((uint16_t)page * OLED_WIDTH) + x] = s_font[glyph][column];
      ++x;
    }
    if (x < OLED_WIDTH)
    {
      s_framebuffer[((uint16_t)page * OLED_WIDTH) + x] = 0U;
      x = (uint8_t)(x + OLED_CHARACTER_SPACING);
    }
    ++text;
  }
  BspOled_Present();
}

/** 从第 0 页重新开始刷新；连续绘制会合并为最新帧。 */
void BspOled_Present(void)
{
  s_refreshPage = 0U;
  s_refreshPending = true;
}

/**
 * @brief 每次最多发送一页显示数据。
 * @details I2C 故障立即停用显示，避免主循环反复阻塞；后续可通过重新初始化恢复。
 */
void BspOled_Task(void)
{
  uint8_t command[3];
  uint8_t data[OLED_WIDTH + 1U];

  if (!s_available || !s_refreshPending)
  {
    return;
  }

  command[0] = (uint8_t)(0xB0U + s_refreshPage);
  command[1] = 0x00U;
  command[2] = 0x10U;
  if (!WriteCommands(command, (uint8_t)sizeof(command)))
  {
    s_available = false;
    s_refreshPending = false;
    return;
  }

  data[0] = 0x40U;
  memcpy(&data[1], &s_framebuffer[(uint16_t)s_refreshPage * OLED_WIDTH], OLED_WIDTH);
  if (HAL_I2C_Master_Transmit(&hi2c1,
                              OLED_ADDRESS_HAL,
                              data,
                              (uint16_t)sizeof(data),
                              OLED_I2C_TIMEOUT_MS) != HAL_OK)
  {
    s_available = false;
    s_refreshPending = false;
    return;
  }

  ++s_refreshPage;
  if (s_refreshPage >= OLED_PAGE_COUNT)
  {
    s_refreshPending = false;
  }
}
