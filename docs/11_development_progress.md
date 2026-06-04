# Development Progress

## 1. Document Purpose

本文档用于记录 `DM-MC02 H723 Embedded Framework` 项目的整体开发进度、阶段安排、当前状态、已完成内容、下一步任务和阶段验收标准。

本项目不是一次性完成的外设 Demo，而是一个逐步演进的嵌入式系统框架项目。因此需要通过进度表持续跟踪：

```text
1. 当前项目处于哪个阶段
2. 每个阶段要完成什么
3. 每个阶段的验收标准是什么
4. 哪些任务已经完成
5. 哪些任务正在进行
6. 哪些任务尚未开始
7. 后续优先级如何安排
8. 当前 Sprint 的重点是什么
9. 每次阶段性提交应包含哪些内容
```

---

## 2. Project Current Status

当前阶段：

```text
Stage 2: UART Reliable Protocol
```

当前状态：

```text
In Progress
```

当前重点：

```text
1. 完成 UART Reliable Protocol V1 基础链路
2. 完成 ProtocolFrame / ProtocolManager / CommandManager / McuInfoApp 分层
3. 完成 PC 查询命令闭环
4. 完成 MCU 主动 EVENT 上报链路
5. 完成 PC 侧 ping / command / robust / event listen 测试
6. 下一步增强 PC 工具对异步 EVENT 的兼容能力
7. 后续接入 GET_RESET_INFO / GET_FAULT_INFO snapshot
```

当前已完成的重要闭环：

```text
PC PING            -> MCU PONG
PC GET_VERSION     -> MCU firmware version
PC GET_STATUS      -> MCU OK
PC GET_TIME_INFO   -> MCU tick
PC GET_UART_STATS  -> MCU UART RX stats
PC GET_APP_STATS   -> MCU McuInfoApp stats
MCU BOOT EVENT     -> PC event listener
MCU APP_MESSAGE    -> PC event listener
```

---

## 3. Progress Legend

| 状态          | 含义       |
| ----------- | -------- |
| Not Started | 尚未开始     |
| In Progress | 正在进行     |
| Done        | 已完成      |
| Blocked     | 被阻塞      |
| Need Review | 需要复盘或修改  |
| Deferred    | 暂缓       |
| Planned     | 已规划，尚未实现 |
| Next        | 下一步优先处理  |

---

## 4. Overall Roadmap

| Stage    | 阶段名称                               | 状态          | 主要目标                          |
| -------- | ---------------------------------- | ----------- | ----------------------------- |
| Stage 0  | Repository and Documentation       | Done        | 建立仓库、目录、README、总纲文档           |
| Stage 1  | Board Bring-up                     | Done        | 点亮板子，验证时钟、串口、复位、HardFault、DWT |
| Stage 2  | UART Reliable Protocol             | In Progress | 建立 PC 与 MCU 的可靠串口通信、命令查询、事件上报 |
| Stage 3  | Diagnostic Framework               | Not Started | 建立诊断、日志、Trace、Buffer 统计和健康监控  |
| Stage 4  | IMU and Algorithm Loop             | Not Started | 建立 IMU 数据采集、滤波和算法闭环           |
| Stage 5  | FDCAN Communication                | Not Started | 建立 FDCAN 通信和诊断能力              |
| Stage 6  | Parameter and Flash System         | Not Started | 实现参数管理、Flash 双备份和掉电保护         |
| Stage 7  | Bootloader and Upgrade             | Not Started | 实现 Bootloader、App 跳转和固件升级     |
| Stage 8  | Security                           | Not Started | 实现 HMAC、固件签名、防重放和安全日志         |
| Stage 9  | Low Power and Hardware Diagnostics | Not Started | 实现低功耗管理和硬件状态诊断                |
| Stage 10 | Chaos and Automated Test           | Not Started | 实现异常注入、压力测试和自动化报告             |

---

## 5. Stage 0: Repository and Documentation

### 5.1 Stage Goal

建立项目的工程基础，包括：

```text
GitHub 仓库
本地目录结构
README
docs 文档体系
Git 提交流程
项目总纲
板卡信息文档
芯片能力文档
软件架构文档
进度管理文档
```

该阶段的核心目标是：

> 先把项目边界、路线、目录结构和开发节奏规划清楚，再进入具体代码开发。

---

### 5.2 Task List

| 任务                   | 状态   | 说明                                                |
| -------------------- | ---- | ------------------------------------------------- |
| 创建本地项目目录             | Done | 已创建 `dm-mc02-h723-framework`                      |
| 初始化 Git 仓库           | Done | 已完成 `git init`                                    |
| 创建 GitHub 仓库         | Done | 仓库名：`dm-mc02-h723-framework`                      |
| 配置 remote origin     | Done | 已切换为 SSH 地址                                       |
| 解决 GitHub 首次 push 问题 | Done | 已完成 SSH 443 配置                                    |
| 建立基础目录结构             | Done | docs、firmware、pc_tool、tools、scripts、tests、release |
| 添加 `.gitignore`      | Done | 已忽略编译产物、密钥、临时文件                                   |
| 添加 README.md         | Done | 项目定位和基础说明                                         |
| 添加 Git 首次推送问题复盘文档    | Done | `docs/10_git_first_push_issue_summary.md`         |
| 添加项目总纲文档             | Done | `docs/00_project_overview.md`                     |
| 添加进度管理文档             | Done | `docs/11_development_progress.md`                 |
| 添加板卡信息文档             | Done | `docs/01_board_notes.md`                          |
| 添加芯片能力文档             | Done | `docs/02_chip_notes.md`                           |
| 添加软件架构文档             | Done | `docs/03_software_architecture.md`                |

---

### 5.3 Stage 0 Acceptance Criteria

Stage 0 完成标准：

```text
1. GitHub 仓库可以正常 push / pull —— Done
2. README.md 描述清楚项目定位 —— Done
3. docs/00_project_overview.md 完成项目总纲 —— Done
4. docs/01_board_notes.md 建立板卡信息骨架 —— Done
5. docs/02_chip_notes.md 建立芯片能力骨架 —— Done
6. docs/03_software_architecture.md 建立软件架构骨架 —— Done
7. docs/11_development_progress.md 可持续跟踪项目进度 —— Done
8. 每个文档至少有初始版本 —— Done
```

---

## 6. Stage 1: Board Bring-up

### 6.1 Stage Goal

验证 DM-MC02 / STM32H723 的基础运行能力，并建立第一批底层可观测能力。

该阶段不追求完整业务功能，也不急于进入复杂协议栈，而是优先确认：

```text
板子能启动
工程能稳定编译、烧录、调试
USART1 能作为调试生命线输出日志
系统 Tick 正常
时钟信息可打印
复位原因可读取
软件复位可验证
HardFault 能捕获和解码
DWT Cycle Counter 可用于性能测量
第一批 Platform / BSP 模块可以稳定运行
```

本阶段的核心目标是：

> 先建立“能启动、能观测、能定位故障、能测量耗时”的基础能力，再进入 Stage 2 的可靠串口协议开发。

---

### 6.2 Planned Tasks

| 任务                            | 状态       | 说明                                                                                     |
| ----------------------------- | -------- | -------------------------------------------------------------------------------------- |
| CubeMX 创建 STM32H723VGT6 工程    | Done     | 建立基础 app 工程                                                                            |
| 配置基础时钟树                       | Done     | 当前使用稳定频率，不急于最高主频                                                                       |
| USART1 printf                 | Done     | USART1 printf verified                                                                 |
| SysTick 1ms                   | Done     | `HAL_GetTick()` verified by UART log                                                   |
| Boot log 输出                   | Done     | 启动 Banner、Build 时间、阶段信息可打印                                                             |
| Clock Info 打印                 | Done     | SYSCLK / HCLK / PCLK1 / PCLK2 可打印                                                      |
| Reset Reason 读取               | Done     | RCC reset flags printed on boot                                                        |
| Primary Reset Cause           | Done     | 多个 reset flags 可根据优先级推断主复位原因                                                           |
| Software Reset Test           | Done     | `NVIC_SystemReset()` verified by reset reason flag                                     |
| HardFault Handler             | Done     | 主动触发并捕获异常                                                                              |
| HardFault Stack Frame Capture | Done     | R0/R1/R2/R3/R12/LR/PC/xPSR 可打印                                                         |
| Fault Register Dump           | Done     | CFSR/HFSR/DFSR/AFSR/MMFAR/BFAR 可打印                                                     |
| HardFault Decode              | Done     | CFSR/HFSR decoded by UART log                                                          |
| Fault Diagnosis Document      | Done     | PC address location method documented                                                  |
| DWT Cycle Counter             | Done     | CPU cycle counter verified by UART log                                                 |
| Platform Time Module          | Done     | HAL tick and DWT cycle counter wrapped by `platform_time`                              |
| Platform UART Module          | Done     | USART1 blocking transmit and printf retarget backend wrapped by `platform_uart`        |
| Platform Reset Module         | Done     | Reset flag capture, primary reset cause and software reset wrapped by `platform_reset` |
| Platform Fault Module         | Done     | HardFault capture and fault decode wrapped by `platform_fault`                         |
| Board Log Module              | Done     | Boot banner and INFO/WARN/ERROR output wrapped by `board_log`                          |
| App Layer Skeleton            | Done     | `App_Init()` / `App_Run()` created in `App/app_main.c`                                 |
| main.c Cleanup                | Done     | `main.c` reduced to HAL init, CubeMX init, `App_Init()` and `App_Run()`                |
| Git 提交基础工程                    | Done     | 基础工程已提交到 `feature/board-bringup`                                                       |
| Git 提交 Platform/BSP 模块化       | Done     | 已提交 `platform_time`、`platform_uart`、`platform_reset`、`platform_fault`、`board_log`      |
| Git 提交 App 层骨架                | Done     | 已提交 `App/app_main.c`、`App/app_main.h` 和 `main.c` 清理                                    |
| LED / GPIO 测试                 | Deferred | 当前板卡无明显用户 LED，暂缓                                                                       |
| Timer GPIO Toggle             | Deferred | 后续确认可用 GPIO 后再做物理频率验证                                                                  |

---

### 6.3 Current Modularized Components

Stage 1 已经完成第一批底层能力模块化。

```text
Platform/
  platform_time    HAL tick, DWT cycle counter, profiling, delay wrapper
  platform_uart    USART1 blocking transmit, printf retarget backend
  platform_reset   RCC reset flags, primary reset cause, software reset
  platform_fault   HardFault stack frame capture, SCB fault registers, CFSR/HFSR decode

BSP/
  board_log        Boot banner, board log output, INFO/WARN/ERROR prefix
```

模块化前，较多底层逻辑集中在 `main.c`：

```text
main.c
  ├── printf retarget
  ├── reset reason decode
  ├── software reset test
  ├── DWT test
  ├── HardFault capture
  ├── fault decode
  └── boot banner
```

模块化后，底层机制开始沉淀到 Platform/BSP 层：

```text
main.c
  ↓
board_log
platform_time
platform_uart
platform_reset

stm32h7xx_it.c
  ↓
platform_fault
```

当前阶段形成的设计原则：

```text
Platform 层负责 MCU / Cortex-M / STM32 底层能力抽象。
BSP 层负责板级身份、板级日志、板级资源策略。
main.c 不应长期承载大量底层诊断逻辑。
后续 Services / Managers / Apps 应建立在稳定的 Platform/BSP 基础之上。
```

---

### 6.4 Verified Runtime Behavior

当前已验证的运行行为：

```text
1. Keil 工程可以稳定编译和下载
2. MCU 可以正常运行到 main
3. USART1 可以稳定输出串口日志
4. printf 重定向正常
5. Boot Banner 可以正常打印
6. SYSCLK / HCLK / PCLK1 / PCLK2 可以打印
7. HAL_GetTick() 正常递增
8. RCC reset flags 可以读取
9. Software Reset 可以通过 NVIC_SystemReset() 触发
10. Software Reset 后可以通过 reset flags 识别
11. HardFault 可以主动触发
12. HardFault_Handler 可以进入自定义处理流程
13. Fault 栈帧可以解析出 PC / LR / xPSR
14. CFSR / HFSR 可以自动解码
15. DWT CYCCNT 可以正常计数
16. cycles 可以换算为 us / ns
17. Platform/BSP 模块化后系统仍能正常启动
18. App_Init / App_Run 结构可稳定运行
```

默认测试宏建议状态：

```c
#define ENABLE_SOFTWARE_RESET_TEST   0
#define ENABLE_HARDFAULT_TEST        0
#define ENABLE_DWT_TEST              0
```

---

### 6.5 Acceptance Criteria

Stage 1 当前验收情况：

```text
1. 固件可以正常编译和下载 —— Done
2. 板子上电后稳定运行 —— Done
3. 串口可以输出 boot log —— Done
4. Reset reason 可以通过串口打印 —— Done
5. Software reset 可以被触发和识别 —— Done
6. HardFault 能进入自定义 Handler —— Done
7. Fault 现场 PC/LR/CFSR/HFSR 可以打印 —— Done
8. Fault 状态可以初步自动解码 —— Done
9. DWT Cycle Counter 可以测量代码耗时 —— Done
10. 第一批 Platform/BSP 模块稳定运行 —— Done
11. main.c 已清理为系统入口 —— Done
12. App_Init / App_Run 骨架已建立 —— Done
13. 代码提交到 GitHub —— Done
```

暂缓项：

```text
LED 可以周期翻转 —— Deferred
Timer 输出频率与配置一致 —— Deferred
```

暂缓原因：

```text
当前 DM-MC02 板卡没有明确用户 LED。
Timer GPIO Toggle 需要先确认可用引脚和测试点，后续在 GPIO/Timer 资源确认后补做。
```

---

### 6.6 Stage 1 Conclusion

Stage 1 已经完成了最关键的底层 bring-up 和可观测能力建设。

当前系统已经具备：

```text
UART 生命线
Boot log
Reset diagnosis
Software reset
Clock info
HAL tick
DWT profiling
HardFault capture
Fault decode
第一批 Platform/BSP 模块
App_Init / App_Run 应用入口
```

这意味着项目已经从“功能跑通”进入“可观测、可诊断、可沉淀”的阶段。

下一阶段已经进入：

```text
Stage 2: UART Reliable Protocol
```

目标是把 UART 从调试输出通道升级为可靠通信通道。

---



## 7. Stage 2: UART Reliable Protocol

### 7.1 Stage Goal

建立 PC 与 MCU 之间的可靠 UART 通信通道。

Stage 2 不只是实现简单串口收发，而是在 Stage 1 已完成的 USART1 生命线基础上，构建一个可扩展、可诊断、可测试、可逐步增强的通信协议框架。

该阶段重点包括：

```text
UART DMA 高效接收
RX RingBuffer 字节流缓存
通用状态机框架
二进制协议帧
CRC16 校验
Frame Parser
Protocol Manager
Command Manager
McuInfoApp 信息中枢 App
PC Python Tool
半包 / 粘包 / 错包 / 垃圾字节恢复
MCU 主动 EVENT 上报
后续 ACK / 重传 / 滑动窗口扩展
```

Stage 2 的核心目标是：

> 把 UART 从“调试输出通道”升级为“可靠命令、状态查询、事件上报与后续数据传输通道”。

---

### 7.2 Protocol Evolution Plan

UART Reliable Protocol 采用分阶段升级路线。

| Version | 目标                                         | 说明                                  |
| ------- | ------------------------------------------ | ----------------------------------- |
| V1      | Reliable Frame + CRC + REQ/RESP/NACK/EVENT | 实现帧边界、长度、CRC、基础请求响应、主动事件上报          |
| V2      | SEQ + ACK/NACK + Timeout Retry             | 加入序号、确认、超时、重传、去重                    |
| V3      | Small Sliding Window                       | 加入小窗口机制，提高连续数据传输效率                  |
| V4      | Fragment / File Transfer                   | 支持参数导入导出、日志上传、固件分片传输                |
| V5      | Security Extension                         | 支持 HMAC、Challenge-Response、防重放、加密预留 |

当前 Stage 2 已经完成 V1 的主要链路，后续将进入 V1 稳定化与 V2 可靠性增强设计。

---

### 7.3 Current Architecture

当前 Stage 2 协议架构已经调整为 CommandService 注册表架构：

```text
PC Tool
  ↓
UART Frame
  ↓
ProtocolFrame Parser
  ↓
ProtocolManager
  ↓
CommandManager
  ↓
CommandService
  ↓
App internal handler
  ↓
McuInfoApp / Other Apps / Platform / Board
```

核心分层原则：

```text
ProtocolFrame 只负责帧格式、编码、解码、CRC 和 Parser。
ProtocolFrame 不定义具体业务 CMD。
ProtocolManager 只负责 UART 字节流接入、协议帧收发和统计。
CommandManager 只负责 REQ / RESP / NACK / EVENT 语义流程。
CommandService 负责统一命令表、命令注册、命令分发和响应辅助封装。
McuInfoApp 是 MCU 与 PC 信息交互中心，也是 App handler 与 CommandService 的牵手入口。
具体命令 handler 集成在各自 App 内部。
其他 App 不直接调用 CommandManager / ProtocolManager。
其他 App 若需要与 PC 通信，先通过 McuInfoApp_RegisterCommand() / McuInfoApp_PostEvent() / McuInfoApp_UpdateSnapshot()。
```

主动 EVENT 上报链路：

```text
Other App / Test Code
  ↓
McuInfoApp_PostEvent()
  ↓
McuInfoApp internal RingBuffer
  ↓
McuInfoApp_Run()
  ↓
CommandManager_PostEvent()
  ↓
CommandManager pending event RingBuffer
  ↓
ProtocolManager_Process()
  ↓
EVENT frame
  ↓
PC proto_event_listen.py
```

命令查询链路：

```text
PC REQ
  ↓
ProtocolManager
  ↓
CommandManager_Dispatch()
  ↓
CommandService_Dispatch()
  ↓
cmd table lookup
  ↓
App internal handler fills CommandManagerResponse_t
  ↓
ProtocolManager sends RESP / NACK
```

当前通信语义定位：

```text
CMD       PC -> MCU       请求、配置、控制、查询
EVENT     MCU -> PC       离散重要事件
SNAPSHOT  MCU 内部缓存     最近状态，PC 通过 CMD 查询
STREAM    MCU -> PC       高频实时数据流，后续实现
BULK      双向/单向        大块可靠传输，后续实现
```

### 7.4 Planned Tasks

| 任务                             | 状态      | 说明                                                                        |
| ------------------------------ | ------- | ------------------------------------------------------------------------- |
| UART Reliable Protocol Design  | Done    | 已完成并根据 CommandService / McuInfoApp 架构重修                                   |
| Protocol Frame Format          | Done    | 使用 SOF + VER + TYPE + FLAGS + SEQ + CMD + LEN + PAYLOAD + CRC16           |
| Protocol Evolution Roadmap     | Done    | 预留 ACK/NACK、重传、小滑动窗口、分片、安全扩展                                              |
| UART DMA + IDLE 接收             | Done    | USART1 RX DMA circular mode                                               |
| DMA Half Transfer 处理           | Done    | DMA 写满前半区后搬运数据到 RX RingBuffer                                             |
| DMA Transfer Complete 处理       | Done    | DMA 写满后半区后搬运数据到 RX RingBuffer                                             |
| UART IDLE 处理                   | Done    | 支持不定长数据和空闲事件                                                              |
| RX RingBuffer                  | Done    | 解耦 DMA 接收与协议解析                                                            |
| RingBuffer 统计                  | Done    | overflow、high watermark、read/write bytes                                  |
| Generic State Machine          | Done    | 通用状态机框架，用于 Frame Parser 和后续模块                                             |
| CRC16                          | Done    | CRC16-CCITT-FALSE，用于基础通信校验                                                |
| UART RX Stats Snapshot         | Done    | MCU side exposes RX DMA and RingBuffer runtime statistics                 |
| UART RX Stats Report           | Done    | MCU periodically outputs `@UARTSTAT` telemetry line                       |
| PC UART Monitor Tool           | Done    | Python UI parses `@UARTSTAT` and displays runtime metrics                 |
| PC UART Stress Tool            | Done    | Python UI sends special byte patterns and burst data                      |
| UART RX Visualization          | Done    | Plot RingBuffer available / high watermark / overflow / rx rate           |
| UART RX Robustness Matrix      | Done    | Test half DMA, full DMA, wraparound, idle gap, overflow and random stream |
| Protocol Frame Module          | Done    | `protocol_frame.h/.c` completed                                           |
| Frame Parser                   | Done    | StateMachine based parser completed                                       |
| Parser Error Recovery          | Done    | 支持半包、粘包、垃圾字节、CRC 错误恢复                                                     |
| Protocol Frame Self Test       | Done    | `ENABLE_PROTOCOL_FRAME_TEST` 宏控测试 PASS                                    |
| Protocol Manager               | Done    | 负责 RX 字节处理、Frame Parser、REQ 分发、RESP/NACK/EVENT 发送                         |
| Command Manager                | Done    | 负责 REQ/RESP/NACK/EVENT 语义流程和 EVENT pending queue                         |
| CommandService                 | Done    | 统一命令注册表，支持 cmd -> handler 分发、统计和 GET_COMMAND_STATS                         |
| McuInfoApp                     | Done    | MCU 与 PC 信息交互中心 App，支持命令挂载、事件收集、事件转发、快照缓存                            |
| McuInfoApp Event Queue         | Done    | 使用 RingBuffer 作为内部事件队列                                                    |
| CommandManager Event Queue     | Done    | 使用 RingBuffer 作为待发送 EVENT 队列                                              |
| PING 命令                        | Done    | PC `ping` -> MCU `PONG`                                                   |
| GET_VERSION 命令                 | Done    | 查询固件版本字符串                                                                 |
| GET_STATUS 命令                  | Done    | 查询系统状态                                                                    |
| GET_TIME_INFO 命令               | Done    | 查询系统 tick                                                                 |
| GET_UART_STATS 命令              | Done    | 查询 UART RX / RingBuffer 统计                                                |
| GET_APP_STATS 命令               | Done    | 查询 McuInfoApp 统计                                                          |
| GET_RESET_INFO 命令              | Done    | 通过 McuInfoApp reset snapshot 返回真实复位信息                                      |
| GET_COMMAND_STATS 命令           | Done    | 查询 CommandService 初始化、注册、分发、错误统计                                         |
| GET_FAULT_INFO 命令              | Planned | 后续通过 fault snapshot/event 接入 Fault 信息                                     |
| Python `proto_ping_test.py`    | Done    | 支持 ping/version/status                                                    |
| Python `proto_command_test.py` | Done    | 支持 command 扩展测试，已加入 GET_COMMAND_STATS                                      |
| Python `proto_robust_test.py`  | Done    | 支持半包、粘包、垃圾字节、CRC 错误、未知命令等测试                                               |
| Python `proto_event_listen.py` | Done    | 支持监听 EVENT 异步事件                                                           |
| Python `proto_async_mix_test.py` | Done  | 支持命令响应与异步 EVENT 混合场景                                                     |
| BOOT EVENT                     | Done    | MCU 启动后可主动上报 BOOT 事件                                                      |
| APP_MESSAGE EVENT              | Done    | 测试事件可通过 McuInfoApp → CommandManager → ProtocolManager 上报到 PC              |
| 栈空间问题记录                        | Done    | 已记录协议链路引入后栈空间不足导致 tick 异常的问题                                              |
| ACK / NACK                     | Planned | V2 可靠性增强                                                                  |
| Timeout Retry                  | Planned | V2 可靠性增强                                                                  |
| Duplicate Detection            | Planned | V2 防止重复执行副作用命令                                                            |
| Small Sliding Window           | Planned | V3 用于大数据传输和固件升级                                                           |
| Fragment Transfer              | Planned | V4 用于固件升级、日志上传、参数导入导出                                                     |
| Security Extension             | Planned | V5 用于 HMAC、认证、防重放                                                         |

### 7.5 Current Module Structure

当前 Stage 2 已形成以下模块：

```text
firmware/app/
├── Middleware/
│   ├── ring_buffer.h
│   ├── ring_buffer.c
│   ├── crc16.h
│   ├── crc16.c
│   ├── state_machine.h
│   ├── state_machine.c
│   ├── protocol_frame.h
│   └── protocol_frame.c
│
├── Services/
│   ├── protocol_manager.h
│   ├── protocol_manager.c
│   ├── command_manager.h
│   ├── command_manager.c
│   ├── command_service.h
│   └── command_service.c
│
├── Apps/
│   ├── mcu_info_app.h
│   └── mcu_info_app.c
│
├── Platform/
│   ├── platform_uart.h
│   ├── platform_uart.c
│   ├── platform_time.h
│   ├── platform_time.c
│   ├── platform_reset.h
│   ├── platform_reset.c
│   ├── platform_fault.h
│   └── platform_fault.c
│
├── Board/
│   ├── board_log.h
│   └── board_log.c
│
└── App/
    ├── app_main.h
    └── app_main.c
```

PC 工具：

```text
pc_tool/h7_uart_ui/
├── proto_ping_test.py
├── proto_command_test.py
├── proto_robust_test.py
├── proto_event_listen.py
├── main.py
└── h7_ui/
```

---

### 7.6 Verified Runtime Behavior

当前已验证行为：

```text
1. UART RX DMA 可以启动
2. DMA Half / Complete / IDLE 可以搬运数据到 RX RingBuffer
3. RX RingBuffer 可以承受 UI 压测
4. RingBuffer high watermark / overflow 可观测
5. CRC16 标准测试向量 PASS
6. StateMachine 自测试 PASS
7. ProtocolFrame 自测试 PASS
8. PC 可以发送 PING，MCU 返回 PONG
9. PC 可以发送 GET_VERSION，MCU 返回版本字符串
10. PC 可以发送 GET_STATUS，MCU 返回 OK
11. PC 可以发送 GET_TIME_INFO，MCU 返回 tick
12. PC 可以发送 GET_UART_STATS，MCU 返回 UART 统计
13. PC 可以发送 GET_APP_STATS，MCU 返回 McuInfoApp 统计
14. PC 可以发送 GET_RESET_INFO，MCU 返回真实 reset snapshot
15. PC 可以发送 GET_COMMAND_STATS，MCU 返回 CommandService 统计
16. 未知命令返回 NACK UNKNOWN_CMD
17. CRC 错误帧被拒绝
18. 半包可以等待后续数据
19. 粘包可以连续解析
20. 垃圾字节不会导致 Parser 长期失效
21. 超长 LEN 被拒绝
22. BOOT EVENT 可以主动上报到 PC
23. APP_MESSAGE TEST EVENT 可以主动上报到 PC
24. PC 工具可以在异步 EVENT 混入时继续正确等待目标 RESP/NACK
25. UI Protocol Monitor 可以显示 RESP / NACK / EVENT，并支持 Auto Poll
26. CommandService 重构后 ping/version/status/time/reset/uart/app/command_stats 测试均通过
```

`GET_COMMAND_STATS` 典型响应：

```text
init=1,reg=10,disp=7,unk=0,err=0,last=0x0D
```

### 7.7 Known Issues and Records

当前已记录问题：

```text
1. 协议链路引入后曾出现 HAL tick / uptime 异常跳变。
2. 初步定位为栈空间不足。
3. 增大工程栈空间后问题消失。
4. 后续需要继续优化协议热路径栈使用。
```

后续优化方向：

```text
1. 大局部结构体继续转移到模块上下文
2. 减少协议热路径 printf / BoardLog
3. 增加 stack usage 估算文档
4. 后续引入 RTOS 后建立任务栈监控
5. 后续所有 PC outbound frame 统一进入 TX owner / TX queue
```

---

### 7.8 Current Stage 2 Status

Current status:

```text
Stage 2.16: CommandService observability completed.
```

已完成：

```text
UART RX DMA + RingBuffer
CRC16
Generic StateMachine
ProtocolFrame Parser
ProtocolManager
CommandManager
CommandService registry
McuInfoApp command mount / event / snapshot
PC Ping Test
PC Command Test
PC Robust Test
PC Event Listen Test
PC Async Mix Test
UI Protocol Monitor
BOOT EVENT
APP_MESSAGE EVENT
GET_RESET_INFO
GET_COMMAND_STATS
```

当前架构闭环：

```text
ProtocolManager
  ↓
CommandManager
  ↓
CommandService
  ↓
App internal handler
```

当前信息出口闭环：

```text
CMD       PC -> MCU       Done
EVENT     MCU -> PC       Done
SNAPSHOT  MCU cache       Done for reset, extensible
STREAM    MCU -> PC       Planned
BULK      transfer        Planned
```

下一步建议：

```text
Stage 2.17: GET_FAULT_INFO Snapshot + Fault EVENT Integration
```

目标：

```text
1. Fault 模块将 fault 信息作为 snapshot/event 接入 McuInfoApp
2. GET_FAULT_INFO 返回最近 fault 状态
3. Fault 发生时可通过 EVENT 主动通知 PC
4. PC command / event / robust / async mix 测试保持通过
```

再下一步：

```text
Stage 2.18: Reliability V2 Design
```

目标：

```text
1. ACK / timeout / retry 机制设计
2. Duplicate request detection
3. 有副作用命令的幂等策略
4. 为后续 sliding window 和 bulk transfer 打基础
```

### 7.9 Stage 2 V1 Acceptance Criteria

Stage 2 V1 当前验收情况：

```text
1. UART RX DMA 可以连续接收字节 —— Done
2. DMA Half / Complete / IDLE 事件能够正确搬运数据 —— Done
3. RX RingBuffer 可以缓存字节流 —— Done
4. RingBuffer 具备 overflow 和 high watermark 统计 —— Done
5. Frame Parser 可以从字节流中恢复完整帧 —— Done
6. CRC16 可以识别错误帧 —— Done
7. PING / GET_VERSION / GET_STATUS 可以正常响应 —— Done
8. GET_TIME_INFO / GET_UART_STATS / GET_APP_STATS 可以正常响应 —— Done
9. GET_RESET_INFO 可以返回真实 reset snapshot —— Done
10. GET_COMMAND_STATS 可以返回 CommandService 统计 —— Done
11. PC Python Tool 可以发送命令并解析响应 —— Done
12. 半包场景下 parser 能等待更多数据 —— Done
13. 粘包场景下 parser 能连续解析多帧 —— Done
14. 垃圾字节不会导致 parser 长期失效 —— Done
15. 超长帧不会导致 buffer 越界 —— Done
16. 通信错误有计数器 —— Done
17. McuInfoApp 可以主动上报 BOOT EVENT —— Done
18. McuInfoApp 可以主动上报 APP_MESSAGE EVENT —— Done
19. CommandManager 可以缓存待发送 EVENT —— Done
20. ProtocolManager 可以发送 EVENT frame —— Done
21. CommandService 可以统一注册和分发命令 —— Done
22. App handler 可以通过 McuInfoApp 牵手挂载到 CommandService —— Done
23. PC 工具兼容异步 EVENT 干扰 —— Done
```

Stage 2 V1 剩余增强项：

```text
1. GET_FAULT_INFO 接入真实 fault snapshot/event —— Next
2. 协议栈空间占用优化 —— Planned
3. CommandService 命令列表查询 —— Planned
4. UI 增加 GET_COMMAND_STATS 独立按钮 —— Optional
```

Stage 2 V2 增强阶段：

```text
1. SEQ request-response matching works
2. ACK/NACK frame works
3. Timeout retry works
4. Duplicate detection works
5. Small sliding window design is implemented or partially implemented
6. Protocol stress test can be automated by PC Tool
```

## 8. Stage 3: Diagnostic Framework

### 8.1 Stage Goal

建立系统可观测性与工程诊断骨架。

Stage 3 将在 Stage 1 的底层可观测能力和 Stage 2 的 UART Reliable Protocol 通信能力基础上，构建一套可查询、可定位、可验证、可扩展的诊断框架。

该阶段的目标不是简单打印日志，也不是把所有内部变量暴露给 PC，而是让系统在出现异常时能够回答以下工程问题：

```text
1. 系统当前是否还活着？
2. 系统整体是否健康？
3. 错误发生在哪一层？
4. 是通信错误、资源压力、状态异常、时序问题，还是 App handler 问题？
5. 问题是偶发、持续累积，还是瞬间崩溃？
6. 出问题前最后发生了什么？
7. 能否通过 PC 工具远程定位，而不依赖调试器？
```

Stage 3 的核心目标是：

> 把系统从“能运行”升级为“可观测、可诊断、可定位”。

---

### 8.2 Diagnostic View Design

Stage 3 采用诊断视图方式组织系统信息，而不是简单暴露所有变量。

推荐诊断视图：

```text
Health View          总体健康视图
Pipeline View        通信链路视图
Error View           错误计数器视图
Buffer View          队列 / 缓冲压力视图
Timing View          时间 / 延迟 / 抖动视图
Last Records View    最近命令 / 事件 / 错误视图
State View           状态机视图，后续扩展
Build / Config View  版本与配置视图，后续扩展
```

Stage 3 V1 优先实现以下四个基础视图：

```text
GET_HEALTH
GET_ERROR_COUNTERS
GET_BUFFER_STATS
GET_LAST_RECORDS
```

---

### 8.3 Current Diagnostic Data Sources

当前工程中多个模块已经具备全局上下文和统计结构体，这些信息将作为 Stage 3 诊断框架的原始数据源。

典型数据源包括：

```text
PlatformUart / RingBuffer
  - rx_bytes
  - rx_error_count
  - rx_ring_available
  - rx_ring_free
  - rb_high_watermark
  - rb_overflow_count

ProtocolFrame Parser
  - frame_ok_count
  - sof_error_count
  - len_error_count
  - crc_error_count

ProtocolManager
  - frame_received_count
  - frame_sent_count
  - req_frame_count
  - resp_frame_count
  - nack_frame_count
  - event_frame_count
  - parser_error_count
  - tx_error_count
  - last_rx_cmd
  - last_tx_cmd

CommandManager
  - dispatch_count
  - post_event_count
  - event_pop_count
  - event_drop_count
  - unknown_cmd_count
  - invalid_param_count
  - error_count
  - last_cmd
  - last_event_id
  - last_error

CommandService
  - init_count
  - register_count
  - registered_count
  - dispatch_count
  - unknown_cmd_count
  - handler_error_count
  - last_cmd
  - last_category
  - last_error

McuInfoApp
  - run_count
  - post_event_count
  - event_forward_count
  - event_drop_count
  - update_snapshot_count
  - get_snapshot_count
  - last_event_id
  - last_error
```

这些 stats 的工程意义是：

```text
把 UART / Parser / Protocol / Command / App / Event 这些链路从黑盒变成可观测对象。
```

---

### 8.4 Diagnostic Framework Architecture

Stage 3 建议新增：

```text
firmware/app/Apps/diagnostic_app.h
firmware/app/Apps/diagnostic_app.c
```

总体链路：

```text
PC Tool / UI
  ↓
ProtocolManager
  ↓
CommandManager
  ↓
CommandService
  ↓
DiagnosticApp command handlers
  ↓
DiagnosticApp
  ↓
ProtocolManager / CommandManager / CommandService / McuInfoApp / Platform / RingBuffer
```

设计原则：

```text
1. DiagnosticApp 不直接发送协议帧
2. DiagnosticApp handler 通过 McuInfoApp_RegisterCommand() 挂载到 CommandService
3. DiagnosticApp 只负责聚合、解释和组织诊断视图
4. ProtocolManager 仍然是唯一帧收发执行者
5. CommandService 仍然是命令表中心
6. McuInfoApp 仍然是 App 与 PC 信息交互牵手入口
```

推荐初始化顺序：

```text
CommandService_Init()
McuInfoApp_Init()
McuInfoApp_UpdateResetSnapshot()
DiagnosticApp_Init()
DiagnosticApp_RegisterCommands()
CommandManager_Init()
ProtocolManager_Init()
```

---

### 8.5 Diagnostic Commands

Stage 3 诊断命令从 `0x20` 开始规划。

```c
typedef enum
{
    DIAG_CMD_GET_HEALTH          = 0x20,
    DIAG_CMD_GET_ERROR_COUNTERS  = 0x21,
    DIAG_CMD_GET_BUFFER_STATS    = 0x22,
    DIAG_CMD_GET_TIMING_STATS    = 0x23,
    DIAG_CMD_GET_LAST_RECORDS    = 0x24,
    DIAG_CMD_GET_PIPELINE_STATS  = 0x25,
    DIAG_CMD_CLEAR_COUNTERS      = 0x26,
    DIAG_CMD_GET_TRACE_STATUS    = 0x27,
    DIAG_CMD_DUMP_TRACE          = 0x28
} DiagnosticCommandId_t;
```

Stage 3 V1 优先实现：

```text
0x20 GET_HEALTH
0x21 GET_ERROR_COUNTERS
0x22 GET_BUFFER_STATS
0x24 GET_LAST_RECORDS
```

暂时保留：

```text
0x23 GET_TIMING_STATS
0x25 GET_PIPELINE_STATS
0x26 CLEAR_COUNTERS
0x27 GET_TRACE_STATUS
0x28 DUMP_TRACE
```

---

### 8.6 Planned Tasks

| 任务 | 状态 | 说明 |
| --- | --- | --- |
| Diagnostic Design Document | Done | 建立 `05_diagnostic_design.md`，作为 Stage 3 开发总纲 |
| DiagnosticApp Skeleton | Not Started | 新增 `diagnostic_app.h/.c`，建立诊断中心 App |
| Diagnostic Commands Registration | Not Started | 通过 `McuInfoApp_RegisterCommand()` 挂载诊断命令 |
| GET_HEALTH | Not Started | 返回系统总体健康摘要 |
| GET_ERROR_COUNTERS | Not Started | 返回各层错误计数器摘要 |
| GET_BUFFER_STATS | Not Started | 返回 UART / RingBuffer / Event Queue 资源压力摘要 |
| GET_LAST_RECORDS | Not Started | 返回最近命令、事件、错误、NACK、复位信息 |
| GET_TIMING_STATS | Planned | 后续返回主循环周期、最大延迟、模块耗时 |
| GET_PIPELINE_STATS | Planned | 后续返回完整通信链路统计 |
| CLEAR_COUNTERS | Planned | 后续支持清除诊断计数器 |
| Trace Ring | Planned | 后续实现最近记录环形追踪 |
| Health State Evaluation | Not Started | 建立 OK / WARN / ERROR 健康状态判断逻辑 |
| Fault Injection Tests | Planned | 通过未知命令、CRC 错误、队列满、handler error 验证诊断视图 |
| Python Diagnostic Test | Not Started | 新增 `proto_diagnostic_test.py` |
| UI Diagnostic Buttons | Not Started | 增加 HEALTH / ERRORS / BUFFERS / LAST 查询按钮 |
| Development Progress Sync | Not Started | Stage 3 开发后同步进度文档 |

---

### 8.7 Health View

Health View 是系统诊断第一视图，用于快速判断系统是否健康。

推荐字段：

```text
health
uptime
loop_count
err_total
drop_total
rx_ovf
last_error
last_cmd
last_event
```

Health 状态建议：

```text
OK:
  没有严重错误
  没有 queue drop
  没有 UART overflow
  handler_error_count == 0

WARN:
  出现 unknown command
  出现少量 parser error
  出现 buffer high watermark 较高
  出现 event drop 但系统仍可运行

ERROR:
  handler_error_count > 0
  queue drop 持续增加
  UART overflow 持续增加
  ProtocolManager tx_error_count > 0
  invalid_state / internal_error 持续出现
```

返回示例：

```text
health=OK,uptime=125430,err=0,drop=0,rx_ovf=0,last=0x00
```

---

### 8.8 Error View

Error View 用于回答系统正在犯什么类型的错误。

推荐统计项：

```text
unknown_cmd
handler_error
invalid_param
invalid_state
busy
queue_full
crc_error
parser_error
uart_error
overflow
tx_error
build_error
```

返回示例：

```text
unknown=1,handler=0,parser=0,crc=0,busy=0,qfull=0,uart=0,ovf=0
```

工程意义：

```text
1. 判断错误是否正在累积
2. 判断错误发生在协议层、命令层、队列层，还是底层 UART
3. 判断是未知命令、handler 错误、资源忙，还是参数错误
4. 人为注入错误后，验证对应计数器是否增长
```

---

### 8.9 Buffer View

Buffer View 用于观察系统资源压力。

推荐统计项：

```text
uart_avail
uart_free
uart_high
uart_ovf
mcu_evt_drop
cmd_evt_drop
mcu_evt_post
mcu_evt_fwd
cmd_evt_post
cmd_evt_pop
```

返回示例：

```text
uart_avail=0,uart_high=180,uart_ovf=0,mcu_drop=0,cmd_drop=0
```

工程意义：

```text
1. 判断 UART RX RingBuffer 是否接近满
2. 判断 McuInfoApp event queue 是否积压
3. 判断 CommandManager event queue 是否积压
4. 判断是否存在 overflow / drop
5. 判断 high watermark 是否接近容量上限
```

---

### 8.10 Last Records View

Last Records View 用于保存最近发生过的关键行为。

第一版推荐字段：

```text
last_cmd
last_event
last_error
last_nack_cmd
last_nack_error
last_reset
last_health
```

返回示例：

```text
last_cmd=0x0D,last_evt=0x85,last_err=0x00,last_nack=0x7E,last_reset=POR
```

后续可升级为 trace ring：

```text
[12340] CMD 0x01 RESP OK
[12400] EVENT 0x85 APP_MESSAGE
[12500] CMD 0x7E NACK UNKNOWN_CMD
[12600] CMD 0x0D RESP OK
```

---

### 8.11 Fault Injection / Chaos Test Plan

诊断框架必须能够被验证。

人为注入错误的意义：

```text
1. 验证错误是否能被检测到
2. 验证错误计数器是否增长
3. 验证错误是否定位到正确模块
4. 验证系统是否能从错误中恢复
5. 验证诊断视图是否能反映真实问题
```

Stage 3 V1 推荐验证以下错误：

| 诊断对象 | 注入方法 | 预期响应 | 预期计数器 |
| --- | --- | --- | --- |
| 未知命令 | PC 发送 `0x7E` | NACK UNKNOWN_CMD | `unknown_cmd_count++` |
| CRC 错误 | PC 发送 CRC 被篡改的帧 | 无 RESP | `crc_error_count++` |
| 垃圾字节 | PC 发送随机字节后再发 PING | PING 正常 | `sof_error_count++` |
| 半包 | PC 发送前半帧，延迟后发送后半帧 | 后半到达后 RESP | parser 等待后恢复 |
| 粘包 | PC 连续发送多帧 | 多个 RESP | frame_ok_count 增长 |
| 事件队列满 | MCU 快速 PostEvent 超过队列容量 | 系统不崩 | event_drop_count++ |
| handler 错误 | 测试 handler 返回 ERROR | NACK INTERNAL_ERROR | handler_error_count++ |
| UART RX 溢出 | PC 高速发送 + MCU 慢消费 | 系统不崩 | rx_ring_overflow++ |

---

### 8.12 Stage 3 V1 Implementation Plan

推荐实现顺序：

| Step | Task | Output |
| --- | --- | --- |
| 3.1 | 诊断设计文档 | `docs/05_diagnostic_design.md` |
| 3.2 | DiagnosticApp skeleton | `diagnostic_app.h/.c` |
| 3.3 | GET_HEALTH | 返回系统健康摘要 |
| 3.4 | GET_ERROR_COUNTERS | 返回错误计数器摘要 |
| 3.5 | GET_BUFFER_STATS | 返回 buffer / queue 压力摘要 |
| 3.6 | GET_LAST_RECORDS | 返回最近命令 / 事件 / 错误 |
| 3.7 | Python CLI test | `proto_diagnostic_test.py` |
| 3.8 | UI Diagnostic buttons | HEALTH / ERRORS / BUFFERS / LAST |
| 3.9 | Fault injection tests | unknown cmd / crc / event queue full / handler error |
| 3.10 | 文档与进度同步 | 更新 development progress |

---

### 8.13 Stage 3 V1 Acceptance Criteria

Stage 3 V1 完成标准：

```text
1. DiagnosticApp 可以初始化并周期运行
2. DiagnosticApp 通过 McuInfoApp_RegisterCommand() 注册诊断命令
3. PC 可以查询 GET_HEALTH
4. PC 可以查询 GET_ERROR_COUNTERS
5. PC 可以查询 GET_BUFFER_STATS
6. PC 可以查询 GET_LAST_RECORDS
7. 未知命令注入后，Error View 能反映 unknown_cmd 增长
8. CRC 错误注入后，Error View 或 Pipeline View 能反映 parser/crc error 增长
9. Event queue full 注入后，Buffer View 能反映 event_drop 增长
10. handler error 注入后，Error View 能反映 handler_error 增长
11. UI 可以显示基础诊断结果
12. 诊断命令不会明显扰动 UART 协议热路径
```

---

### 8.14 Design Rules

```text
1. 诊断视图用于回答工程问题，不用于展示所有变量
2. 诊断数据优先来自各模块已有 stats
3. 不重复维护已有模块已经维护的计数器
4. DiagnosticApp 负责聚合与解释，不负责协议收发
5. DiagnosticApp 不直接调用 ProtocolManager
6. DiagnosticApp 命令 handler 通过 McuInfoApp_RegisterCommand() 挂载
7. 所有诊断命令第一版使用 ASCII payload，便于 PC/UI 调试
8. 后续再考虑二进制 TLV payload
9. 诊断命令不得频繁 printf
10. 诊断命令不得大量占用栈空间
11. 大型临时 buffer 应放入模块上下文或使用静态缓冲
12. 每个诊断视图都应至少有一种人为注入方式验证
13. Health View 应只给结论和关键摘要
14. Error View 应按错误类型分类
15. Buffer View 必须包含 high watermark / overflow / drop
16. Timing View 后续使用 DWT cycle counter 增强
17. Last Records View 后续可升级为 trace ring
18. Stage 3 V1 先小步闭环，不追求大而全
```

---

### 8.15 Current Decision

当前 Stage 3 设计决策：

```text
1. 新增 DiagnosticApp 作为诊断聚合中心
2. 诊断命令从 0x20 开始
3. Stage 3 V1 优先实现 GET_HEALTH / GET_ERROR_COUNTERS / GET_BUFFER_STATS / GET_LAST_RECORDS
4. 第一版诊断 payload 使用 ASCII 字符串
5. 不急于实现完整 trace ring
6. 不急于实现 HardFault snapshot
7. 不急于实现 Timing 精细 profiling
8. 先基于已有 stats 建立诊断视图
9. 每个视图必须有对应错误注入测试
10. Stage 3 的目标是工程定位能力，而不是日志展示能力
```

---

## 9. Stage 4: IMU and Algorithm Loop

### 9.1 Stage Goal

建立硬件传感器数据流和物理算法闭环。

该阶段在 Stage 2 通信框架和 Stage 3 诊断框架的基础上，引入 BMI088 六轴 IMU，完成从硬件采样、BSP 驱动、App 状态机、姿态解算、命令查询、事件上报到 PC 可视化的完整闭环。

本阶段重点不是单纯读取 IMU 数据，而是建立一个可扩展的 IMU 子系统：


Platform SPI
↓
BMI088 BSP
↓
ImuApp
↓
Attitude Estimator
↓
CommandService / McuInfoApp / DiagnosticApp
↓
PC IMU Dashboard / 3D Attitude View


---

### 9.2 Current Design

#### 9.2.1 Driver Layer

IMU 驱动分为两层：


platform_spi

只负责通用 SPI 阻塞收发
只管理 SPI 设备号、CS 控制、SPI 统计
不包含 BMI088 寄存器语义

bsp_bmi088

负责 BMI088 寄存器定义
负责 Accel / Gyro Chip ID 检测
负责 BMI088 初始化
负责加速度计、陀螺仪、温度 raw 数据读取

该分层保证后续更换 IMU 或扩展其他 SPI 设备时，不需要破坏 Platform 层。

---

#### 9.2.2 IMU App Layer

`ImuApp` 作为 Stage 4 的业务中心，负责：

管理 IMU 状态机
周期读取 BMI088 原始数据
维护 raw / scaled / attitude 缓存
调用姿态解算算法
维护 IMU 运行统计
注册 IMU 命令
通过 McuInfoApp 上报 IMU EVENT
为 DiagnosticApp 后续聚合预留接口

状态机复用通用 `StateMachine` 模块，不再为 IMU 单独写一套状态管理逻辑。

---

#### 9.2.3 Algorithm Layer

当前姿态解算第一版采用六轴互补滤波：


roll / pitch:
gyro 积分 + accel 重力方向修正

yaw:
gyro 积分


由于 BMI088 是六轴 IMU，不包含磁力计，因此：


roll / pitch 在静止和低动态场景下相对可信
yaw 只能短时间观察，会随时间漂移


后续可在该接口基础上扩展 Mahony / Madgwick / EKF 等姿态算法。

---

#### 9.2.4 PC Visualization

当前不再规划 LCD 显示，所有可视化优先通过 PC 上位机完成。

PC 端 IMU Dashboard 包含：

IMU 控制按钮
IMU 原始数据 / 姿态数据 / 算法统计显示
roll / pitch / yaw 实时曲线
3D 姿态显示区域
IMU EVENT 日志

LCD 显示从本阶段移除，后续如有独立屏幕需求，可作为硬件显示扩展任务重新规划。

---

### 9.3 Planned Tasks

| 任务 | 状态 | 说明 |
| --- | --- | --- |
| SPI2 CubeMX 配置 | Done | 使用 SPI2，阻塞通信，软件 CS |
| Platform SPI 层 | Done | 通用 SPI 设备访问，不包含 BMI088 语义 |
| BMI088 BSP 驱动适配 | Done | 已完成 Chip ID、初始化、raw 数据读取 |
| BMI088 原始数据验证 | Done | Acc ID = 0x1E，Gyro ID = 0x0F，raw 数据稳定 |
| IMU App 状态机 | Done | 复用通用 StateMachine，支持 READY / RUNNING / ERROR |
| IMU 周期采样 | Done | ImuApp_Run 周期读取 BMI088 数据 |
| 数据时间戳 | Done | raw / attitude 数据带 tick_ms |
| 姿态解算第一版 | Done | 已实现 6-axis complementary filter |
| Quaternion 输出接口 | Done | 由 roll / pitch / yaw 换算得到 q0/q1/q2/q3 |
| IMU 命令注册 | Done | 通过 McuInfoApp / CommandService 注册 IMU 命令 |
| IMU 命令查询 | Done | 支持 GET_RAW / GET_FILTERED / GET_STATUS / GET_ATTITUDE / GET_STATS |
| IMU START / STOP | Done | 支持启动和停止 IMU 采样任务 |
| IMU EVENT Stream | Done | 支持 START_STREAM / STOP_STREAM，姿态 EVENT 上报 |
| PC IMU 测试脚本 | Done | 已完成 proto_imu_test.py |
| PC IMU Dashboard | Done | 已完成第二界面、按钮、曲线、数据解析 |
| PC 3D 姿态显示 | In Progress | 已接入 OpenGL 3D Widget，需继续调试坐标方向 |
| Algorithm Stats | Done | 已统计采样次数、算法次数、耗时、错误次数 |
| 一阶低通滤波 | Not Started | 后续用于 raw / scaled 数据平滑 |
| 中值滤波 | Not Started | 后续用于抗尖峰数据 |
| 零偏估计 | Not Started | 静止状态下估计 gyro bias |
| 静止检测 | Not Started | 基于 acc norm 和 gyro norm 判断静止 |
| 异常样本检测 | Planned | 检测突变、超范围、时间间隔异常 |
| DiagnosticApp 聚合 IMU 状态 | Planned | 将 IMU stats 接入 Stage 3 诊断框架 |
| McuInfoApp IMU Snapshot | Planned | 后续将 IMU 状态作为 snapshot 快速查询 |
| Stream 通道 | Planned | 当前先走 EVENT，后续高频数据迁移到 Stream |
| 姿态算法升级 | Planned | 后续扩展 Mahony / Madgwick / EKF |
| 坐标系标定 | Planned | 校正 roll / pitch / yaw 与 PC 3D 坐标方向关系 |
| PC 3D UI 优化 | Planned | 增加 Reset View、Reset Attitude、模型方向校准 |

---

### 9.4 IMU Command List

当前 IMU 命令域为：


0x30 IMU_GET_RAW
0x31 IMU_GET_FILTERED
0x32 IMU_GET_STATUS
0x36 IMU_GET_ALGO_STATS
0x37 IMU_SET_SAMPLE_RATE
0x38 IMU_START_STREAM
0x39 IMU_STOP_STREAM
0x3A IMU_START
0x3B IMU_STOP
0x3C IMU_GET_ATTITUDE
0x3D IMU_CLEAR_STATS


---

### 9.5 IMU Event List

当前 IMU EVENT 初步规划：


0x90 IMU_EVENT_STARTED
0x91 IMU_EVENT_STOPPED
0x95 IMU_EVENT_ERROR
0x97 IMU_EVENT_ATTITUDE
0x98 IMU_EVENT_RAW_SAMPLE


payload 格式：


att,t=123456,r=123,p=-45,y=2340


---

### 9.6 Data Flow

#### 9.6.1 Command Query Path


PC Button / Python Script
↓
ProtocolManager
↓
CommandManager
↓
CommandService
↓
ImuApp Command Handler
↓
RESP
↓
PC Dashboard


#### 9.6.2 Event Stream Path


ImuApp_Run
↓
BspBmi088_ReadRaw
↓
AttitudeEstimator_Update6Axis
↓
McuInfoApp_PostEvent
↓
McuInfoApp_Run
↓
ProtocolManager_Process
↓
EVENT Frame
↓
PC Dashboard / 3D View


---

### 9.7 Acceptance Criteria

BMI088 Accel Chip ID 稳定读取为 0x1E
BMI088 Gyro Chip ID 稳定读取为 0x0F
IMU 原始数据能够稳定输出
静止状态下 gyro raw 接近零附近小幅波动
静止状态下 accel 某一轴能够反映重力方向
PC 可以通过命令查询 IMU raw 数据
PC 可以通过命令查询 roll / pitch / yaw
PC 可以通过命令查询 IMU algorithm stats
IMU START / STOP 命令能够正确切换 App 状态
IMU START_STREAM / STOP_STREAM 能够控制 EVENT 上报
EVENT 姿态流可以被 PC 正确解析
PC IMU Dashboard 可以显示数值和姿态曲线
PC 3D UI 可以根据 roll / pitch / yaw 实时更新模型姿态
算法执行时间可以统计
采样间隔可以统计
异常读取和算法错误能够计数
IMU 关键状态可以通过 McuInfoApp 暴露给 PC

---

### 9.8 Future Extensions

一阶低通滤波 / 中值滤波 / 滑动平均滤波 / 尖峰剔除
Gyro 零偏估计 / 校准 / 清除
静止检测
高级姿态算法（Mahony / Madgwick / EKF / Error-State）
Stream 通道用于高频 raw / scaled 数据
PC 3D UI 增强（Reset View / Reset Attitude / 坐标校准 / 模型方向 / 数据录制）

---






## 10. Stage 5: FDCAN Communication

### 10.1 Stage Goal

建立 FDCAN 通信与诊断能力。

---

### 10.2 Planned Tasks

| 任务                      | 状态          | 说明                    |
| ----------------------- | ----------- | --------------------- |
| FDCAN 初始化               | Not Started | 配置 FDCAN kernel clock |
| CAN RX FIFO             | Not Started | 接收队列                  |
| CAN TX Queue            | Not Started | 发送队列                  |
| CAN Message Queue       | Not Started | 上层消息分发                |
| FDCAN Stats             | Not Started | rx/tx/error/bus-off   |
| CAN 压力测试                | Not Started | 高频收发测试                |
| CAN 异常测试                | Not Started | 总线断开、错误帧等             |
| McuInfoApp CAN Snapshot | Planned     | CAN 状态后续通过信息中枢上报 PC   |

---

### 10.3 Acceptance Criteria

```text
1. 能发送 CAN 帧
2. 能接收 CAN 帧
3. CAN 错误状态可查询
4. 高频收发不死机
5. 总线异常能记录并恢复
6. CAN 统计可通过 PC 查询
```

---

## 11. Stage 6: Parameter and Flash System

### 11.1 Stage Goal

建立可靠的参数管理与非易失存储。

---

### 11.2 Planned Tasks

| 任务                  | 状态          | 说明                      |
| ------------------- | ----------- | ----------------------- |
| Parameter Table     | Not Started | 定义参数 ID、类型、默认值          |
| Param Get/Set       | Not Started | 参数读写接口                  |
| Flash Layout        | Not Started | 参数区地址规划                 |
| A/B Backup          | Not Started | 双备份                     |
| CRC32               | Not Started | 防随机损坏                   |
| Version / Sequence  | Not Started | 版本兼容和新旧选择               |
| Restore Default     | Not Started | 恢复默认参数                  |
| Param Import/Export | Not Started | PC 工具导入导出               |
| Parameter Event     | Planned     | 参数修改后通过 McuInfoApp 上报事件 |

---

### 11.3 Acceptance Criteria

```text
1. 参数可以在线读写
2. 参数可以保存到 Flash
3. 重启后参数保持
4. 单个参数区损坏可从备份恢复
5. 双区损坏可恢复默认
6. 写入中断不会导致设备不可用
7. 参数修改事件可以上报 PC
```

---

## 12. Stage 7: Bootloader and Upgrade

### 12.1 Stage Goal

实现可靠、不变砖的固件升级系统。

---

### 12.2 Planned Tasks

| 任务              | 状态          | 说明                       |
| --------------- | ----------- | ------------------------ |
| Bootloader 工程   | Not Started | 独立工程                     |
| App 工程          | Not Started | 独立应用工程                   |
| Flash 分区        | Not Started | Bootloader / App / Param |
| App Jump        | Not Started | 跳转 App                   |
| VTOR 设置         | Not Started | 向量表重定位                   |
| Firmware Header | Not Started | 固件头                      |
| File Transfer   | Not Started | 分包传输                     |
| CRC/SHA Check   | Not Started | 完整性校验                    |
| App Valid       | Not Started | 固件有效标志                   |
| App Confirmed   | Not Started | App 自确认                  |
| Upgrade Timeout | Not Started | 升级超时处理                   |
| Upgrade Event   | Planned     | 升级状态通过 McuInfoApp 上报 PC  |

---

### 12.3 Acceptance Criteria

```text
1. Bootloader 可以跳转 App
2. App 可以请求进入 Bootloader
3. PC 可以升级 App
4. 错误固件不运行
5. 升级中断不变砖
6. App 未 confirmed 不永久生效
7. 升级进度和错误可以上报 PC
```

---

## 13. Stage 8: Security

### 13.1 Stage Goal

建立基础可信机制。

---

### 13.2 Planned Tasks

| 任务                          | 状态          | 说明                      |
| --------------------------- | ----------- | ----------------------- |
| Crypto Service              | Not Started | SHA/HMAC/ECDSA 接口       |
| HMAC Dangerous Command Auth | Not Started | 危险命令认证                  |
| Challenge-Response          | Not Started | 防止明文密码                  |
| Replay Protection           | Not Started | nonce / counter         |
| Parameter HMAC              | Not Started | 防参数篡改                   |
| Firmware Signature          | Not Started | 固件签名验证                  |
| Security Event Log          | Not Started | 安全事件记录                  |
| Security Test               | Not Started | 安全测试报告                  |
| Security Event Report       | Planned     | 安全事件通过 McuInfoApp 上报 PC |

---

### 13.3 Acceptance Criteria

```text
1. 未认证危险命令被拒绝
2. 重放命令被拒绝
3. 参数篡改可发现
4. 未签名固件不能运行
5. 安全事件可查询
6. 安全事件可上报 PC
```

---

## 14. Stage 9: Low Power and Hardware Diagnostics

### 14.1 Stage Goal

探索 DM-MC02 / STM32H723 的硬件状态和低功耗能力。

---

### 14.2 Planned Tasks

| 任务                           | 状态          | 说明              |
| ---------------------------- | ----------- | --------------- |
| Power Manager                | Not Started | 功耗模式管理          |
| Sleep Mode                   | Not Started | 基础睡眠            |
| Stop Mode                    | Not Started | 深度低功耗           |
| Wakeup Reason                | Not Started | 唤醒原因            |
| Clock Restore                | Not Started | 唤醒后时钟恢复         |
| UART Restore                 | Not Started | 唤醒后串口恢复         |
| FDCAN Restore                | Not Started | 唤醒后 CAN 恢复      |
| LCD/IMU Power Control        | Not Started | 外设功耗观察          |
| Hardware Diagnostic Commands | Not Started | 查询硬件状态          |
| Power Event                  | Planned     | 低功耗进入/退出事件上报 PC |

---

### 14.3 Acceptance Criteria

```text
1. 能进入并退出 Sleep/Stop
2. 唤醒后 UART 正常
3. 唤醒后系统状态正确
4. 功耗变化可以被测量和解释
5. 硬件状态可通过 PC 查询
6. 低功耗事件可上报 PC
```

---

## 15. Stage 10: Chaos and Automated Test

### 15.1 Stage Goal

通过异常注入验证系统鲁棒性。

---

### 15.2 Planned Tasks

| 任务                     | 状态          | 说明                        |
| ---------------------- | ----------- | ------------------------- |
| Protocol Fuzz Test     | Not Started | 随机帧、错误帧                   |
| Buffer Stress Test     | Not Started | RX/TX/Log/Trace 压力        |
| Upgrade Interrupt Test | Not Started | 升级中断                      |
| Flash Fault Injection  | Not Started | 模拟写失败                     |
| HardFault Test         | Not Started | 主动触发                      |
| Watchdog Test          | Not Started | 卡死测试                      |
| Security Test          | Not Started | 重放、错误 HMAC、非法命令           |
| Automated Report       | Not Started | 自动生成测试报告                  |
| Chaos Event Report     | Planned     | 异常注入结果通过 McuInfoApp 上报 PC |

---

### 15.3 Acceptance Criteria

```text
1. 异常输入不导致未知死机
2. 错误能够被记录
3. 系统能够恢复或进入安全状态
4. 测试结果可自动生成报告
5. 关键异常有 Trace 或 Blackbox 记录
6. 异常事件可通过 PC 查询或监听
```

---

## 16. Current Sprint

当前 Sprint：

```text
Sprint 2: UART Protocol V1 and McuInfoApp Event Path
```

### 16.1 Sprint Goal

完成 UART Reliable Protocol V1 的基础通信闭环和主动事件上报链路。

本 Sprint 的核心目标：

```text
1. 建立 ProtocolFrame Parser
2. 建立 ProtocolManager
3. 建立 CommandManager
4. 建立 McuInfoApp 信息中枢
5. 支持 PC 查询命令
6. 支持 MCU 主动 EVENT 上报
7. 完成 PC 侧基础测试、命令测试、健壮性测试和事件监听测试
```

---

### 16.2 Sprint Tasks

| 任务                               | 状态      |
| -------------------------------- | ------- |
| UART DMA RX + RingBuffer         | Done    |
| CRC16                            | Done    |
| Generic StateMachine             | Done    |
| ProtocolFrame Parser             | Done    |
| ProtocolFrame Self Test          | Done    |
| ProtocolManager                  | Done    |
| CommandManager                   | Done    |
| McuInfoApp                       | Done    |
| PC Ping Test                     | Done    |
| PC Command Test                  | Done    |
| PC Robust Test                   | Done    |
| PC Event Listen Test             | Done    |
| BOOT EVENT                       | Done    |
| APP_MESSAGE EVENT                | Done    |
| 栈空间问题记录                          | Done    |
| PC 工具兼容异步 EVENT 干扰               | Next    |
| GET_RESET_INFO snapshot 接入       | Planned |
| GET_FAULT_INFO snapshot/event 接入 | Planned |

---

### 16.3 Sprint Exit Criteria

```text
1. PC 可以 ping MCU —— Done
2. PC 可以查询 version/status/time/uart/app stats —— Done
3. Parser 可以处理半包、粘包、垃圾字节、CRC 错误 —— Done
4. MCU 可以主动上报 BOOT EVENT —— Done
5. MCU 可以主动上报 APP_MESSAGE EVENT —— Done
6. CommandManager 和 McuInfoApp 事件队列正常工作 —— Done
7. PC event listener 可以解析 EVENT frame —— Done
8. 当前成果提交到 Git —— Pending
```

---

## 17. Immediate Next Actions

近期最优先任务：

```text
1. 更新 docs/04_uart_reliable_protocol_design.md
2. 更新 docs/11_development_progress.md
3. 提交 Stage 2 UART Protocol V1 当前成果
4. 修改 PC command / robust 测试脚本，使其兼容异步 EVENT 干扰
5. 设计 GET_RESET_INFO snapshot 接入方式
6. 设计 GET_FAULT_INFO snapshot/event 接入方式
7. 梳理协议栈空间使用优化计划
```

下一步开发建议：

```text
Stage 2.12: PC Tool Async Event Compatibility
```

目标：

```text
PC 侧等待命令响应时，如果先收到 EVENT，不应误判失败。
PC 工具需要持续读取帧，跳过 EVENT，直到匹配到目标 seq/cmd 的 RESP/NACK。
```

然后进入：

```text
Stage 2.13: Reset/Fault Snapshot Integration
```

---

## 18. Update Rules

本文档需要在以下情况更新：

```text
1. 每完成一个阶段
2. 每开始一个新阶段
3. 每完成关键模块
4. 每次发现阶段计划需要调整
5. 每次项目目标发生变化
6. 每次 Sprint 结束
7. 每次协议架构发生分层调整
8. 每次完成关键 Git 提交
```

更新时至少修改：

```text
当前阶段
当前 Sprint
Overall Roadmap 状态
对应 Stage 的任务状态
Immediate Next Actions
Git Commit Suggestion
```

---

## 19. Git Commit Suggestion

当前节点建议提交一次 Git，因为 Stage 2 已经完成一个完整小闭环：

```text
1. UART RX DMA + RingBuffer
2. ProtocolFrame Parser
3. ProtocolManager
4. CommandManager
5. McuInfoApp 信息中枢
6. PC 命令查询
7. PC 健壮性测试
8. MCU 主动 EVENT 上报
9. PC EVENT 监听
10. 协议设计文档和进度文档更新
```

提交前检查：

```bash
git status -sb
```

确认不要提交以下文件：

```text
__pycache__/
*.pyc
Keil 临时文件
编译输出目录
日志文件
临时备份文件
```

建议暂存：

```bash
git add docs/04_uart_reliable_protocol_design.md
git add docs/11_development_progress.md

git add firmware/app/Apps/mcu_info_app.h
git add firmware/app/Apps/mcu_info_app.c

git add firmware/app/Services/command_manager.h
git add firmware/app/Services/command_manager.c
git add firmware/app/Services/protocol_manager.h
git add firmware/app/Services/protocol_manager.c

git add firmware/app/Middleware/protocol_frame.h
git add firmware/app/Middleware/protocol_frame.c

git add firmware/app/App/app_main.c

git add pc_tool/h7_uart_ui/proto_ping_test.py
git add pc_tool/h7_uart_ui/proto_command_test.py
git add pc_tool/h7_uart_ui/proto_robust_test.py
git add pc_tool/h7_uart_ui/proto_event_listen.py
```

如果确认 `.gitignore` 已经正确排除编译产物，也可以：

```bash
git add .
```

但提交前必须看：

```bash
git diff --cached --stat
git status -sb
```

建议 commit message：

```bash
git commit -m "feat: add mcu info app event protocol path"
```

推送：

```bash
git push
```

---

## 20. Next Development Focus

提交完成后，下一步开发重点：

```text
Stage 2.12: PC Tool Async Event Compatibility
```

需要解决的问题：

```text
当前 PC 工具在等待命令响应时，可能读到 EVENT。
如果直接把第一帧当成响应，会出现误判。
```

目标行为：

```text
PC 发送 REQ(seq=N, cmd=X)
PC 持续读取帧
如果收到 EVENT：
  打印或缓存 EVENT
  继续等待
如果收到 RESP/NACK 且 seq/cmd 匹配：
  判定该命令完成
如果超时仍未匹配：
  判定 timeout
```

后续阶段：

```text
Stage 2.13: GET_RESET_INFO / GET_FAULT_INFO Snapshot Integration
Stage 2.14: Protocol Stack Usage Optimization
Stage 2.15: Reliability V2 Design
```
