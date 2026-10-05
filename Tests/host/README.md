# 主机服务测试

在仓库根目录使用MinGW GCC执行：

```powershell
gcc -std=c11 -Wall -Wextra -Werror `
  -IFirmware/SmartDoorLock/Services/Inc `
  Tests/host/test_services.c `
  Firmware/SmartDoorLock/Services/Src/lock_service.c `
  Firmware/SmartDoorLock/Services/Src/crc.c `
  Firmware/SmartDoorLock/Services/Src/storage_service.c `
  Firmware/SmartDoorLock/Services/Src/credential_service.c `
  Firmware/SmartDoorLock/Services/Src/as608_protocol.c `
  Firmware/SmartDoorLock/Services/Src/auth_service.c `
  -o Tests/host/test_services.exe

Tests/host/test_services.exe
```

覆盖内容：非阻塞自动上锁、非法开锁时长、tick回绕、CRC标准向量、配置双副本恢复、卡片/指纹映射、AS608数据包与错误校验、连续失败锁定和自动解除。
