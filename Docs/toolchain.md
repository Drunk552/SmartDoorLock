# 工具链基线

| 项目 | 版本/配置 | 证据 |
|---|---|---|
| STM32CubeMX生成记录 | 6.15.0 | `SmartDoorLock.ioc` |
| STM32CubeF1 | 1.8.6 | `SmartDoorLock.ioc` |
| Keil MDK | 5.39.0.0 | 构建日志 |
| Arm Compiler | 6.21 | 构建日志，工程显式选择AC6 |
| Device Pack | Keil STM32F1xx DFP 2.4.1 | `.uvprojx` |
| MCU | STM32F103C8T6 | 64KB Flash，20KB SRAM |

阶段6完整重构建结果：0错误、22条警告。22条均来自未修改的ST HAL UART/SPI/DMA/FLASH/EXTI源文件；项目自有`Core/App/Bsp/Services`代码没有编译器警告。资源占用为Flash 31,036字节、RAM 3,424字节。

ST-Link固件版本需在首次连接板卡后补充，当前为`PENDING-HW`。
