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
```

---

## 2. Project Current Status

当前阶段：

```text
Stage 0: Repository and Documentation
```

当前状态：

```text
In Progress
```

当前重点：

```text
1. 完成项目仓库初始化
2. 完成 GitHub 推送配置
3. 建立 README 和 docs 文档体系
4. 明确项目总体目标和开发路线
5. 为后续 Board Bring-up 做准备
```

---

## 3. Progress Legend

| 状态          | 含义      |
| ----------- | ------- |
| Not Started | 尚未开始    |
| In Progress | 正在进行    |
| Done        | 已完成     |
| Blocked     | 被阻塞     |
| Need Review | 需要复盘或修改 |
| Deferred    | 暂缓      |

---

## 4. Overall Roadmap

| Stage    | 阶段名称                               | 状态          | 主要目标                          |
| -------- | ---------------------------------- | ----------- | ----------------------------- |
| Stage 0  | Repository and Documentation       | Done  | 建立仓库、目录、README、总纲文档           |
| Stage 1  | Board Bring-up                     | Done  | 点亮板子，验证时钟、串口、定时器、复位、HardFault |
| Stage 2  | UART Reliable Protocol             | In Progress | 建立 PC 与 MCU 的可靠串口通信           |
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

### 5.2 Task List

| 任务                   | 状态                 | 说明                                                |
| -------------------- | ------------------ | ------------------------------------------------- |
| 创建本地项目目录             | Done               | 已创建 `dm-mc02-h723-framework`                      |
| 初始化 Git 仓库           | Done               | 已完成 `git init`                                    |
| 创建 GitHub 仓库         | Done               | 仓库名：`dm-mc02-h723-framework`                      |
| 配置 remote origin     | Done               | 已切换为 SSH 地址                                       |
| 解决 GitHub 首次 push 问题 | Done               | 已完成 SSH 443 配置                                    |
| 建立基础目录结构             | Done               | docs、firmware、pc_tool、tools、scripts、tests、release |
| 添加 `.gitignore`      | Done               | 已忽略编译产物、密钥、临时文件                                   |
| 添加 README.md         | Done         | 初稿已准备，需提交                                         |
| 添加 Git 首次推送问题复盘文档    | Done  | 建议保存为 `docs/10_git_first_push_issue_summary.md`   |
| 添加项目总纲文档             | Done         | `docs/00_project_overview.md`                     |
| 添加进度管理文档             | Done         | 当前文档                                              |
| 添加板卡信息文档             | Done         | `docs/01_board_notes.md`                          |
| 添加芯片能力文档             | Done         | `docs/02_chip_notes.md`                           |
| 添加软件架构文档             | Done         | `docs/03_software_architecture.md`                |

### 5.3 Stage 0 Acceptance Criteria

Stage 0 完成标准：

```text
1. GitHub 仓库可以正常 push / pull
2. README.md 描述清楚项目定位
3. docs/00_project_overview.md 完成项目总纲
4. docs/01_board_notes.md 建立板卡信息骨架
5. docs/02_chip_notes.md 建立芯片能力骨架
6. docs/03_software_architecture.md 建立软件架构骨架
7. docs/11_development_progress.md 可持续跟踪项目进度
8. 每个文档至少有初始版本
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
| Git 提交 Platform/BSP 模块化       | Done  | 待提交 `platform_time`、`platform_uart`、`platform_reset`、`platform_fault`、`board_log`      |
| Git 提交 App 层骨架                | Done  | 待提交 `App/app_main.c`、`App/app_main.h` 和 `main.c` 清理                                    |
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
后续 Services / Managers 应建立在稳定的 Platform/BSP 基础之上。
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
```

默认测试宏状态：

```c
#define ENABLE_SOFTWARE_RESET_TEST   0
#define ENABLE_HARDFAULT_TEST        0
#define ENABLE_DWT_TEST              1
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
11. 代码提交到 GitHub —— Done
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
```

这意味着项目已经从“功能跑通”进入“可观测、可诊断、可沉淀”的阶段。

下一步建议进入：

```text
Stage 1.6: App Layer Skeleton
```

目标是创建：

```text
App/app_main.h
App/app_main.c
```

并把当前 `main.c` 中的启动流程迁移到：

```c
App_Init();
App_Run();
```

使 `main.c` 逐步变成一个干净的系统入口。


---
## 7. Stage 2: UART Reliable Protocol

### 7.1 Stage Goal

建立 PC 与 MCU 之间的可靠 UART 通信通道。

Stage 2 不只是实现简单串口收发，而是要在 Stage 1 已完成的 USART1 生命线基础上，构建一个可扩展、可诊断、可测试、可逐步增强的通信协议框架。

该阶段重点包括：

```text
UART DMA 高效接收
RingBuffer 字节流缓存
通用状态机框架
二进制协议帧
CRC16 校验
Frame Parser
Protocol Manager
Command Manager
基础命令闭环
Python PC Tool
半包 / 粘包 / 错包 / 垃圾字节恢复
后续 ACK / NACK / 重传 / 滑动窗口扩展
```

Stage 2 的核心目标是：

> 把 UART 从“调试输出通道”升级为“可靠命令与数据通信通道”。

---

### 7.2 Protocol Evolution Plan

UART Reliable Protocol 采用分阶段升级路线。

| Version | 目标                              | 说明                                  |
| ------- | ------------------------------- | ----------------------------------- |
| V1      | Reliable Frame + CRC + REQ/RESP | 先实现帧边界、长度、CRC、基础请求响应                |
| V2      | SEQ + ACK/NACK + Timeout Retry  | 加入序号、确认、超时、重传、去重                    |
| V3      | Small Sliding Window            | 加入小窗口机制，提高连续数据传输效率                  |
| V4      | Fragment / File Transfer        | 支持参数导入导出、日志上传、固件分片传输                |
| V5      | Security Extension              | 支持 HMAC、Challenge-Response、防重放、加密预留 |

当前 Stage 2 第一阶段以 V1 为主，同时在协议字段中预留 V2/V3/V4/V5 的升级空间。

---

### 7.3 Planned Tasks

| 任务                            | 状态          | 说明                                                              |
| ----------------------------- | ----------- | --------------------------------------------------------------- |
| UART Reliable Protocol Design | Done        | 已创建 `docs/04_uart_reliable_protocol_design.md`                  |
| Protocol Frame Format         | Designed    | 使用 SOF + VER + TYPE + FLAGS + SEQ + CMD + LEN + PAYLOAD + CRC16 |
| Protocol Evolution Roadmap    | Designed    | 预留 ACK/NACK、重传、小滑动窗口、分片、安全扩展                                    |
| UART DMA + IDLE 接收            | Not Started | H7 需要特别关注 D-Cache 与 DMA 一致性                                     |
| DMA Half Transfer 处理          | Not Started | DMA 写满前半区后搬运数据到 RX RingBuffer                                   |
| DMA Transfer Complete 处理      | Not Started | DMA 写满后半区后搬运数据到 RX RingBuffer                                   |
| UART IDLE 处理                  | Not Started | 处理不定长帧和空闲事件                                                     |
| RX RingBuffer                 | Done | 解耦 DMA 接收与协议解析                                                  |
| RingBuffer 统计                 | Done | overflow、high watermark、read/write bytes                        |
| Generic State Machine         | Done | 通用状态机框架，用于 Frame Parser 和后续模块                                   |
| CRC16                         | Done | CRC16-CCITT-FALSE，用于基础通信校验                                      |
| Frame Parser                  | Not Started | 从字节流解析完整协议帧                                                     |
| Parser Error Recovery         | Not Started | 支持半包、粘包、垃圾字节、CRC 错误恢复                                           |
| Protocol Frame Module         | Not Started | 协议帧编码/解码结构体与工具函数                                                |
| Protocol Manager              | Not Started | 接收、校验、分发、响应、统计                                                  |
| Command Manager               | Not Started | 命令注册、参数检查、执行和响应封装                                               |
| PING 命令                       | Not Started | 最小通信闭环                                                          |
| GET_VERSION 命令                | Not Started | 查询固件版本信息                                                        |
| GET_STATUS 命令                 | Not Started | 查询系统状态                                                          |
| GET_RESET_INFO 命令             | Planned     | 查询 Stage 1 已建立的 reset info                                      |
| GET_TIME_INFO 命令              | Planned     | 查询 tick / DWT / uptime 信息                                       |
| Python PC Tool                | Not Started | `h7tool ping/version/status`                                    |
| 协议压力测试                        | Not Started | 半包、粘包、CRC 错误、垃圾字节、超长帧                                           |
| ACK / NACK                    | Planned     | V2 可靠性增强                                                        |
| Timeout Retry                 | Planned     | V2 可靠性增强                                                        |
| Duplicate Detection           | Planned     | V2 防止重复执行副作用命令                                                  |
| Small Sliding Window          | Planned     | V3 用于大数据传输和固件升级                                                 |
| Fragment Transfer             | Planned     | V4 用于固件升级、日志上传、参数导入导出                                           |
| Security Extension            | Planned     | V5 用于 HMAC、认证、防重放                                               |

---

### 7.4 Planned Module Structure

Stage 2 计划新增以下模块。

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
│   └── command_manager.c
│
├── Platform/
│   ├── platform_uart.h
│   └── platform_uart.c
│
└── App/
    ├── app_main.h
    └── app_main.c
```

PC 工具计划：

```text
pc_tool/
└── h7tool/
    ├── cli.py
    ├── protocol.py
    ├── commands.py
    ├── serial_backend.py
    └── tests.py
```

---

### 7.5 UART RX Architecture

UART 接收计划采用：

```text
UART RX DMA Circular Mode
DMA Half Transfer Interrupt
DMA Transfer Complete Interrupt
UART IDLE Interrupt
RX RingBuffer
Frame Parser
Protocol Manager
```

数据流：

```text
USART1 RX
  ↓
DMA Circular Buffer
  ↓
Half / Complete / IDLE Event
  ↓
RX RingBuffer
  ↓
Frame Parser State Machine
  ↓
Protocol Manager
  ↓
Command Manager
```

设计原则：

```text
DMA callback 中只做最少工作
协议解析不在中断中执行
RingBuffer 解耦接收和解析
Parser 从 RingBuffer 中持续取字节
所有错误必须有计数器
```

---

### 7.6 Generic State Machine Plan

Stage 2 将引入通用状态机框架：

```text
Middleware/state_machine.h
Middleware/state_machine.c
```

用于支撑：

```text
Frame Parser State Machine
Protocol Reliability State Machine
Future Upgrade State Machine
Future Security Authentication State Machine
Device State Machine
```

Frame Parser 计划状态：

```text
WAIT_SOF1
WAIT_SOF2
READ_HEADER
READ_PAYLOAD
READ_CRC
VERIFY_CRC
FRAME_READY
ERROR_RECOVERY
```

通用状态机框架目标：

```text
统一状态切换风格
支持 enter / exit / event handler
支持状态切换统计
支持状态停留时间统计
支持后续诊断查询
```

---

### 7.7 Protocol Frame Summary

第一版协议帧格式：

```text
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
| SOF1 | SOF2  | VER | TYPE | FLAGS | SEQ | CMD | LEN    | PAYLOAD | CRC16 |
| 0xA5 | 0x5A  | 1B  | 1B   | 1B    | 1B  | 1B  | 2B LE  | N bytes | 2B LE |
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
```

CRC 覆盖范围：

```text
VER + TYPE + FLAGS + SEQ + CMD + LEN + PAYLOAD
```

不覆盖：

```text
SOF1
SOF2
CRC16 itself
```

V1 主要使用：

```text
REQ
RESP
NACK
```

后续扩展：

```text
ACK
DATA
WINDOW_ACK
EVENT
```

---

### 7.8 Acceptance Criteria

Stage 2 第一阶段完成标准：

```text
1. UART RX DMA 可以连续接收字节
2. DMA Half Transfer / Transfer Complete / IDLE 事件能够正确搬运数据
3. RX RingBuffer 可以缓存字节流
4. RingBuffer 具备 overflow 和 high watermark 统计
5. Frame Parser 可以从字节流中恢复完整帧
6. CRC16 可以识别错误帧
7. PING / GET_VERSION / GET_STATUS 可以正常响应
8. PC Python Tool 可以发送命令并解析响应
9. 半包场景下 parser 能等待更多数据
10. 粘包场景下 parser 能连续解析多帧
11. 垃圾字节不会导致 parser 长期失效
12. 超长帧不会导致 buffer 越界
13. 通信错误有计数器
```

Stage 2 增强阶段完成标准：

```text
1. SEQ request-response matching works
2. ACK/NACK frame works
3. Timeout retry works
4. Duplicate detection works
5. Small sliding window design is implemented or partially implemented
6. Protocol stress test can be automated by PC Tool
```

---

### 7.9 Current Stage 2 Status

Current status:

```text
Stage 2.4: Generic State Machine Middleware
```

Completed:

```text
RingBuffer middleware
RingBuffer read/write/wraparound test
RingBuffer overflow and high watermark statistics
CRC16 middleware
CRC16-CCITT-FALSE standard test vector
Generic state machine middleware
State transition / dispatch / statistics test
```

Next step:

```text
Stage 2.5: UART DMA RX Path
```

Planned output:

```text
Middleware/ring_buffer.h
Middleware/ring_buffer.c
RingBuffer unit-style test through App_Run or temporary test function
RingBuffer statistics
```


---

## 8. Stage 3: Diagnostic Framework

### 8.1 Stage Goal

建立系统可观测性骨架。

### 8.2 Planned Tasks

| 任务                 | 状态          | 说明             |
| ------------------ | ----------- | -------------- |
| Device Manager     | Not Started | 管理设备状态         |
| Command Manager    | Not Started | 命令注册和分发        |
| Diagnostic Manager | Not Started | 状态、错误、计数器      |
| Buffer Manager     | Not Started | 统一记录 buffer 水位 |
| Log Manager        | Not Started | 非阻塞日志          |
| Trace Manager      | Not Started | 事件追踪           |
| Health Manager     | Not Started | 任务健康上报         |
| Watchdog Manager   | Not Started | 基于健康状态喂狗       |

### 8.3 Acceptance Criteria

```text
1. PC 可以查询系统状态
2. PC 可以查询错误计数器
3. PC 可以查询 buffer 使用情况
4. 状态切换有 Trace
5. 关键任务有健康上报
6. 系统异常有明确错误码
```

---

## 9. Stage 4: IMU and Algorithm Loop

### 9.1 Stage Goal

建立硬件传感器数据流和物理算法闭环。

### 9.2 Planned Tasks

| 任务              | 状态          | 说明                |
| --------------- | ----------- | ----------------- |
| BMI088 驱动适配     | Not Started | 需确认 SPI/I2C 接口    |
| IMU 数据读取        | Not Started | 加速度计、陀螺仪          |
| 数据时间戳           | Not Started | 保证采样时序可分析         |
| 一阶低通滤波          | Not Started | 基础滤波算法            |
| 中值滤波            | Not Started | 抗尖峰               |
| 零偏估计            | Not Started | 静止状态下估计 gyro bias |
| 静止检测            | Not Started | 基于加速度/角速度判断       |
| Algorithm Stats | Not Started | 运行次数、最大耗时、错误次数    |
| LCD/串口显示        | Not Started | 可视化 IMU 状态        |

### 9.3 Acceptance Criteria

```text
1. IMU 原始数据稳定输出
2. 滤波后数据可观察
3. 算法执行时间可统计
4. 静止检测基本有效
5. 异常数据能够被计数
```

---

## 10. Stage 5: FDCAN Communication

### 10.1 Stage Goal

建立 FDCAN 通信与诊断能力。

### 10.2 Planned Tasks

| 任务                | 状态          | 说明                    |
| ----------------- | ----------- | --------------------- |
| FDCAN 初始化         | Not Started | 配置 FDCAN kernel clock |
| CAN RX FIFO       | Not Started | 接收队列                  |
| CAN TX Queue      | Not Started | 发送队列                  |
| CAN Message Queue | Not Started | 上层消息分发                |
| FDCAN Stats       | Not Started | rx/tx/error/bus-off   |
| CAN 压力测试          | Not Started | 高频收发测试                |
| CAN 异常测试          | Not Started | 总线断开、错误帧等             |

### 10.3 Acceptance Criteria

```text
1. 能发送 CAN 帧
2. 能接收 CAN 帧
3. CAN 错误状态可查询
4. 高频收发不死机
5. 总线异常能记录并恢复
```

---

## 11. Stage 6: Parameter and Flash System

### 11.1 Stage Goal

建立可靠的参数管理与非易失存储。

### 11.2 Planned Tasks

| 任务                  | 状态          | 说明             |
| ------------------- | ----------- | -------------- |
| Parameter Table     | Not Started | 定义参数 ID、类型、默认值 |
| Param Get/Set       | Not Started | 参数读写接口         |
| Flash Layout        | Not Started | 参数区地址规划        |
| A/B Backup          | Not Started | 双备份            |
| CRC32               | Not Started | 防随机损坏          |
| Version / Sequence  | Not Started | 版本兼容和新旧选择      |
| Restore Default     | Not Started | 恢复默认参数         |
| Param Import/Export | Not Started | PC 工具导入导出      |

### 11.3 Acceptance Criteria

```text
1. 参数可以在线读写
2. 参数可以保存到 Flash
3. 重启后参数保持
4. 单个参数区损坏可从备份恢复
5. 双区损坏可恢复默认
6. 写入中断不会导致设备不可用
```

---

## 12. Stage 7: Bootloader and Upgrade

### 12.1 Stage Goal

实现可靠、不变砖的固件升级系统。

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

### 12.3 Acceptance Criteria

```text
1. Bootloader 可以跳转 App
2. App 可以请求进入 Bootloader
3. PC 可以升级 App
4. 错误固件不运行
5. 升级中断不变砖
6. App 未 confirmed 不永久生效
```

---

## 13. Stage 8: Security

### 13.1 Stage Goal

建立基础可信机制。

### 13.2 Planned Tasks

| 任务                          | 状态          | 说明                |
| --------------------------- | ----------- | ----------------- |
| Crypto Service              | Not Started | SHA/HMAC/ECDSA 接口 |
| HMAC Dangerous Command Auth | Not Started | 危险命令认证            |
| Challenge-Response          | Not Started | 防止明文密码            |
| Replay Protection           | Not Started | nonce / counter   |
| Parameter HMAC              | Not Started | 防参数篡改             |
| Firmware Signature          | Not Started | 固件签名验证            |
| Security Event Log          | Not Started | 安全事件记录            |
| Security Test               | Not Started | 安全测试报告            |

### 13.3 Acceptance Criteria

```text
1. 未认证危险命令被拒绝
2. 重放命令被拒绝
3. 参数篡改可发现
4. 未签名固件不能运行
5. 安全事件可查询
```

---

## 14. Stage 9: Low Power and Hardware Diagnostics

### 14.1 Stage Goal

探索 DM-MC02 / STM32H723 的硬件状态和低功耗能力。

### 14.2 Planned Tasks

| 任务                           | 状态          | 说明         |
| ---------------------------- | ----------- | ---------- |
| Power Manager                | Not Started | 功耗模式管理     |
| Sleep Mode                   | Not Started | 基础睡眠       |
| Stop Mode                    | Not Started | 深度低功耗      |
| Wakeup Reason                | Not Started | 唤醒原因       |
| Clock Restore                | Not Started | 唤醒后时钟恢复    |
| UART Restore                 | Not Started | 唤醒后串口恢复    |
| FDCAN Restore                | Not Started | 唤醒后 CAN 恢复 |
| LCD/IMU Power Control        | Not Started | 外设功耗观察     |
| Hardware Diagnostic Commands | Not Started | 查询硬件状态     |

### 14.3 Acceptance Criteria

```text
1. 能进入并退出 Sleep/Stop
2. 唤醒后 UART 正常
3. 唤醒后系统状态正确
4. 功耗变化可以被测量和解释
5. 硬件状态可通过 PC 查询
```

---

## 15. Stage 10: Chaos and Automated Test

### 15.1 Stage Goal

通过异常注入验证系统鲁棒性。

### 15.2 Planned Tasks

| 任务                     | 状态          | 说明                 |
| ---------------------- | ----------- | ------------------ |
| Protocol Fuzz Test     | Not Started | 随机帧、错误帧            |
| Buffer Stress Test     | Not Started | RX/TX/Log/Trace 压力 |
| Upgrade Interrupt Test | Not Started | 升级中断               |
| Flash Fault Injection  | Not Started | 模拟写失败              |
| HardFault Test         | Not Started | 主动触发               |
| Watchdog Test          | Not Started | 卡死测试               |
| Security Test          | Not Started | 重放、错误 HMAC、非法命令    |
| Automated Report       | Not Started | 自动生成测试报告           |

### 15.3 Acceptance Criteria

```text
1. 异常输入不导致未知死机
2. 错误能够被记录
3. 系统能够恢复或进入安全状态
4. 测试结果可自动生成报告
5. 关键异常有 Trace 或 Blackbox 记录
```

---

## 16. Current Sprint

当前 Sprint：

```text
Sprint 0: Documentation Foundation
```

### 16.1 Sprint Goal

完成项目文档基础，使项目进入可长期推进状态。

### 16.2 Sprint Tasks

| 任务                               | 状态          |
| -------------------------------- | ----------- |
| README.md                        | In Progress |
| docs/00_project_overview.md      | In Progress |
| docs/11_development_progress.md  | In Progress |
| docs/01_board_notes.md           | Not Started |
| docs/02_chip_notes.md            | Not Started |
| docs/03_software_architecture.md | Not Started |
|   USART1 printf bring-up         |  Done
### 16.3 Sprint Exit Criteria

```text
1. README 可以说明项目目标
2. Project Overview 可以说明项目总规划
3. Development Progress 可以跟踪项目进度
4. Board Notes 建立板卡资源记录入口
5. Chip Notes 建立芯片能力记录入口
6. Software Architecture 建立架构设计入口
```

---

## 17. Immediate Next Actions

近期最优先任务：

```text
1. 提交 README.md
2. 提交 docs/00_project_overview.md
3. 提交 docs/11_development_progress.md
4. 创建 docs/01_board_notes.md
5. 创建 docs/02_chip_notes.md
6. 创建 docs/03_software_architecture.md
7. 创建 feature/board-bringup 分支
8. 开始 CubeMX 基础工程
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
```

更新时至少修改：

```text
当前阶段
当前 Sprint
Overall Roadmap 状态
对应 Stage 的任务状态
Immediate Next Actions
```

---

## 19. Git Commit Suggestion

首次添加本文档时使用：

```bash
git add docs/11_development_progress.md
git commit -m "docs: add development progress tracker"
git push
```

后续更新进度时可以使用：

```bash
git add docs/11_development_progress.md
git commit -m "docs: update development progress"
git push
```
