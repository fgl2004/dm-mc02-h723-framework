# Chip Notes: STM32H723

## 1. Document Purpose

本文档用于记录 STM32H723 在本项目中的底层能力、关键外设、内存体系、时钟、Flash、Cache、DMA、异常调试和低功耗相关知识。

本文档不是 datasheet 的简单复制，而是围绕项目开发需要，整理：

```text
哪些芯片能力必须理解
哪些能力需要实验验证
哪些能力会影响软件架构
哪些能力会影响后续 Bootloader、协议、诊断、安全和算法系统
```

---

## 2. MCU Basic Information

目标芯片：

```text
STM32H723VGT6
```

核心能力摘要：

```text
Core: ARM Cortex-M7
Frequency: up to 550 MHz
Flash: up to 1 MB
RAM: up to 564 KB
Peripherals: UART, SPI, I2C, FDCAN, USB, ADC, Timer, DMA, etc.
```

具体参数以 ST 官方 Datasheet、Reference Manual 和 CubeMX 为准。

---

## 3. Core

需要重点理解：

```text
ARM Cortex-M7
I-Cache
D-Cache
MPU
FPU
NVIC
SysTick
Fault Exception
DWT Cycle Counter
```

项目中的用途：

```text
1. 用 DWT 统计函数耗时
2. 用 SysTick 建立 1ms 系统 tick
3. 用 NVIC 管理中断优先级
4. 用 Fault Handler 捕获 HardFault / BusFault / MemManage
5. 用 Cache 提升性能，同时处理 DMA 一致性问题
```

---

## 4. Clock Tree

需要关注：

```text
SYSCLK
HCLK
APB1
APB2
APB3
APB4
PLL1
PLL2
PLL3
UART kernel clock
FDCAN kernel clock
ADC clock
Timer clock
USB clock
```

Stage 1 实验：

```text
1. 配置基础主频
2. UART printf 稳定输出
3. Timer GPIO toggle 验证定时频率
4. 读取并打印当前时钟配置
```

设计原则：

```text
先稳定，再提频。
先证明 UART、Timer、SysTick 正常，再探索高主频。
```

---

## 5. Memory Map

STM32H7 内存体系复杂，需要重点研究：

```text
Flash
ITCM
DTCM
AXI SRAM
AHB SRAM
Backup SRAM
Peripheral memory
```

项目中初步规划：

```text
Flash:
  Bootloader
  Application
  Parameter Area
  Blackbox / Flags

RAM:
  Stack / Heap
  DMA Buffer
  RingBuffer
  MessageQueue
  Log Buffer
  Trace Buffer
  Algorithm State
  IMU Data Buffer
```

待验证：

```text
1. DMA 能否访问 DTCM
2. 哪些 SRAM 区域适合 DMA buffer
3. 哪些区域受 D-Cache 影响
4. 是否需要 MPU 配置 Non-cacheable 区域
5. 链接脚本如何规划不同段
```

---

## 6. Flash Layout

初步设想：

```text
0x0800_0000 - Bootloader
Application Area
Parameter A
Parameter B
Blackbox / Flag Area
```

注意：

```text
具体地址必须根据 STM32H723VGT6 的实际 Flash 大小、Bank、Sector/Page 结构对齐。
```

Bootloader 阶段需要确认：

```text
1. Flash 总大小
2. 擦除粒度
3. 写入粒度
4. Bank 结构
5. Option Bytes
6. 读保护配置
7. App 起始地址
8. VTOR 设置
```

---

## 7. Cache and MPU

H7 的重要问题：

```text
D-Cache 和 DMA 一致性
```

典型风险：

```text
DMA 已经写入内存，但 CPU 读到旧 Cache 数据
CPU 改了 TX buffer，但 DMA 发送旧数据
```

实验计划：

```text
1. 关闭 D-Cache，UART DMA 接收测试
2. 开启 D-Cache，不做 invalidate，观察异常
3. RX DMA 后执行 cache invalidate
4. TX DMA 前执行 cache clean
5. DMA buffer 地址 cache line 对齐
6. 尝试 MPU 配置 non-cacheable buffer
```

代码规范：

```text
1. 所有 DMA buffer 必须明确所在内存区域
2. 所有 DMA buffer 必须明确 Cache 策略
3. Platform 层封装 Cache clean / invalidate
4. 上层业务代码不直接处理 Cache 细节
```

---

## 8. DMA

项目中 DMA 用途：

```text
UART RX DMA
UART TX DMA
SPI DMA
ADC DMA
LCD DMA, if available
FDCAN generally not DMA style, depends on peripheral architecture
```

DMA 设计注意：

```text
1. Buffer 对齐
2. Cache 一致性
3. 中断优先级
4. 半传输 / 传输完成
5. IDLE line detection
6. 循环模式
7. 错误中断
```

Stage 2 重点：

```text
UART DMA + IDLE + RingBuffer
```

---

## 9. UART

项目用途：

```text
1. printf log
2. PC Tool command channel
3. Firmware upgrade channel
4. Security authentication test channel
```

需要确认：

```text
1. UART instance
2. TX/RX pins
3. Clock source
4. DMA channel
5. IDLE interrupt
6. Baudrate error
7. FIFO configuration
```

第一阶段：

```text
Blocking printf
```

第二阶段：

```text
DMA RX + DMA TX + Protocol
```

---

## 10. FDCAN

项目用途：

```text
1. Robot / motor control communication
2. FDCAN diagnostics
3. CAN stress test
4. Future upgrade channel extension
```

需要确认：

```text
1. FDCAN instance
2. Kernel clock
3. Nominal bit timing
4. Data bit timing, if CAN FD enabled
5. Filter configuration
6. RX FIFO
7. TX Buffer / TX FIFO
8. Error states
9. Bus-off recovery
```

诊断计数：

```text
fdcan_rx_count
fdcan_tx_count
fdcan_error_count
fdcan_bus_off_count
fdcan_rx_lost_count
fdcan_tx_fail_count
```

---

## 11. SPI / I2C

项目用途：

```text
IMU
LCD
Possible external devices
```

待确认：

```text
1. BMI088 uses SPI or I2C
2. LCD interface
3. SPI clock rate
4. DMA usage
5. GPIO alternate function
6. Chip select timing
```

---

## 12. ADC

项目用途：

```text
1. Hardware diagnostics
2. Voltage measurement, if board supports
3. Internal temperature / Vrefint, if configured
4. Future analog input test
```

需要研究：

```text
ADC clock
Sampling time
Resolution
Calibration
Vrefint
Internal temperature sensor
DMA sampling
```

---

## 13. Timer / PWM

项目用途：

```text
1. Timebase
2. GPIO frequency verification
3. PWM test
4. Future control algorithm output
5. Profiling signal toggle
```

Bring-up 测试：

```text
1. Timer 1Hz LED toggle
2. Timer 1kHz GPIO output
3. PWM duty cycle test
4. 用逻辑分析仪或示波器验证周期
```

---

## 14. RNG / Crypto / Hash

安全模块需要确认：

```text
1. STM32H723VGT6 是否具备硬件 RNG
2. 是否具备 HASH 硬件
3. 是否具备 CRYP 硬件
4. 是否需要纯软件实现 SHA/HMAC/ECDSA
```

项目初期策略：

```text
Crypto Service 先做抽象接口。
底层可以先用软件实现。
如果芯片支持硬件加速，再在 Platform 层替换。
```

安全用途：

```text
HMAC dangerous command authentication
Parameter HMAC
Firmware signature verification
Random nonce
Replay protection
```

---

## 15. Low Power

需要研究：

```text
Sleep
Stop
Standby
Wakeup source
Clock restore
UART restore
FDCAN restore
SysTick behavior
GPIO state
```

项目目标：

```text
不是追求极限低功耗，而是理解低功耗进入、唤醒和外设恢复流程。
```

---

## 16. Debug and Fault

需要实现：

```text
HardFault_Handler
MemManage_Handler
BusFault_Handler
UsageFault_Handler
Fault stack frame decode
CFSR
HFSR
BFAR
MMFAR
PC
LR
xPSR
Reset Reason
```

调试工具：

```text
ST-Link / J-Link
GDB
Map file
addr2line
DWT cycle counter
Logic analyzer
Oscilloscope
```

---

## 17. Immediate Experiments

Stage 1 必做实验：

```text
1. LED GPIO toggle
2. UART printf
3. SysTick 1ms
4. Timer GPIO toggle
5. Reset reason print
6. HardFault trigger and capture
7. DWT cycle counter test
8. Clock info print
```

Stage 2 必做实验：

```text
1. UART DMA RX
2. UART IDLE interrupt
3. RingBuffer
4. D-Cache on/off comparison
5. Cache clean/invalidate verification
```

---

## 18. Questions To Verify

当前待确认：

```text
1. STM32H723VGT6 的实际 Flash / SRAM 映射
2. DTCM 是否可被当前 DMA 访问
3. UART 使用哪个实例
4. FDCAN 使用哪个实例
5. BMI088 接口类型
6. LCD 接口类型
7. 是否具备硬件 RNG / HASH / CRYP
8. Flash 擦除粒度
9. Bootloader 分区如何对齐
10. Option Bytes / RDP 配置策略
11. Clock tree 第一版配置多少 MHz 更稳妥
```

---

## 19. Update Log

| Date | Update                     |
| ---- | -------------------------- |
| TBD  | Initial chip notes created |
