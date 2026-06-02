# Stage 1 Bring-up Summary

## 1. Document Purpose

本文档用于总结 `DM-MC02 H723 Embedded Framework` 项目在 **Stage 1: Board Bring-up** 阶段完成的全部工作。

Stage 1 的核心目标不是实现复杂业务功能，而是完成开发板基础运行能力验证，并建立后续系统开发所依赖的最小可观测、可诊断、可测量能力。

本阶段完成后，项目已经从简单的 CubeMX 工程进入到具备初步分层架构的嵌入式系统框架。

---

## 2. Stage 1 Goal

Stage 1 的目标是验证 DM-MC02 / STM32H723 的基础运行能力。

该阶段重点确认：

```text
1. 板子可以正常启动
2. 工程可以稳定编译、下载和调试
3. USART1 可以作为调试生命线输出日志
4. 系统 Tick 正常
5. 时钟信息可以打印
6. 复位原因可以读取
7. 软件复位可以触发和识别
8. HardFault 可以捕获和解码
9. DWT Cycle Counter 可以用于性能测量
10. 第一批 Platform / BSP / App 层模块可以稳定运行
```

本阶段的核心思想是：

> 先建立“能启动、能观测、能定位故障、能测量耗时”的基础能力，再进入后续可靠通信协议、诊断系统、Bootloader 和安全机制开发。

---

## 3. Development Environment

当前使用的开发环境如下：

| Item                     | Tool           |
| ------------------------ | -------------- |
| MCU Configuration        | STM32CubeMX    |
| Build / Download / Debug | Keil MDK       |
| Code Editing             | VS Code        |
| Version Control          | Git            |
| Remote Repository        | GitHub         |
| Target Board             | Damiao DM-MC02 |
| Target MCU               | STM32H723VGT6  |

当前主要开发分支：

```text
feature/board-bringup
```

---

## 4. Stage 1 Completed Tasks

| Task                          | Status   | Description                                                                            |
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
| LED / GPIO 测试                 | Deferred | 当前板卡无明显用户 LED，暂缓                                                                       |
| Timer GPIO Toggle             | Deferred | 后续确认可用 GPIO 后再做物理频率验证                                                                  |

---

## 5. Runtime Verification Results

Stage 1 中已经完成以下运行时验证：

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
17. Platform/BSP/App 模块化后系统仍能正常启动
```

---

## 6. Current Boot Log Capability

当前系统启动后可以通过 USART1 打印以下信息：

```text
Boot Banner
Build Date / Build Time
Board Name
MCU Name
Console UART
Reset Flags
Primary Reset Cause
Clock Info
Tick Test
Platform Time Status
DWT Cycle Counter Test
Runtime Uptime
```

这说明系统已经具备基本自描述能力。

启动日志不只是调试输出，而是后续诊断系统、Bootloader、远程维护和现场问题分析的基础。

---

## 7. Implemented Module Structure

Stage 1 完成后，项目已经形成第一批 Platform / BSP / App 模块。

当前模块结构如下：

```text
firmware/app/
├── App/
│   ├── app_main.h
│   └── app_main.c
│
├── BSP/
│   ├── board_log.h
│   └── board_log.c
│
├── Platform/
│   ├── platform_time.h
│   ├── platform_time.c
│   ├── platform_uart.h
│   ├── platform_uart.c
│   ├── platform_reset.h
│   ├── platform_reset.c
│   ├── platform_fault.h
│   └── platform_fault.c
│
├── Core/
├── Drivers/
└── MDK-ARM/
```

---

## 8. Current Layer Responsibility

### 8.1 Core Layer

`Core/` 主要由 CubeMX 生成。

当前职责：

```text
HAL_Init()
SystemClock_Config()
MX_GPIO_Init()
MX_USART1_UART_Init()
中断入口
启动文件
CubeMX 外设初始化
```

原则：

```text
Core 层尽量不承载复杂业务逻辑。
main.c 只作为系统启动入口。
```

---

### 8.2 App Layer

当前已建立：

```text
App/app_main.h
App/app_main.c
```

当前职责：

```text
App_Init()
App_Run()
启动流程组织
周期运行入口
测试宏入口管理
后续 Manager 调度入口
```

`App_Init()` 当前负责：

```text
PlatformUart_Init
PlatformTime_Init
BoardLog_Init
BoardLog_PrintBootBanner
PlatformReset_Capture
PlatformReset_PrintInfo
PlatformReset_ClearFlags
Clock Info print
Tick test
DWT test
```

`App_Run()` 当前负责：

```text
周期性 uptime 打印
Software Reset Test 入口
HardFault Test 入口
后续 manager process 调度入口
```

---

### 8.3 Platform Layer

Platform 层负责 MCU / Cortex-M / STM32 底层能力抽象。

当前已完成模块：

```text
platform_time
platform_uart
platform_reset
platform_fault
```

#### platform_time

职责：

```text
HAL tick wrapper
DWT cycle counter
cycles to us/ns
profile start/end
delay wrapper
DWT self-test
```

代表接口：

```c
void PlatformTime_Init(void);
uint32_t PlatformTime_GetMs(void);
uint32_t PlatformTime_GetCycle(void);
uint32_t PlatformTime_CyclesToUs(uint32_t cycles);
uint32_t PlatformTime_CyclesToNs(uint32_t cycles);
uint32_t PlatformTime_ProfileStart(void);
uint32_t PlatformTime_ProfileEndCycles(uint32_t start_cycle);
uint32_t PlatformTime_ProfileEndUs(uint32_t start_cycle);
void PlatformTime_DelayMs(uint32_t ms);
void PlatformTime_PrintStatus(void);
void PlatformTime_RunDwtTest(void);
```

#### platform_uart

职责：

```text
USART1 blocking transmit
printf retarget backend
send byte
send buffer
send string
HAL UART status translation
```

代表接口：

```c
void PlatformUart_Init(void);
int PlatformUart_SendByte(uint8_t byte);
int PlatformUart_SendBuffer(const uint8_t *buf, uint16_t len);
int PlatformUart_SendString(const char *str);
```

#### platform_reset

职责：

```text
RCC reset flags capture
primary reset cause detection
software reset
reset flags clear
reset info print
```

代表接口：

```c
void PlatformReset_Capture(PlatformResetInfo_t *info);
void PlatformReset_ClearFlags(void);
void PlatformReset_SoftwareReset(void);
uint8_t PlatformReset_IsSoftwareReset(const PlatformResetInfo_t *info);
const char *PlatformReset_CauseToString(PlatformResetCause_t cause);
void PlatformReset_PrintInfo(const PlatformResetInfo_t *info);
```

#### platform_fault

职责：

```text
HardFault stack frame capture
SCB fault registers capture
CFSR decode
HFSR decode
HardFault info print
C-level HardFault handler
```

代表接口：

```c
void PlatformFault_Capture(uint32_t *stack_frame, PlatformFaultInfo_t *info);
void PlatformFault_PrintInfo(const PlatformFaultInfo_t *info);
void PlatformFault_PrintHfsrDecode(uint32_t hfsr);
void PlatformFault_PrintCfsrDecode(uint32_t cfsr);
void PlatformFault_HandlerC(uint32_t *stack_frame);
```

---

### 8.4 BSP Layer

BSP 层负责板级身份、板级资源策略和板级日志输出。

当前已完成模块：

```text
board_log
```

#### board_log

职责：

```text
Boot banner
Board name print
MCU name print
Console info print
INFO/WARN/ERROR log prefix
log separator
```

代表接口：

```c
void BoardLog_Init(void);
void BoardLog_PrintSeparator(void);
void BoardLog_PrintBootBanner(void);
void BoardLog_Info(const char *fmt, ...);
void BoardLog_Warn(const char *fmt, ...);
void BoardLog_Error(const char *fmt, ...);
```

---

## 9. Dependency Direction

当前依赖关系如下：

```text
Core/main.c
  ↓
App/app_main.c
  ↓
BSP/board_log
Platform/platform_time
Platform/platform_uart
Platform/platform_reset

Core/stm32h7xx_it.c
  ↓
Platform/platform_fault
```

更具体的输出链路：

```text
printf
  ↓
fputc / __io_putchar
  ↓
PlatformUart_SendByte
  ↓
HAL_UART_Transmit
  ↓
USART1
```

HardFault 链路：

```text
HardFault_Handler
  ↓
PlatformFault_HandlerC
  ↓
PlatformFault_Capture
  ↓
PlatformFault_PrintInfo
  ↓
printf / PlatformUart
```

Reset 诊断链路：

```text
App_Init
  ↓
PlatformReset_Capture
  ↓
PlatformReset_PrintInfo
  ↓
PlatformReset_ClearFlags
```

---

## 10. main.c Cleanup Result

Stage 1 前期，`main.c` 中直接包含了较多 bring-up 逻辑：

```text
printf retarget
boot banner
reset reason decode
software reset test
clock info print
DWT test
HardFault capture
fault decode
uptime print
```

Stage 1 收尾后，`main.c` 目标形态已经变为：

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    App_Init();

    while (1)
    {
        App_Run();
    }
}
```

这说明 `main.c` 已经从功能堆叠入口，逐步转变为干净的系统入口。

---

## 11. Key Debug Conclusions

### 11.1 printf Retarget

Keil ARM Compiler 6 可能定义 `__GNUC__`，因此单纯依赖：

```c
#ifdef __GNUC__
```

来区分 `__io_putchar()` 和 `fputc()` 路径可能导致判断不符合预期。

最终方案：

```text
同时提供 fputc()
同时提供 __io_putchar()
二者统一调用 PlatformUart_SendByte()
```

这样可以兼容 Keil / ARMClang / GCC 类工具链。

---

### 11.2 Reset Flags

STM32 reset flags 不是互斥枚举，而是一组粘滞标志位。

软件复位后可能看到：

```text
PINRST = SET
SFTRST = SET
```

这不代表软件复位失败，而是说明多个 reset flag 可以同时置位。

当前策略：

```text
1. 启动后读取原始 reset flags
2. 打印所有 flags
3. 根据优先级推断 Primary Reset Cause
4. 读取和打印后清除 reset flags
```

当前主复位原因优先级：

```text
IWDG / WWDG > Software > BOR > POR > PIN > Unknown
```

---

### 11.3 HardFault Capture

当前 HardFault 处理流程已经验证：

```text
1. 通过非法访问主动触发 HardFault
2. 进入自定义 HardFault_Handler
3. 判断异常前使用 MSP / PSP
4. 传递 stack_frame 到 PlatformFault_HandlerC()
5. 解析 R0/R1/R2/R3/R12/LR/PC/xPSR
6. 读取 CFSR/HFSR/DFSR/AFSR/MMFAR/BFAR
7. 自动解码 CFSR/HFSR
8. 串口打印故障现场
```

测试示例：

```c
volatile uint32_t *bad_addr = (uint32_t *)0xFFFFFFFFU;
*bad_addr = 0x12345678U;
```

测试结论：

```text
该测试触发 UsageFault: UNALIGNED。
HFSR.FORCED 置位，说明 UsageFault 被升级为 HardFault。
```

---

### 11.4 PC Address Location

HardFault 现场中最重要的是：

```text
PC
LR
CFSR
HFSR
```

其中：

```text
PC = 发生异常时 CPU 正在执行的指令地址
LR = 异常发生前的返回地址或调用链相关地址
```

后续可通过以下方式定位 PC：

```text
1. Keil Disassembly 窗口
2. map 文件
3. debug symbol
4. 断点复现
```

相关方法已经记录在：

```text
docs/12_fault_diagnosis_and_pc_location.md
```

---

### 11.5 DWT Cycle Counter

当前已经验证：

```text
DWT->CYCCNT 可以正常计数
cycles 可以根据 HCLK 换算为 us/ns
DWT 可以用于函数执行耗时测量
```

该能力后续将用于：

```text
协议解析耗时统计
命令分发耗时统计
算法执行耗时统计
任务调度耗时统计
Manager 最大执行时间统计
性能诊断
```

---

## 12. Safe Default Test Macros

Stage 1 收尾时，测试宏应保持安全默认状态：

```c
#define ENABLE_SOFTWARE_RESET_TEST   0
#define ENABLE_HARDFAULT_TEST        0
#define ENABLE_DWT_TEST              1
```

说明：

```text
ENABLE_SOFTWARE_RESET_TEST = 0，避免启动后自动软件复位。
ENABLE_HARDFAULT_TEST = 0，避免启动后主动触发 HardFault。
ENABLE_DWT_TEST = 1，保留 DWT 自检输出，便于持续观察性能计数能力。
```

---

## 13. Deferred Items

以下内容在 Stage 1 中暂缓：

| Item                | Reason                            |
| ------------------- | --------------------------------- |
| LED / GPIO 测试       | 当前板卡无明确用户 LED                     |
| Timer GPIO Toggle   | 需要后续确认可用 GPIO 或测试点                |
| Watchdog Reset Test | 放到后续 Health / Watchdog 阶段         |
| UART DMA            | 放到 Stage 2 UART Reliable Protocol |
| RingBuffer          | 放到 Stage 2                        |
| FDCAN               | 放到 Stage 5                        |
| IMU                 | 放到 Stage 4                        |
| Bootloader          | 放到 Stage 7                        |
| Security            | 放到 Stage 8                        |

---

## 14. Stage 1 Acceptance Criteria

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
10. 第一批 Platform/BSP/App 模块稳定运行 —— Done
11. main.c 已初步清理为系统入口 —— Done
12. 代码提交到 GitHub —— Done
```

暂缓项：

```text
LED 可以周期翻转 —— Deferred
Timer 输出频率与配置一致 —— Deferred
```

---

## 15. Stage 1 Final Conclusion

Stage 1 已经完成最关键的底层 Bring-up 和可观测能力建设。

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
初步 Platform/BSP/App 分层
```

这意味着项目已经完成了从“功能跑通”到“可观测、可诊断、可沉淀”的第一阶段转变。

当前工程已经具备进入下一阶段的基础条件。

---

## 16. Ready for Stage 2

下一阶段为：

```text
Stage 2: UART Reliable Protocol
```

Stage 2 的目标是基于当前 USART1 生命线，构建可靠的 PC ↔ MCU 通信协议。

Stage 2 计划重点包括：

```text
1. UART RX path
2. RingBuffer
3. Protocol frame format
4. CRC16
5. PING / GET_VERSION / GET_STATUS
6. Command dispatch
7. PC Python Tool
8. Protocol robustness test
9. Half packet / sticky packet / CRC error recovery
10. Future ACK / NACK / SEQ / retry support
```

进入 Stage 2 前，需要确保：

```text
1. Stage 1 测试宏处于安全默认状态
2. feature/board-bringup 分支已提交
3. main.c 已保持简洁
4. Platform/BSP/App 模块可以稳定编译运行
5. USART1 输出正常
```

---

## 17. Related Documents

相关文档：

```text
docs/00_project_overview.md
docs/03_software_architecture.md
docs/11_development_progress.md
docs/12_fault_diagnosis_and_pc_location.md
docs/13_stage1_bringup_summary.md
```

后续 Stage 2 将重点维护：

```text
docs/04_protocol_design.md
```
