# Project Overview

## 1. Project Name

```text
DM-MC02 H723 Embedded Framework
```

中文名称：

```text
基于达妙 DM-MC02 / STM32H723 的嵌入式可靠通信、诊断、安全与算法框架
```

本项目基于达妙 DM-MC02 开发板与 STM32H723 MCU，构建一个面向嵌入式工程训练的综合性系统框架。

项目并不以单一外设 Demo 为目标，而是希望通过一块真实开发板，系统性探索 MCU 底层能力、软件架构设计、硬件物理约束、算法系统、安全机制、调试诊断和工程化协作流程。

---

## 2. Project Background

嵌入式开发并不只是“写几个外设驱动”或者“让功能跑起来”。真实企业项目通常同时涉及：

```text
硬件资源理解
芯片底层机制
通信协议设计
状态机设计
中断与并发
缓冲区和数据流
非阻塞调度
参数管理
故障诊断
Bootloader 升级
安全认证
低功耗管理
自动化测试
产测与自检
现场问题追溯
```

很多项目的问题并不出现在正常流程，而是出现在异常路径：

```text
通信半包、粘包、丢包、重放
DMA 与 Cache 不一致
Flash 写入中断
升级失败
参数损坏
任务卡死
HardFault
低功耗唤醒失败
电源波动
外设异常
危险命令未授权执行
```

因此，本项目的核心思想是：

> 不只追求功能跑通，而是构建一个可诊断、可恢复、可升级、可测试、可扩展、可移植的 MCU 系统框架。

---

## 3. Target Board

目标开发板：

```text
Damiao DM-MC02
```

目标 MCU：

```text
STM32H723VGT6
```

该平台适合探索以下能力：

```text
Cortex-M7 高性能内核
STM32H7 时钟树
I-Cache / D-Cache
DMA 与 Cache 一致性
Flash 分区与 Bootloader
UART / FDCAN 通信
IMU 数据采集
LCD 状态显示
Timer / PWM
ADC
低功耗模式
HardFault / BusFault / MemManage
安全与固件签名机制
```

本项目会把 DM-MC02 视为一个小型机器人/工业控制器训练平台，而不是普通点灯板。

---

## 4. Project Positioning

本项目定位为：

> 一个基于 STM32H723 的企业级 MCU 设备框架训练项目。

它包含三条主线。

### 4.1 Software Architecture

软件架构主线关注：

```text
分层架构
Manager 化模块设计
状态机
通信协议
缓冲区
非阻塞调度
诊断系统
参数系统
Bootloader
安全认证
自动化测试
```

目标是训练如何组织一个中大型 MCU 项目，使复杂功能能够被清晰拆分、稳定扩展和长期维护。

### 4.2 Hardware and Physical Thinking

硬件物理主线关注：

```text
电源
时钟
信号
GPIO
DMA
Cache
Flash
复位
调试接口
FDCAN 物理层
IMU 采样
LCD 接口
低功耗测量
```

目标是理解软件运行背后的物理约束，形成“代码行为必须能被硬件测量验证”的工程习惯。

### 4.3 Algorithm Thinking

算法主线关注：

```text
采样
滤波
状态估计
PID / 控制算法
故障检测
协议解析
调度策略
缓冲区策略
重试恢复
安全认证
```

目标是训练把复杂变化压缩成可计算规则，并让系统通过反馈机制朝目标收敛。

---

## 5. Core Design Philosophy

本项目遵循以下设计思想。

### 5.1 Data Plane and Control Plane Separation

高频数据路径保持短、快、确定。

例如：

```text
UART DMA → RX RingBuffer → Protocol Parser
FDCAN RX → CAN Message Queue
IMU Sample → Filter Algorithm
Timer ISR → Lightweight Event
```

控制面负责状态、配置、诊断、权限、升级和策略。

例如：

```text
Parameter Manager
Diagnostic Manager
Upgrade Manager
Security Manager
Power Manager
Algorithm Manager
```

基本原则：

> 数据面追求效率，控制面追求可维护性和可观测性。

---

### 5.2 Mechanism and Policy Separation

机制表示系统能做什么。

策略表示什么时候做、如何选择、失败后怎么办。

例如：

```text
RingBuffer 是机制
Buffer Manager 的溢出处理是策略

Flash Write 是机制
Parameter Manager 的双备份保存是策略

UART Send 是机制
Protocol Manager 的 ACK/NACK、重传、去重是策略

Sleep Mode 是机制
Power Manager 的进入、唤醒、恢复流程是策略
```

机制尽量通用，策略集中收口。

---

### 5.3 Observable by Design

所有关键模块都应该能够回答：

```text
当前状态是什么？
最近一次错误是什么？
错误发生了多少次？
最大耗时是多少？
缓冲区最大水位是多少？
是否发生过溢出？
是否发生过复位？
是否发生过认证失败？
故障前发生了什么？
```

系统不只要运行，还要能解释自己为什么这样运行。

---

### 5.4 Failure-Oriented Design

项目从一开始就考虑异常路径：

```text
非法命令
半包
粘包
CRC 错误
HMAC 失败
重放攻击
缓冲区溢出
Flash 写失败
升级中断
App 启动失败
HardFault
Watchdog Reset
低功耗唤醒失败
```

设计目标不是让系统永远不出错，而是：

> 出错后能够发现、拒绝、隔离、恢复、记录，并避免局部错误扩散成系统崩溃。

---

### 5.5 Portability by Abstraction

项目不希望上层代码直接绑定 STM32 HAL。

上层模块尽量通过 Platform Abstraction Layer 调用底层能力：

```text
platform_uart
platform_fdcan
platform_flash
platform_time
platform_gpio
platform_power
platform_watchdog
platform_rng
```

这样未来可以迁移到其他 STM32、GD32、CH32、ESP32 或 PC 仿真环境。

---

## 6. System Architecture Overview

整体架构如下：

```text
PC Tool / Test Tool
    ↓
Protocol / Command Layer
    ↓
Application Layer
    ↓
Service / Manager Layer
    ↓
Middleware Layer
    ↓
Platform Abstraction Layer
    ↓
BSP / HAL Layer
    ↓
Hardware
```

---

## 7. Layer Responsibilities

### 7.1 PC Tool / Test Tool

PC 工具负责：

```text
串口通信
FDCAN 通信测试
命令行控制
协议组包/解包
固件升级
诊断查询
参数读写
安全认证
Chaos 测试
测试报告生成
```

计划实现：

```text
h7tool ping
h7tool info
h7tool status
h7tool param read/write
h7tool upgrade
h7tool diag
h7tool security
h7tool chaos
h7tool report
```

---

### 7.2 Protocol / Command Layer

负责设备通信入口。

主要能力：

```text
Frame Parser
CRC16 / CRC32
ACK / NACK
SEQ
Timeout
Retry
Duplicate Detection
Replay Protection
Command Dispatch
Error Code
```

---

### 7.3 Application Layer

负责业务流程组织。

例如：

```text
设备运行流程
产测流程
算法演示流程
升级入口流程
系统初始化流程
```

Application Layer 不应该直接操作底层硬件。

---

### 7.4 Service / Manager Layer

负责系统能力和策略收口。

计划模块：

```text
Device Manager
Command Manager
Protocol Manager
Parameter Manager
Diagnostic Manager
Buffer Manager
Health Manager
Watchdog Manager
Upgrade Manager
Power Manager
Security Manager
Algorithm Manager
Factory Manager
Chaos Manager
```

---

### 7.5 Middleware Layer

负责通用机制。

计划模块：

```text
RingBuffer
MessageQueue
CRC16 / CRC32
SoftTimer
EventBus
ProtocolFrame
FileTransfer
LogCore
TraceBuffer
MemoryPool
```

---

### 7.6 Platform Abstraction Layer

负责屏蔽芯片和外设差异。

计划模块：

```text
platform_uart
platform_fdcan
platform_flash
platform_time
platform_gpio
platform_power
platform_watchdog
platform_rng
platform_interrupt
platform_imu
platform_lcd
```

---

### 7.7 BSP / HAL Layer

负责具体板级适配。

包括：

```text
DM-MC02 板级引脚
STM32H723 时钟配置
UART/FDCAN/SPI/I2C/DMA 配置
LCD 驱动
BMI088 IMU 驱动适配
调试接口
启动文件
链接脚本
```

---

## 8. Planned Main Modules

### 8.1 Protocol Manager

目标：

```text
构建可靠串口/FDCAN 通信协议。
```

核心内容：

```text
帧格式
CRC
ACK/NACK
SEQ
超时
重传
去重
错误码
安全认证扩展
```

---

### 8.2 Command Manager

目标：

```text
将协议命令和业务处理解耦。
```

核心内容：

```text
命令注册
参数校验
权限检查
状态检查
命令分发
响应封装
命令耗时统计
```

---

### 8.3 Device Manager

目标：

```text
管理设备整体状态。
```

状态示例：

```text
POWER_ON
INIT
IDLE
RUNNING
ERROR
RECOVERY
FACTORY_MODE
UPGRADING
LOW_POWER
```

---

### 8.4 Parameter Manager

目标：

```text
构建设备参数系统。
```

核心内容：

```text
默认参数
参数表
参数合法性检查
Flash A/B 双备份
CRC32
HMAC 预留
version
sequence
恢复默认
导入导出
```

---

### 8.5 Diagnostic Manager

目标：

```text
建立系统可观测性出口。
```

核心内容：

```text
错误码
计数器
任务统计
通信统计
Buffer 统计
Reset Reason
HardFault Snapshot
Trace
Log
Blackbox
```

---

### 8.6 Buffer Manager

目标：

```text
统一观察和管理系统中的关键缓冲区。
```

缓冲区包括：

```text
UART RX RingBuffer
UART TX Queue
FDCAN RX Queue
FDCAN TX Queue
Message Queue
Log Buffer
Trace Buffer
Upgrade Buffer
IMU Data Buffer
```

---

### 8.7 Health Manager and Watchdog Manager

目标：

```text
建立系统健康监控和看门狗喂狗策略。
```

原则：

> 只有关键模块都健康时，才允许喂狗。

监控对象：

```text
Main Loop
Protocol Task
Command Task
IMU Task
FDCAN Task
Log Task
Diagnostic Task
State Machine
```

---

### 8.8 Upgrade Manager and Bootloader

目标：

```text
实现可靠固件升级。
```

核心内容：

```text
Bootloader
App Jump
Vector Table Relocation
Firmware Header
CRC/SHA
App Valid
App Confirmed
Upgrade Timeout
Fail-safe
Signature Verification
```

---

### 8.9 Security Manager

目标：

```text
建立设备可信边界。
```

核心内容：

```text
HMAC dangerous command authentication
Challenge-Response
Replay Protection
Parameter HMAC
Firmware Signature
Security Counter
Security Event Log
```

---

### 8.10 Algorithm Manager

目标：

```text
构建可配置、可观测、可测试的算法层。
```

第一阶段算法：

```text
Low-pass Filter
Median Filter
Moving Average
Bias Estimation
Static Detection
Fault Detection
```

后续扩展：

```text
Complementary Filter
PID
State Estimation
Data Replay Test
```

---

### 8.11 Chaos Manager

目标：

```text
通过异常注入验证系统鲁棒性。
```

异常类型：

```text
CRC Error
Half Packet
Sticky Packet
Garbage Bytes
Replay Frame
Buffer Overflow
Task Delay
Flash Write Fail
Upgrade Interrupt
HardFault Trigger
Watchdog Trigger
IMU Data Fault
FDCAN Fault
```

---

## 9. Development Roadmap

### Stage 0: Repository and Documentation

目标：

```text
建立项目仓库、目录结构和文档体系。
```

任务：

```text
创建 GitHub 仓库
配置 SSH 推送
建立 README
建立 docs 目录
建立 firmware/app 和 firmware/bootloader 目录
建立 pc_tool、tools、scripts、tests、release 目录
```

交付：

```text
README.md
docs/00_project_overview.md
docs/01_board_notes.md
docs/02_chip_notes.md
docs/03_software_architecture.md
```

---

### Stage 1: Board Bring-up

目标：

```text
验证开发板基础运行能力。
```

任务：

```text
CubeMX 创建 STM32H723VGT6 工程
配置时钟树
LED/GPIO 测试
UART printf
SysTick 1ms
Timer GPIO toggle
Reset Reason
HardFault Handler
Git 提交基础工程
```

验收标准：

```text
板子稳定启动
串口能够输出日志
定时器周期可测量
复位原因可读取
HardFault 能进入自定义 handler
```

---

### Stage 2: UART Reliable Protocol

目标：

```text
建立 PC 与 MCU 的可靠控制通道。
```

任务：

```text
UART DMA + IDLE
D-Cache 处理
RX RingBuffer
Frame Parser
CRC16
PING
GET_VERSION
GET_STATUS
Python CLI
```

验收标准：

```text
连续 PING 10000 次无异常
错误 CRC 能被拒绝
半包/粘包可恢复
Buffer 统计可查询
```

---

### Stage 3: Diagnostic Framework

目标：

```text
建立系统可观测性骨架。
```

任务：

```text
Device Manager
Command Manager
Diagnostic Manager
Buffer Manager
Log / Trace
Health Manager
GET_COUNTERS
GET_TRACE
GET_BUFFER_STATUS
```

验收标准：

```text
PC 可以查询系统状态
系统错误有计数
Buffer 高水位可查询
状态切换有 Trace
```

---

### Stage 4: IMU and Algorithm Loop

目标：

```text
建立物理数据采集与算法闭环。
```

任务：

```text
BMI088 Driver
IMU Timestamp
Low-pass Filter
Median Filter
Bias Estimation
Static Detection
Algorithm Stats
LCD/Serial Output
```

验收标准：

```text
IMU 数据稳定输出
滤波效果可观察
算法耗时可统计
异常数据可检测
```

---

### Stage 5: FDCAN Communication

目标：

```text
建立 FDCAN 通信能力。
```

任务：

```text
FDCAN Init
RX FIFO
TX Queue
CAN Message Queue
FDCAN Stats
Bus-off Detection
CAN Stress Test
```

验收标准：

```text
能收发 CAN 帧
错误状态可查询
高频收发不死机
总线异常可记录
```

---

### Stage 6: Parameter and Flash System

目标：

```text
建立参数管理和非易失存储机制。
```

任务：

```text
Parameter Table
Default Parameter
Flash A/B Backup
CRC32
Version
Sequence
Parameter Import/Export
```

验收标准：

```text
参数可读写
参数可保存
写入中断不导致参数全坏
CRC 错误可恢复默认或备份
```

---

### Stage 7: Bootloader and Upgrade

目标：

```text
实现可升级、不变砖的固件系统。
```

任务：

```text
Bootloader Project
App Project
Flash Layout
Firmware Header
App Jump
VTOR
CRC/SHA Check
App Valid
App Confirmed
Serial Upgrade
```

验收标准：

```text
可升级 App
错误固件不运行
升级中断不变砖
App 未 confirmed 不永久生效
```

---

### Stage 8: Security

目标：

```text
建立基础安全可信机制。
```

任务：

```text
HMAC Dangerous Command Auth
Challenge-Response
Replay Protection
Parameter HMAC
Firmware Signature
Security Log
```

验收标准：

```text
未认证危险命令被拒绝
重放命令被拒绝
参数篡改可发现
未签名固件不能运行
```

---

### Stage 9: Low Power and Hardware Diagnostics

目标：

```text
探索 STM32H723 和 DM-MC02 的硬件状态与低功耗能力。
```

任务：

```text
Power Manager
Sleep
Stop
Wakeup Reason
Clock Restore
UART Restore
FDCAN Restore
LCD/IMU Power Control
Hardware Diagnostic Commands
```

验收标准：

```text
能进入并唤醒
唤醒后通信恢复
功耗变化可解释
硬件状态可查询
```

---

### Stage 10: Chaos and Automated Test

目标：

```text
通过异常注入验证系统鲁棒性。
```

任务：

```text
Protocol Fuzz Test
Buffer Stress Test
Upgrade Interrupt Test
Flash Fault Injection
HardFault Test
Watchdog Test
Security Test
Automated Report
```

验收标准：

```text
异常输入不导致未知死机
错误能被记录
系统能恢复或进入安全状态
PC 能生成测试报告
```

---

## 10. Success Criteria

项目最终希望达到以下标准：

```text
1. 系统具备清晰分层架构
2. 通信协议可靠且可测试
3. 状态机明确且异常路径完整
4. Buffer、任务、错误、复位均可观测
5. 参数和升级流程具有掉电保护
6. 固件升级不易变砖
7. 危险命令具有认证机制
8. IMU 数据和算法链路可运行、可观测
9. FDCAN 通信稳定可诊断
10. 支持 Chaos 测试和自动化测试报告
11. Platform 层使项目具备迁移潜力
```

---

## 11. What This Project Is Not

本项目不是：

```text
普通点灯 Demo
单一外设驱动 Demo
只会跑通功能的裸机工程
只追求复杂架构而没有验证的空壳
```

本项目希望避免：

```text
main.c 越写越大
状态散落在全局变量里
错误没有记录
协议不可测试
升级失败变砖
参数损坏无法恢复
硬件问题只能猜
软件模块强绑定 HAL
```

---

## 12. Development Principles

后续开发遵循以下原则：

```text
小步提交
每个阶段都有可验证目标
每个模块都有状态和统计
关键路径不阻塞
中断中少做事
DMA Buffer 明确 Cache 策略
错误必须有错误码
危险命令必须有权限控制
每个功能都要能被 PC Tool 测试
文档和代码同步推进
```

---

## 13. Current Status

当前阶段：

```text
Stage 0: Repository and Documentation
```

已完成：

```text
GitHub 仓库创建
SSH 推送配置
项目目录结构创建
README 初稿
GitHub 首次推送问题复盘
```

下一步：

```text
完善 board_notes
完善 chip_notes
完善 software_architecture
开始 Board Bring-up
```

---

## 14. Next Actions

近期任务：

```text
1. 完成 docs/01_board_notes.md
2. 完成 docs/02_chip_notes.md
3. 完成 docs/03_software_architecture.md
4. CubeMX 创建 STM32H723VGT6 基础工程
5. LED / UART / SysTick / Timer / Reset Reason bring-up
6. 提交 feature/board-bringup 分支
```

---

## 15. Long-Term Vision

本项目长期目标是形成一个可复用的 MCU 设备框架。

它应当具备：

```text
可靠通信能力
系统诊断能力
故障恢复能力
固件升级能力
安全认证能力
算法运行能力
自动化测试能力
高移植性
```

最终希望通过该项目证明：

> 嵌入式工程能力不只是会写外设驱动，而是能理解一个设备从启动、运行、通信、配置、诊断、升级、安全、测试到维护的完整闭环。

```
```
