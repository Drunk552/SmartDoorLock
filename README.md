# SmartDoorLock 智能门锁

基于STM32F103C8T6的离线智能门锁毕业设计，当前目标是完成密码、RFID和指纹三种本地认证方式，以及OLED交互、RTC时间、掉电配置存储和安全的电磁锁控制。

## 开发基线

- 主控：STM32F103C8T6
- 工程生成：STM32CubeMX
- 开发环境：Keil MDK（MDK-ARM）
- 固件库：STM32 HAL
- 下载调试：ST-Link + SWD
- 调试日志：USART1，115200，8N1
- Git模型：`main + develop + feature/*`

## 当前硬件

- 4×4矩阵键盘
- MFRC522/RC522 RFID模块
- AS608光学指纹模块（3.3V供电）
- SSD1315 0.96英寸128×64 I²C OLED
- AT24C02 EEPROM
- DS3231 RTC
- LYO3C DC12V/0.4A电磁锁

电磁锁必须通过MOSFET或合适的驱动模块控制，禁止由STM32 GPIO直接驱动。当前LYO3C的通电动作逻辑仍待实测。

## 项目文档

- [离线控制系统开发文档](STM32F103智能门锁离线控制系统开发文档.md)
- [硬件接线与接口定义文档](智能门锁硬件接线与接口定义文档.md)

## 分支说明

- `main`：经过验证、可演示或可发布的版本。
- `develop`：日常集成分支。
- `feature/*`：功能、缺陷、文档、测试和构建任务分支。

## 当前阶段

项目处于硬件基线确认和基础工程建立阶段。下一里程碑是在`feature/BUILD-001-create-stm32-project`分支建立CubeMX与Keil MDK最小工程，完成SWD下载、USART1日志和LED/GPIO验证。

## 安全与隐私

本仓库为公开仓库。禁止提交真实管理员密码、Wi-Fi凭据、访问令牌、私钥、指纹模板和其他个人敏感信息。
