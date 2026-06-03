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

建立系统可观测性骨架。

该阶段将在 Stage 1 的底层可观测能力和 Stage 2 的 PC 通信能力基础上，建立更系统的诊断框架。

重点包括：

```text
Device Manager
Diagnostic Manager
Buffer Manager
Log Manager
Trace Manager
Health Manager
Watchdog Manager
PC 查询诊断信息
```

---

### 8.2 Planned Tasks

| 任务                             | 状态          | 说明                       |
| ------------------------------ | ----------- | ------------------------ |
| Device Manager                 | Not Started | 管理设备状态                   |
| Diagnostic Manager             | Not Started | 状态、错误、计数器                |
| Buffer Manager                 | Not Started | 统一记录 buffer 水位           |
| Log Manager                    | Not Started | 非阻塞日志                    |
| Trace Manager                  | Not Started | 事件追踪                     |
| Health Manager                 | Not Started | 任务健康上报                   |
| Watchdog Manager               | Not Started | 基于健康状态喂狗                 |
| PC Diagnostic Commands         | Not Started | 通过 UART 协议查询诊断信息         |
| McuInfoApp Diagnostic Snapshot | Planned     | 诊断信息通过 McuInfoApp 汇总到 PC |

---

### 8.3 Acceptance Criteria

```text
1. PC 可以查询系统状态
2. PC 可以查询错误计数器
3. PC 可以查询 buffer 使用情况
4. 状态切换有 Trace
5. 关键任务有健康上报
6. 系统异常有明确错误码
7. 诊断信息可以通过 McuInfoApp 暴露给 PC
```

---

## 9. Stage 4: IMU and Algorithm Loop

### 9.1 Stage Goal

建立硬件传感器数据流和物理算法闭环。

---

### 9.2 Planned Tasks

| 任务                      | 状态          | 说明                  |
| ----------------------- | ----------- | ------------------- |
| BMI088 驱动适配             | Not Started | 需确认 SPI/I2C 接口      |
| IMU 数据读取                | Not Started | 加速度计、陀螺仪            |
| 数据时间戳                   | Not Started | 保证采样时序可分析           |
| 一阶低通滤波                  | Not Started | 基础滤波算法              |
| 中值滤波                    | Not Started | 抗尖峰                 |
| 零偏估计                    | Not Started | 静止状态下估计 gyro bias   |
| 静止检测                    | Not Started | 基于加速度/角速度判断         |
| Algorithm Stats         | Not Started | 运行次数、最大耗时、错误次数      |
| LCD/串口显示                | Not Started | 可视化 IMU 状态          |
| McuInfoApp IMU Snapshot | Planned     | IMU 状态后续通过信息中枢上报 PC |

---

### 9.3 Acceptance Criteria

```text
1. IMU 原始数据稳定输出
2. 滤波后数据可观察
3. 算法执行时间可统计
4. 静止检测基本有效
5. 异常数据能够被计数
6. IMU 关键状态可以通过 McuInfoApp 上报或查询
```

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
