# 阶段0～6完成报告

日期：2026-10-05
结论：软件实现、主机测试、代码评审和Keil构建闭环完成；所有实物功能保持`PENDING-HW`。

## 已实现范围

- 阶段0：冻结STM32F103C8T6阶段0～6引脚，PA13/PA14保留SWD；PA8仅作为LED锁模拟输出并在GPIO初始化早期拉低。
- 阶段1：Keil AC6工程、PC13心跳、USART1日志、SSD1315/SSD1306兼容OLED分页刷新。
- 阶段2：4×4键盘轮询与20ms消抖、PIN输入、非阻塞5秒自动上锁。
- 阶段3：DS3231有效性检查、AT24C02分页写和ACK轮询、配置魔数/版本/双副本/序号/CRC16恢复。
- 阶段4：RC522 SPI驱动、4字节UID、最多8张卡的添加/删除/授权与CRC8记录。
- 阶段5：AS608口令验证、图像/特征/搜索、两次采集录入、删除和ID映射；USART2中断环形队列、包长度/地址/校验和及2秒超时检查。
- 阶段6：独立管理员PIN和菜单、PIN/卡/指纹管理、统一失败计数、默认5次失败锁定60秒、蜂鸣提示和约2秒IWDG。

## 自动证据

| 证据 | 状态 | 结果 |
|---|---|---|
| 主机服务测试 | AUTO-PASS | `-Wall -Wextra -Werror`通过 |
| Keil AC6完整重构建 | AUTO-PASS | 0错误；项目代码0警告 |
| 第三方HAL诊断 | REVIEW-PASS | 22条，均来自未修改的ST HAL源文件 |
| Flash | AUTO-PASS | 31,036/65,536字节，47.4% |
| SRAM | AUTO-PASS | 3,424/20,480字节，16.7% |
| 动态内存和长延时扫描 | REVIEW-PASS | 未使用动态内存或`HAL_Delay` |
| SWD和锁安全输出评审 | REVIEW-PASS | PA13/PA14保留；PA8配置输出前先写低 |

主机测试覆盖自动上锁、非法时长、tick回绕、CRC标准向量、配置损坏恢复、凭据增删、AS608编码/解析/坏校验和、连续失败锁定和到期恢复。

## 尚未通过的项目

- OLED、键盘、RC522、AS608、DS3231、AT24C02、蜂鸣器、PA8 LED和IWDG的板上功能均为`PENDING-HW`。
- DS3231 D2/R4是否形成CR2032充电支路、AS608连接器线序/TXD高电平/峰值电流、蜂鸣器类型与有效电平仍待实测。
- 真实LYO3C及其MOSFET/继电器驱动未实现、未接入；阶段7和100次稳定性测试尚未开始。
- V1以明文保存PIN；CRC仅检错。这是论文必须披露的原型安全限制。

## 交付入口

- 固件工程：`Firmware/SmartDoorLock/MDK-ARM/SmartDoorLock.uvprojx`
- 烧录文件：`Firmware/SmartDoorLock/MDK-ARM/SmartDoorLock/SmartDoorLock.hex`
- 接口基线：`Docs/interfaces.md`
- 存储布局：`Docs/storage-layout.md`
- 需求追踪：`Docs/requirements.md`
- 测试记录：`Docs/test-records.md`
- 台架步骤：`Docs/stage0-6-bench-checklist.md`

下一安全检查点：用户按台架清单完成阶段0～6验证并返回日志、照片和测量值；在这些证据评审完成前不进入阶段7真实电磁锁驱动。
