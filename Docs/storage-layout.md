# AT24C02 V1 存储布局

物理容量256字节，页大小按8字节处理。所有地址由`storage_service`集中管理。

| 地址范围 | 长度 | 内容 |
|---|---:|---|
| `0x00-0x1F` | 32 | 配置副本A |
| `0x20-0x3F` | 32 | 配置副本B |
| `0x40-0x6F` | 48 | 8条RFID记录，每条6字节 |
| `0x70-0x8F` | 32 | 8条指纹映射，每条4字节 |
| `0x90-0xFF` | 112 | 保留，V1不得使用 |

## 配置记录（32字节）

| 字段 | 长度 |
|---|---:|
| magic `SDLK` | 4 |
| schema_version | 2 |
| record_size | 2 |
| sequence | 4 |
| user_pin（最多6位） | 6 |
| admin_pin（最多6位） | 6 |
| user_pin_len | 1 |
| admin_pin_len | 1 |
| failure_limit | 1 |
| unlock_seconds | 1 |
| lockout_seconds | 2 |
| CRC16-CCITT | 2 |

启动时读取两份记录，校验魔数、版本、长度、字段范围和CRC；选择序号较新的有效记录。保存时写入较旧副本，跨页自动拆分并ACK轮询。两份都无效时加载默认值，但只有显式保存时才写EEPROM。

RFID记录：`used + uid[4] + crc8`。当前RC522 V1仅接受4字节UID。
指纹记录：`used + template_id_le16 + crc8`，模板ID范围`0..299`。

## 已知安全限制

V1配置记录直接保存最多6位的普通PIN和管理员PIN，CRC只用于检错，不提供保密性。该取舍用于在256字节EEPROM内先完成掉电恢复闭环；毕业论文和答辩必须将其列为系统限制。后续版本应迁移到带随机盐的密码摘要，并提高存储格式版本，不能把CRC当作密码哈希。
