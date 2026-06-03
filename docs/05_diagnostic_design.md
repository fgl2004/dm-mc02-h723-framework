# Diagnostic Framework Design

## 1. 文档目的

本文档用于指导 `DM-MC02 H723 Embedded Framework` 项目 **Stage 3: Diagnostic Framework** 的设计与开发。

Stage 2 已经基本完成 UART Reliable Protocol 的基础闭环：

```text
UART DMA RX
RX RingBuffer
ProtocolFrame Parser
CRC16
ProtocolManager
CommandManager
CommandService
McuInfoApp
PC Python Tool
UI Protocol Monitor
CMD / RESP / NACK / EVENT
GET_RESET_INFO
GET_COMMAND_STATS
```

Stage 3 的目标不是简单增加日志打印，也不是把所有变量暴露给 PC，而是建立一套真正有工程意义的诊断框架，使系统在出现异常时可以回答：

```text
1. 系统现在是否还活着？
2. 系统是否健康？
3. 错误发生在哪一层？
4. 是通信错误、资源压力、状态异常、时序问题，还是 App handler 问题？
5. 问题是偶发、持续累积，还是瞬间崩溃？
6. 出问题前最后发生了什么？
7. 能否通过 PC 工具远程定位，而不依赖调试器？
```

本文档作为 Stage 3 开发总纲，后续代码、命令、UI、测试脚本都应围绕本文档定义的诊断视图展开。

---

## 2. Stage 3 总目标

Stage 3 的核心目标：

```text
1. 建立 DiagnosticApp 诊断中心
2. 统一收集各模块 stats / state / error / buffer / timing 信息
3. 建立 Health / Error / Buffer / Pipeline / Timing / Last Records 等诊断视图
4. 通过 CommandService + McuInfoApp 暴露诊断命令
5. 通过 PC CLI / UI 查询诊断信息
6. 通过人为故障注入验证诊断视图有效性
7. 为后续 IMU、FDCAN、Parameter、Bootloader、Security 提供统一诊断接入模式
```

Stage 3 的核心原则：

```text
诊断不是为了展示变量，而是为了定位问题。
```

---

## 3. 当前系统已有诊断数据源

当前工程中的很多模块已经有全局上下文结构体和 stats 字段，这些就是 Stage 3 的原始诊断数据源。

典型数据源：

```text
PlatformUart / RingBuffer
  - rx_bytes
  - rx_error_count
  - rx_ring_available
  - rx_ring_free
  - rb_high_watermark
  - rb_overflow_count

ProtocolFrame Parser
  - input_bytes
  - frame_ok_count
  - frame_ready_count
  - sof_error_count
  - len_error_count
  - crc_error_count

ProtocolManager
  - process_count
  - rx_bytes_consumed
  - frame_received_count
  - frame_sent_count
  - req_frame_count
  - resp_frame_count
  - nack_frame_count
  - event_frame_count
  - parser_error_count
  - tx_error_count
  - build_error_count
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
  - update_reset_snapshot_count
  - get_reset_snapshot_count
  - last_event_id
  - last_error
```

这些 stats 的意义不是“好看”，而是把系统从黑盒变成可观测链路。

---

## 4. 诊断框架总体架构

Stage 3 建议新增：

```text
firmware/app/Apps/diagnostic_app.h
firmware/app/Apps/diagnostic_app.c
```

整体结构：

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

注意：

```text
1. DiagnosticApp 不直接发送协议帧
2. DiagnosticApp handler 通过 McuInfoApp_RegisterCommand() 挂载到 CommandService
3. DiagnosticApp 只负责收集和组织诊断视图
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

如果后续 McuInfoApp 内部负责统一 App 注册，也可以采用：

```text
McuInfoApp_Init()
DiagnosticApp_Init()
DiagnosticApp_RegisterToMcuInfoApp()
```

---

## 5. 诊断视图设计

Stage 3 不应把所有变量直接扔给 PC，而应建立几个有工程意义的诊断视图。

推荐诊断视图：

```text
1. Health View          总体健康视图
2. Pipeline View        通信链路视图
3. Error View           错误计数器视图
4. Buffer View          队列 / 缓冲压力视图
5. Timing View          时间 / 延迟 / 抖动视图
6. Last Records View    最近命令 / 事件 / 错误视图
7. State View           状态机视图，后续使用
8. Build / Config View  版本与配置视图，后续使用
```

Stage 3 V1 不需要一次实现全部视图，优先实现：

```text
GET_HEALTH
GET_ERROR_COUNTERS
GET_BUFFER_STATS
GET_LAST_RECORDS
```

---

## 6. Health View

### 6.1 目标

Health View 是系统诊断的第一视图，用于快速判断系统是否健康。

它回答：

```text
1. 系统是否还活着？
2. 主循环是否还在运行？
3. 是否存在明显错误？
4. 是否存在队列丢弃？
5. 是否存在 UART overflow？
6. 最近错误是什么？
```

### 6.2 推荐字段

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

### 6.3 Health 状态判断

建议第一版定义：

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

### 6.4 返回示例

```text
health=OK,uptime=125430,err=0,drop=0,rx_ovf=0,last=0x00
```

---

## 7. Pipeline View

### 7.1 目标

Pipeline View 用于定位数据停在哪一层。

当前通信链路：

```text
PC
  ↓
UART DMA
  ↓
UART RX RingBuffer
  ↓
ProtocolFrame Parser
  ↓
ProtocolManager
  ↓
CommandManager
  ↓
CommandService
  ↓
McuInfoApp / App handler
  ↓
RESP / EVENT
```

Pipeline View 回答：

```text
1. UART 是否收到数据？
2. RingBuffer 是否积压？
3. Parser 是否出帧？
4. ProtocolManager 是否收到 REQ？
5. CommandManager 是否分发？
6. CommandService 是否找到 handler？
7. App handler 是否正常返回？
8. EVENT 是否进入发送队列？
```

### 7.2 推荐字段

```text
uart_rx
uart_err
parser_ok
parser_err
proto_rx
proto_tx
req
resp
nack
event
cmd_dispatch
cmd_unknown
handler_err
mcu_event_post
mcu_event_fwd
mcu_event_drop
```

### 7.3 返回示例

```text
uart_rx=10240,parser_ok=120,proto_rx=120,req=80,resp=78,nack=2,cmd=80,unk=1,herr=0
```

Stage 3 V1 可以暂时不独立实现 `GET_PIPELINE_STATS`，但 Health/Error/Buffer 三个视图应覆盖其中大部分信息。

---

## 8. Error View

### 8.1 目标

Error View 用于回答系统正在犯什么类型的错误。

不要只提供一个 `error_count`，应按错误类型拆分：

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

### 8.2 推荐命令

```text
DIAG_CMD_GET_ERROR_COUNTERS = 0x21
```

### 8.3 返回示例

```text
unknown=1,handler=0,parser=0,crc=0,busy=0,qfull=0,uart=0,ovf=0
```

### 8.4 工程意义

Error View 回答：

```text
1. 错误是否在累积？
2. 错误发生在协议层、命令层、队列层，还是底层 UART？
3. 是未知命令、handler 错误、资源忙，还是参数错误？
4. 人为注入错误后，对应计数器是否增长？
```

---

## 9. Buffer View

### 9.1 目标

Buffer View 用于观察资源压力。

嵌入式系统中很多问题不是立刻崩溃，而是：

```text
生产者速度 > 消费者速度
队列慢慢变满
高水位不断升高
最终出现 overflow / drop
```

Buffer View 回答：

```text
1. UART RX RingBuffer 是否接近满？
2. McuInfoApp event queue 是否积压？
3. CommandManager event queue 是否积压？
4. 是否出现 overflow / drop？
5. high watermark 是否接近容量上限？
```

### 9.2 推荐命令

```text
DIAG_CMD_GET_BUFFER_STATS = 0x22
```

### 9.3 推荐字段

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

### 9.4 返回示例

```text
uart_avail=0,uart_high=180,uart_ovf=0,mcu_drop=0,cmd_drop=0
```

---

## 10. Timing View

### 10.1 目标

Timing View 用于观察主循环和关键模块是否存在卡顿。

很多嵌入式问题表面是功能问题，本质是时序问题。

需要观察：

```text
main_loop_count
main_loop_max_gap_ms
protocol_process_count
protocol_max_process_us
command_max_dispatch_us
mcu_info_max_run_us
diagnostic_max_run_us
```

### 10.2 Stage 3 V1 策略

第一版只建议实现：

```text
main_loop_count
main_loop_max_gap_ms
```

后续再使用 DWT cycle counter 对 ProtocolManager、CommandService、App handler 做精细 profiling。

### 10.3 推荐命令

```text
DIAG_CMD_GET_TIMING_STATS = 0x23
```

V1 可先保留，不急于实现。

---

## 11. Last Records View

### 11.1 目标

Last Records View 用于保存“最近发生过什么”。

当前状态只能说明系统现在如何，但很多问题需要知道故障前的最近动作。

第一版不需要完整 trace ring，可以先记录：

```text
last_cmd
last_event
last_error
last_nack_cmd
last_nack_error
last_reset
last_health
```

### 11.2 推荐命令

```text
DIAG_CMD_GET_LAST_RECORDS = 0x24
```

### 11.3 返回示例

```text
last_cmd=0x0D,last_evt=0x85,last_err=0x00,last_nack=0x7E,last_reset=POR
```

### 11.4 后续增强

后续可以实现真正的 trace ring：

```text
[12340] CMD 0x01 RESP OK
[12400] EVENT 0x85 APP_MESSAGE
[12500] CMD 0x7E NACK UNKNOWN_CMD
[12600] CMD 0x0D RESP OK
```

---

## 12. State View

State View 主要服务于后续有状态机的复杂模块，例如：

```text
Parameter Save StateMachine
Bootloader Upgrade StateMachine
IMU Calibration StateMachine
FDCAN Recovery StateMachine
Security Auth StateMachine
```

每个状态机建议暴露：

```text
current_state
previous_state
state_duration_ms
transition_count
dispatch_count
error_count
last_event
```

Stage 3 V1 先不实现统一 State View，只在文档中确立规范。

---

## 13. Build / Config View

Build / Config View 用于确认固件版本、配置和测试宏状态。

推荐字段：

```text
firmware_version
protocol_version
build_date
build_time
git_hash
enabled_features
buffer_size
queue_size
baudrate
test_macros
```

它回答：

```text
1. 当前固件是不是预期版本？
2. PC 和 MCU 协议版本是否匹配？
3. buffer 配置是否正确？
4. 测试宏是否误开？
```

Stage 3 V1 可先不实现，后续作为 `GET_BUILD_INFO` 和 `GET_CONFIG_INFO` 扩展。

---

## 14. Diagnostic Commands

建议 Stage 3 命令从 `0x20` 开始。

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

## 15. DiagnosticApp API Design

### 15.1 初始化与运行

```c
void DiagnosticApp_Init(void);
void DiagnosticApp_Run(void);
```

### 15.2 命令注册

```c
int DiagnosticApp_RegisterCommands(void);
```

内部通过：

```c
McuInfoApp_RegisterCommand(DIAG_CMD_GET_HEALTH,
                           DiagnosticApp_HandleGetHealth,
                           NULL,
                           "GET_HEALTH");
```

### 15.3 信息生成接口

```c
int DiagnosticApp_GetHealth(char *buf, uint16_t buf_size, uint16_t *out_len);
int DiagnosticApp_GetErrorCounters(char *buf, uint16_t buf_size, uint16_t *out_len);
int DiagnosticApp_GetBufferStats(char *buf, uint16_t buf_size, uint16_t *out_len);
int DiagnosticApp_GetLastRecords(char *buf, uint16_t buf_size, uint16_t *out_len);
```

### 15.4 主循环记录接口

```c
void DiagnosticApp_RecordMainLoopTick(uint32_t now_ms);
void DiagnosticApp_RecordError(uint8_t source, uint8_t error_code);
void DiagnosticApp_RecordCommand(uint8_t cmd, uint8_t result);
void DiagnosticApp_RecordEvent(uint8_t event_id);
```

第一版可以先不实现全部记录接口，优先完成查询视图。

---

## 16. DiagnosticApp 内部上下文建议

```c
typedef enum
{
    DIAG_HEALTH_OK = 0,
    DIAG_HEALTH_WARN = 1,
    DIAG_HEALTH_ERROR = 2
} DiagnosticHealthState_t;

typedef struct
{
    uint8_t initialized;

    uint32_t init_count;
    uint32_t run_count;

    uint32_t main_loop_count;
    uint32_t last_loop_tick_ms;
    uint32_t max_loop_gap_ms;

    uint32_t get_health_count;
    uint32_t get_error_counters_count;
    uint32_t get_buffer_stats_count;
    uint32_t get_last_records_count;

    uint8_t health_state;
    uint8_t last_error;
    uint8_t last_cmd;
    uint8_t last_event;
    uint8_t last_nack_cmd;
    uint8_t last_nack_error;
} DiagnosticAppContext_t;
```

V1 不追求复杂，先用已有模块 stats 计算 health。

---

## 17. Fault Injection / Chaos Test Design

诊断框架必须能够被验证。

人为注入错误的意义：

```text
1. 验证错误是否能被检测到
2. 验证错误计数器是否增长
3. 验证错误是否定位到正确模块
4. 验证系统是否能从错误中恢复
5. 验证诊断视图是否能反映真实问题
```

Stage 3 V1 推荐先验证以下错误：

| 诊断对象 | 注入方法 | 预期响应 | 预期计数器 |
|---|---|---|---|
| 未知命令 | PC 发送 `0x7E` | NACK UNKNOWN_CMD | `unknown_cmd_count++` |
| CRC 错误 | PC 发送 CRC 被篡改的帧 | 无 RESP | `crc_error_count++` |
| 垃圾字节 | PC 发送随机字节后再发 PING | PING 正常 | `sof_error_count++` |
| 半包 | PC 发送前半帧，延迟后发送后半帧 | 后半到达后 RESP | 无错误或等待计数 |
| 粘包 | PC 连续发送多帧 | 多个 RESP | frame_ok_count 增长 |
| 事件队列满 | MCU 快速 PostEvent 超过队列容量 | 系统不崩 | event_drop_count++ |
| handler 错误 | 测试 handler 返回 ERROR | NACK INTERNAL_ERROR | handler_error_count++ |
| UART RX 溢出 | PC 高速发送 + MCU 慢消费 | 系统不崩 | rx_ring_overflow++ |

---

## 18. Stage 3 V1 Implementation Plan

推荐实现顺序：

| Step | Task | Output |
|---|---|---|
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

## 19. Stage 3 V1 Acceptance Criteria

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

## 20. Stage 3 Design Rules

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

## 21. 当前决策

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
