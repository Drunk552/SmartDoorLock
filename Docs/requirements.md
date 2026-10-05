# SmartDoorLock 需求与追踪基线

> 适用硬件：STM32F103C8T6，64KB Flash，20KB SRAM
> 当前软件目标：阶段0至阶段6
> 证据状态：`AUTO-PASS`、`BENCH-PASS`、`REVIEW-PASS`、`PENDING-HW`、`BLOCKED`

## 需求清单

| 需求编号 | 阶段 | 可验证需求 | 验收标准 | 当前证据 |
|---|---:|---|---|---|
| REQ-IF-001 | 0 | 主控必须为STM32F103C8T6，PA13/PA14保留SWD | `.ioc`和代码未占用PA13/PA14；本地资料与照片一致 | REVIEW-PASS |
| REQ-PWR-001 | 0 | 逻辑器件统一使用3.3V逻辑，未知峰值模块不得默认由板载稳压器供电 | 电源限制、待测项和分级上电步骤已记录 | REVIEW-PASS / PENDING-HW |
| REQ-SAF-001 | 0-6 | 门锁输出上电后立即进入安全低电平，禁止GPIO直接驱动12V锁 | PA8先写低再配置输出；阶段0-6仅允许LED模拟 | REVIEW-PASS / PENDING-HW |
| REQ-MNT-001 | 1 | 工程可由固定工具链重复构建 | Keil MDK 5.39、Arm Compiler 6.21构建0错误；项目代码0警告，第三方HAL警告单独记录 | AUTO-PASS |
| REQ-FUN-001 | 1 | 上电提供LED、USART1日志和OLED启动状态 | PC13非阻塞心跳；115200 8N1日志；OLED显示启动/降级状态 | REVIEW-PASS / PENDING-HW |
| REQ-FUN-002 | 2 | 4x4键盘支持消抖、单次按键事件和密码输入 | 不使用长延时；正确密码请求开锁，错误/超时不得开锁 | REVIEW-PASS / PENDING-HW |
| REQ-FUN-003 | 2 | 开锁控制必须非阻塞并在5秒后自动上锁 | 主循环持续调度；用回绕安全的tick比较；PA8 LED模拟 | AUTO-PASS / PENDING-HW |
| REQ-DATA-001 | 3 | AT24C02保存带魔数、版本、序号和CRC的双份配置 | 损坏一份可恢复；两份都无效时加载安全默认值 | AUTO-PASS / PENDING-HW |
| REQ-FUN-004 | 3 | DS3231提供离线时间并检测无效时间 | I2C超时有界；BCD与范围检查；故障时系统降级运行 | REVIEW-PASS / PENDING-HW |
| REQ-FUN-005 | 4 | RC522读取4字节UID并支持最多8张授权卡 | 读卡超时有界；添加、删除、重复和满表路径明确 | AUTO-PASS / PENDING-HW |
| REQ-FUN-006 | 5 | AS608支持搜索、录入、删除和模板ID映射 | UART解析有长度/校验/超时检查；ID范围0-299 | AUTO-PASS / PENDING-HW |
| REQ-SEC-001 | 6 | 连续5次认证失败后锁定60秒 | 锁定期所有认证不得请求开锁；到期自动恢复 | AUTO-PASS / PENDING-HW |
| REQ-FUN-007 | 6 | 管理员通过独立密码进入菜单管理用户 | 菜单可改密码、管理卡和指纹，取消/超时返回待机 | REVIEW-PASS / PENDING-HW |
| REQ-SAF-002 | 6 | 主循环受独立看门狗保护 | IWDG约2秒；仅在主循环完成一轮调度后刷新 | REVIEW-PASS / PENDING-HW |

## 追踪矩阵

| 需求 | 设计/接口 | 代码 | 自动测试 | 实物测试 |
|---|---|---|---|---|
| REQ-IF-001 | `Docs/interfaces.md` | `.ioc`、`main.h` | Keil构建 | TC-HW-001 |
| REQ-SAF-001 | 锁服务单一所有者 | `bsp_board`、`lock_service` | TC-LOCK-001~003 | TC-HW-002 |
| REQ-FUN-001 | OLED分页刷新、心跳状态机 | `bsp_oled`、`app` | Keil构建 | TC-S1-001~004 |
| REQ-FUN-002 | 行轮询+时间消抖 | `bsp_keypad`、`app` | TC-KEY-001~004 | TC-S2-001~005 |
| REQ-FUN-003 | 非阻塞锁状态机 | `lock_service` | TC-LOCK-001~003 | TC-S2-006 |
| REQ-DATA-001 | `Docs/storage-layout.md` | `bsp_at24c02`、`storage_service` | TC-STG-001~006 | TC-S3-001~004 |
| REQ-FUN-004 | 有界I2C读取 | `bsp_ds3231` | TC-RTC-001~004 | TC-S3-005~007 |
| REQ-FUN-005 | SPI寄存器驱动与授权表 | `bsp_rc522`、`credential_service` | TC-CRED-001~006 | TC-S4-001~008 |
| REQ-FUN-006 | 异步包解析与操作状态机 | `bsp_as608`、`app` | TC-AS608-001~005 | TC-S5-001~010 |
| REQ-SEC-001 | 统一认证管理器 | `auth_service` | TC-AUTH-001~006 | TC-S6-001~006 |
| REQ-FUN-007 | 应用状态机 | `app` | 状态/边界评审 | TC-S6-007~012 |
| REQ-SAF-002 | 主循环末尾喂狗 | `iwdg.c`、`main.c` | Keil构建 | TC-S6-013 |

## 未关闭限制

- 所有实物项目当前均为`PENDING-HW`，不得据自动构建宣称板上通过。
- DS3231 HW-084可能带充电支路，CR2032条件下禁止5V供电，等待测量确认。
- AS608峰值电流和六线连接器定义未冻结。
- 真实电磁锁、驱动器和12V电源不在阶段0至阶段6的软件验收范围内。
- V1 PIN以明文保存在AT24C02中；这是已记录的原型限制，不能描述为产品级密码保护。
