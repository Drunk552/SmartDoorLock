# SmartDoorLock 接口与软件边界

## V1引脚分配

| 功能       | MCU引脚      | 配置                             | 安全/约束                           |
| ---------- | ------------ | -------------------------------- | ----------------------------------- |
| 板载LED    | PC13         | 推挽输出，低有效                 | 仅状态指示                          |
| 锁模拟输出 | PA8          | 推挽输出，默认低                 | 阶段0-6只接LED+限流电阻             |
| 调试串口   | PA9/PA10     | USART1 115200 8N1                | 所有发送有超时                      |
| AS608      | PA2/PA3      | USART2 57600 8N1                 | TX/RX交叉，模块3.3V，电平待测       |
| RC522      | PA4~PA7、PB0 | SPI1 Mode 0，NSS软件控制         | PA4 CS默认高，PB0 RST默认低后释放   |
| I2C总线    | PB6/PB7      | I2C1 100kHz                      | OLED/AT24C02/DS3231共享，先测上拉   |
| 蜂鸣器     | PB1          | 推挽输出，默认低，软件暂按高有效 | 型号/有效电平未确认，确认前可不安装 |
| 键盘行     | PB8~PB11     | 推挽输出，默认高                 | 每次只拉低一行                      |
| 键盘列     | PB12~PB15    | 输入上拉                         | 排线顺序C4,C3,C2,C1,R1,R2,R3,R4     |
| SWD        | PA13/PA14    | Serial Wire                      | 不得复用                            |

## 分层依赖

```text
App/app.c
  +-> Services/auth_service, lock_service, storage_service,
  |            credential_service, as608_protocol
  +-> Bsp/bsp_keypad, bsp_oled, bsp_ds3231, bsp_at24c02,
               bsp_rc522, bsp_as608, bsp_board
                    -> STM32 HAL
```

- BSP只提供设备操作和状态，不决定是否开锁。
- `auth_service`是认证结果与连续失败策略的唯一所有者。
- `lock_service`是PA8锁模拟输出的唯一运行期所有者。
- `storage_layout.h`集中定义AT24C02地址；服务通过后端接口读写，BSP不理解数据布局。
- ISR不执行菜单、存储或认证业务。

## 时间与超时

- 统一用`(uint32_t)(now - start) >= interval`处理`HAL_GetTick()`回绕。
- I2C单次HAL调用上限20ms；EEPROM ACK轮询总上限20ms。
- SPI单次HAL调用上限10ms；RC522命令轮询上限30ms。
- UART发送上限20ms；AS608由USART2单字节中断写入64字节静态环形队列，主循环每轮最多解析24字节，单命令总超时2秒。
- 密码输入30秒超时；管理员菜单60秒超时；开锁5秒；失败锁定60秒。
- IWDG按约2秒配置，只在`App_Task()`完成一轮调度后刷新。
