# Software Architecture

## 1. Document Purpose

本文档用于描述 `DM-MC02 H723 Embedded Framework` 的软件架构设计。

本文档重点回答：

```text
1. 项目为什么要分层
2. 每一层负责什么
3. Manager 模块的边界是什么
4. 数据面和控制面如何分离
5. Middleware、Service、Platform、BSP 如何区分
6. 后续模块如何持续扩展
```

---

## 2. Architecture Principle

本项目不是简单外设 Demo，而是一个可长期演进的 MCU 系统框架。

核心设计原则：

```text
分层清晰
职责单一
状态收口
错误可观测
关键路径非阻塞
高频路径少抽象
控制路径可管理
硬件依赖下沉到 Platform / BSP
```

---

## 3. Layered Architecture

整体架构：

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

## 4. PC Tool / Test Tool

PC Tool 是整个项目的重要组成部分，不只是辅助工具。

职责：

```text
协议通信
命令发送
参数读写
诊断查询
固件升级
安全认证
异常注入
自动化测试
测试报告生成
```

计划命令：

```text
h7tool ping
h7tool info
h7tool status
h7tool param get/set
h7tool diag
h7tool buffer
h7tool upgrade
h7tool security
h7tool chaos
h7tool report
```

设计目标：

```text
所有 MCU 关键能力都应该能通过 PC Tool 验证。
```

---

## 5. Protocol / Command Layer

该层负责把外部通信转换成内部命令。

主要模块：

```text
Protocol Manager
Command Manager
Auth Manager integration
```

Protocol Manager 负责：

```text
Frame parsing
CRC
ACK / NACK
SEQ
Timeout
Retry
Duplicate detection
Secure frame verification
```

Command Manager 负责：

```text
Command registration
Command dispatch
Parameter check
Permission check
State check
Response encode
Command statistics
```

设计原则：

```text
Protocol 只处理帧和可靠性。
Command 只处理命令分发和权限。
具体业务交给对应 Manager。
```

---

## 6. Application Layer

Application Layer 负责设备业务流程。

典型文件：

```text
app_main.c
app_init.c
app_device_flow.c
app_factory_flow.c
app_algorithm_demo.c
```

职责：

```text
系统初始化流程
设备运行流程
产测流程
算法演示流程
低功耗入口流程
升级入口流程
```

原则：

```text
App 层组织流程，不直接操作底层硬件。
App 层调用 Manager 接口表达业务意图。
```

例如：

```text
DeviceMgr_PostEvent(DEV_EVT_START)
ParamMgr_Save()
DiagMgr_RecordEvent()
PowerMgr_RequestSleep()
```

---

## 7. Service / Manager Layer

Manager 层是系统能力和策略收口层。

Manager 的本质：

```text
管理一类资源、状态、生命周期、策略、错误和诊断出口。
```

计划模块：

```text
Device Manager
Protocol Manager
Command Manager
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

每个 Manager 推荐提供：

```text
Init
Start / Stop, if needed
GetStatus
GetStats
GetLastError
ResetStats
Process / Poll, if needed
```

设计原则：

```text
Manager 不应成为万能 SystemManager。
每个 Manager 必须有明确边界。
```

---

## 8. Middleware Layer

Middleware 层提供通用机制。

典型模块：

```text
RingBuffer
MessageQueue
CRC16
CRC32
SoftTimer
EventBus
ProtocolFrame
FileTransfer
LogCore
TraceBuffer
MemoryPool
```

特点：

```text
业务语义弱
可复用性强
不依赖 App
不直接操作具体硬件
```

原则：

```text
Middleware 提供机制，不制定复杂业务策略。
```

例如：

```text
RingBuffer 负责读写和容量管理。
Buffer Manager 负责统计、告警和策略。
```

---

## 9. Platform Abstraction Layer

Platform 层屏蔽芯片和外设差异。

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

职责：

```text
封装 STM32 HAL / LL
统一上层接口
处理 H7 Cache / DMA 细节
封装 Flash 擦写
封装串口收发
封装 FDCAN 收发
封装时间接口
```

原则：

```text
上层模块尽量不直接调用 HAL。
硬件相关细节尽量下沉到 Platform / BSP。
```

---

## 10. BSP / HAL Layer

BSP / HAL 层负责具体板级和芯片适配。

内容：

```text
CubeMX generated Core
STM32H7 HAL
CMSIS
Startup file
Linker script
Clock config
DMA config
GPIO alternate function
Board pin definition
LCD low-level driver
IMU low-level driver
```

原则：

```text
BSP 描述这块板是什么。
Platform 描述如何统一使用这些硬件能力。
```

---

## 11. Data Plane and Control Plane

### 11.1 Data Plane

数据面负责高频数据流。

例如：

```text
UART DMA RX
FDCAN RX
IMU sampling
Protocol parsing
RingBuffer push/pop
Timer ISR
```

数据面原则：

```text
路径短
少阻塞
少动态内存
少复杂抽象
中断中少做事
可测量执行时间
```

### 11.2 Control Plane

控制面负责配置、状态、策略和诊断。

例如：

```text
Parameter set/save
Diagnostic query
Security authentication
Upgrade state machine
Power mode transition
Algorithm parameter update
Fault handling
```

控制面原则：

```text
状态清晰
可观测
可测试
可恢复
可扩展
```

核心结论：

```text
高频数据面保持直接。
低频控制面使用 Manager 收口。
```

---

## 12. Mechanism and Policy

机制和策略分离是本项目的重要原则。

例子：

```text
RingBuffer 是机制。
Buffer Manager 的溢出处理是策略。

Flash Write 是机制。
Parameter Manager 的 A/B 双备份是策略。

UART Send 是机制。
Protocol Manager 的 ACK/NACK 和重传是策略。

Sleep Mode 是机制。
Power Manager 的进入和恢复流程是策略。
```

---

## 13. Manager Boundary

### 13.1 Device Manager

负责：

```text
设备总状态
状态切换
状态切换原因
状态持续时间
状态合法性
```

不负责：

```text
直接操作 UART
直接写 Flash
直接执行算法
```

---

### 13.2 Parameter Manager

负责：

```text
参数表
默认值
合法性检查
Flash 保存
A/B 双备份
CRC/HMAC
版本迁移
```

不负责：

```text
决定业务是否重启
直接处理协议帧
```

---

### 13.3 Diagnostic Manager

负责：

```text
系统状态汇总
错误码
计数器
复位原因
任务统计
模块状态查询
```

不负责：

```text
替其他模块执行恢复动作
```

---

### 13.4 Buffer Manager

负责：

```text
注册 buffer
统计水位
统计溢出
查询 buffer 状态
```

不负责：

```text
每个 buffer 的具体业务含义
```

---

### 13.5 Upgrade Manager

负责：

```text
升级状态机
固件头
分包接收
校验
App valid
App confirmed
升级失败处理
```

不负责：

```text
底层 Flash 驱动细节
协议帧解析细节
```

---

### 13.6 Security Manager

负责：

```text
认证状态
HMAC 验证
防重放
安全计数器
安全事件
```

不负责：

```text
具体 SHA/HMAC/ECDSA 算法底层实现
```

底层算法由 Crypto Service 提供。

---

### 13.7 Algorithm Manager

负责：

```text
算法注册
参数管理
统计信息
测试入口
版本信息
```

不强制负责：

```text
所有高频算法调用路径
```

原则：

```text
算法计算路径可以直接。
算法管理路径统一抽象。
```

---

## 14. Error Handling Strategy

所有模块需要统一错误码风格。

错误类型：

```text
OK
INVALID_PARAM
INVALID_STATE
TIMEOUT
BUSY
NO_MEMORY
CRC_ERROR
AUTH_FAIL
REPLAY_DETECTED
FLASH_ERROR
HARDFAULT_CAPTURED
UNKNOWN_ERROR
```

原则：

```text
错误必须可返回
关键错误必须可记录
严重错误必须可查询
致命错误必须进入 Fault / Device 状态机
```

---

## 15. Observability Strategy

每个关键模块建议提供：

```text
GetStatus
GetStats
GetLastError
ResetStats
```

典型统计：

```text
run_count
error_count
last_error
max_exec_us
rx_count
tx_count
drop_count
overflow_count
timeout_count
auth_fail_count
replay_detect_count
```

目标：

```text
系统出问题时可以通过 PC Tool 查询证据链。
```

---

## 16. Concurrency and Non-blocking Rules

规则：

```text
1. 中断中只做最少工作
2. 串口接收走 DMA + RingBuffer
3. Flash 写入要状态机化
4. 日志输出不能阻塞关键路径
5. 协议发送走 TX Queue
6. 任务必须有最大执行时间统计
7. 禁止在核心任务中长时间 delay
```

---

## 17. Directory Plan

计划目录：

```text
firmware/app/
├── Core/
├── App/
├── Services/
├── Middleware/
├── Platform/
├── BSP/

firmware/bootloader/
├── Core/
├── App/
├── Services/
├── Middleware/
├── Platform/
├── BSP/
```

PC 工具：

```text
pc_tool/
├── h7tool/
│   ├── cli.py
│   ├── protocol.py
│   ├── commands.py
│   ├── diagnostic.py
│   ├── upgrade.py
│   ├── security.py
│   ├── chaos.py
│   └── report.py
```

---

## 18. Development Rule

每个新模块至少包含：

```text
.h 接口文件
.c 实现文件
Init 接口
Status / Stats 结构
Error Code
基本文档说明
PC Tool 测试入口，若适用
```

---

## 19. Current Architecture Stage

当前阶段：

```text
Stage 0: Documentation and architecture planning
```

下一阶段：

```text
Stage 1: Board Bring-up
```

Stage 1 开始后，优先实现：

```text
BSP clock
platform_time
platform_gpio
platform_uart
basic app_main
reset reason
hardfault handler
```
## Current Implemented Low-Level Modules

The first Platform/BSP modules have been implemented during Stage 1 bring-up.

```text
Platform/
  platform_time    HAL tick, DWT cycle counter, profiling
  platform_uart    USART1 blocking transmit, printf retarget backend
  platform_reset   RCC reset flags, primary reset cause, software reset
  platform_fault   HardFault capture, SCB fault registers, fault decode

BSP/
  board_log        Boot banner, board log output, log level prefix
```

Current dependency direction:

```text
main.c
  ↓
board_log
platform_time
platform_reset
platform_uart

stm32h7xx_it.c
  ↓
platform_fault
  ↓
platform_uart / printf
```

Design rule:

```text
main.c should not directly contain low-level diagnosis logic.
Platform modules own MCU/platform mechanisms.
BSP modules own board-level identity and board-level log policy.
Services/Managers will be introduced after the low-level platform layer becomes stable.
```

---

## 20. Update Log

| Date | Update                                |
| ---- | ------------------------------------- |
| TBD  | Initial software architecture created |
