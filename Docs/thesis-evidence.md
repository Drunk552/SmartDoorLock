# 毕业论文证据索引

| 主题 | 当前材料 | 可信状态 |
|---|---|---|
| 代码可读性与设计证据 | 自有源文件中文 Doxygen 风格注释 | REVIEW-PASS：覆盖分层职责、状态机、协议/存储边界、超时、中断最小化与门锁安全约束；不代表实物验证通过 |
| 需求与总体架构 | `Docs/requirements.md`、`Docs/interfaces.md` | 已设计，REVIEW-PASS |
| 核心板接口与电源 | 硬件文档、本地原理图、实物照片 | 部分确认，PENDING-HW |
| 软件分层和状态机 | `App/Services/Bsp`实现 | 已实现，REVIEW-PASS，PENDING-HW |
| 键盘消抖 | 一行一周期扫描和20ms稳定时间判定 | 已实现，REVIEW-PASS，PENDING-HW |
| 统一认证接口 | `auth_service`与应用统一入口 | 已实现，AUTO-PASS，PENDING-HW |
| EEPROM与CRC | 双副本配置、CRC16、记录CRC8 | 已实现，AUTO-PASS，PENDING-HW |
| 非阻塞门锁控制 | `lock_service` | 已实现，AUTO-PASS，PENDING-HW |
| 连续失败锁定 | `auth_service`，默认5次/60秒 | 已实现，AUTO-PASS，PENDING-HW |
| 看门狗和异常恢复 | 主循环末尾刷新IWDG、外设降级 | 已实现，REVIEW-PASS，PENDING-HW |
| 功能和稳定性数据 | `Docs/test-records.md` | 尚无BENCH-PASS，不得写入实验结论 |

论文中必须继续区分：已实现并验证、已实现但待实物验证、接口预留、未来工作。

当前可用于论文的自动证据：主机服务测试通过；Keil完整重构建0错误；Flash 31,036字节（47.4%）、RAM 3,424字节（16.7%）。这些数据只能证明构建与主机逻辑，不能替代实物功能、稳定性和时序测试。

必须在“系统限制”中披露：V1明文保存PIN、只支持RC522四字节UID、最多8张卡/8条指纹映射、蜂鸣器有效电平与AS608线序尚待实测、阶段7真实锁驱动尚未实现。
