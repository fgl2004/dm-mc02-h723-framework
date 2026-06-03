# UART Reliable Protocol Design

## 1. 文档目的

本文档用于设计 `DM-MC02 H723 Embedded Framework` 项目的 Stage 2 UART 可靠通信协议。

Stage 1 已经完成基础平台能力：

```text
USART1 printf
Platform UART
Platform Time
Platform Reset
Platform Fault
Board Log
App_Init / App_Run
HardFault Decode
DWT Cycle Counter
```

Stage 2 的目标是在已有 USART1 生命线基础上，构建一个可扩展、可诊断、可测试、可逐步增强的 PC ↔ MCU 通信协议框架。

本协议不是简单的串口字符串命令，而是面向后续参数管理、诊断查询、固件升级、安全认证、主动事件上报、自动化测试等功能设计的二进制通信协议。

---

## 2. Stage 2 总目标

Stage 2 的核心目标：

```text
1. 建立 UART DMA 高效接收路径
2. 建立 RingBuffer 字节流缓存
3. 建立通用状态机框架
4. 建立协议帧格式
5. 实现 Frame Parser
6. 实现 CRC16 校验
7. 实现 ProtocolManager 帧收发管理
8. 实现 CommandManager 命令与事件语义管理
9. 实现 McuInfoApp 信息中枢 App
10. 支持基础命令：PING / GET_VERSION / GET_STATUS / GET_TIME_INFO
11. 支持 MCU 主动 EVENT 上报
12. 支持半包、粘包、CRC 错误、垃圾字节恢复
13. 为 ACK / NACK / 重传 / 滑动窗口预留升级空间
```

Stage 2 不追求一次实现完整 TCP 式可靠传输，而是采用分阶段演进方式：

```text
V1: Reliable Frame + CRC + REQ/RESP/NACK/EVENT
V2: SEQ + ACK/NACK + Timeout Retry
V3: Small Sliding Window
V4: Fragment / File Transfer / Upgrade Channel
V5: Security / HMAC / Encryption Extension
```

---

## 3. 当前架构决策

当前协议架构的核心决策已经从早期的 `CommandManager -> McuInfoApp_HandleCommand()`，重构为 **CommandService 命令注册表架构**：

```text
1. ProtocolFrame 只负责帧格式、编码、解码、CRC、Parser 状态机
2. ProtocolFrame 不定义具体业务命令
3. ProtocolManager 只负责 UART 字节流接入、帧解析、帧发送
4. CommandManager 负责 REQ / RESP / NACK / EVENT 的语义流程
5. CommandService 负责统一命令表、命令注册、命令分发和响应辅助封装
6. McuInfoApp 是 MCU 与 PC 信息交互中心 App，同时作为 App handler 与 CommandService 的“牵手入口”
7. 具体命令 handler 集成在各自 App 内部，不堆到 CommandManager / ProtocolManager 中
8. 其他 App 不直接操作 CommandManager / ProtocolManager
9. 其他 App 若需要与 PC 交互，通过 McuInfoApp 注册命令、更新快照或上报事件
10. McuInfoApp 使用 RingBuffer 缓存内部事件
11. CommandManager 使用 RingBuffer 缓存待发送 EVENT
12. ProtocolManager 从 CommandManager 取 EVENT，再封装为 EVENT 帧发送给 PC
```

当前三类已落地的信息通路：

```text
CMD       PC -> MCU       请求、配置、控制、查询
EVENT     MCU -> PC       离散事件主动上报
SNAPSHOT  MCU 内部缓存     保存最近状态，PC 通过 CMD 查询
```

后续预留两类增强通路：

```text
STREAM    MCU -> PC       高频实时数据流，例如 IMU / 控制环 / ADC 波形
BULK      双向/单向        大块可靠传输，例如固件升级、日志 dump、参数导入导出
```

因此，主动上报 EVENT 不允许 App 直接调用 ProtocolManager 发送。

正确路径是：

```text
Other App
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
ProtocolManager sends EVENT frame
  ↓
PC Tool
```

命令注册与响应路径是：

```text
App internal handler
  ↓ register through
McuInfoApp_RegisterCommand()
  ↓
CommandService_Register()
  ↓
CommandService command table
```

PC 查询时：

```text
PC REQ
  ↓
ProtocolManager
  ↓
CommandManager_Dispatch()
  ↓
CommandService_Dispatch()
  ↓
cmd -> handler
  ↓
App internal handler fills CommandManagerResponse_t
  ↓
ProtocolManager sends RESP / NACK
```

## 4. Protocol Layering

### 4.1 总体层次

```text
PC Tool
  ↓
Protocol Frame
  ↓
ProtocolManager
  ↓
CommandManager
  ↓
CommandService
  ↓
App internal command handler
  ↓
McuInfoApp snapshot / event / provider / Platform / Board
```

### 4.2 数据接收路径

```text
PC Tool
  ↓ UART bytes
USART1 RX DMA
  ↓
RX RingBuffer
  ↓
ProtocolManager_Process()
  ↓
ProtocolFrameParser_InputByte()
  ↓
ProtocolFrame_t
  ↓
CommandManager_Dispatch()
  ↓
CommandService_Dispatch()
  ↓
cmd table lookup
  ↓
App internal handler
  ↓
CommandManagerResponse_t
  ↓
ProtocolManager sends RESP / NACK
```

### 4.3 主动事件上报路径

```text
Other App / Platform / Fault / UART
  ↓
McuInfoApp_PostEvent()
  ↓
McuInfoApp event RingBuffer
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
PC Tool
```

### 4.4 快照查询路径

```text
Platform / App captures state
  ↓
McuInfoApp_UpdateSnapshot() / specialized snapshot API
  ↓
McuInfoApp snapshot store
  ↓
PC sends GET_xxx_INFO / GET_xxx_STATS
  ↓
App handler reads snapshot
  ↓
RESP returns latest cached state
```

示例：`GET_RESET_INFO` 使用启动阶段捕获的 reset snapshot，而不是 PC 查询时重新读取 RCC reset flags。

## 5. 各模块职责

### 5.1 ProtocolFrame

位置：

```text
firmware/app/Middleware/protocol_frame.h
firmware/app/Middleware/protocol_frame.c
```

职责：

```text
1. 定义协议帧格式
2. 定义 Frame Type
3. 定义 Frame Flags
4. 定义通用 Error Code
5. 定义 ProtocolFrame_t
6. 实现 ProtocolFrame_Build()
7. 实现 ProtocolFrameParser
8. 使用通用 StateMachine 框架解析字节流
9. 使用 CRC16 校验帧完整性
```

不允许做：

```text
1. 不定义 PING / GET_VERSION / GET_STATUS 等业务 CMD
2. 不处理命令
3. 不访问 Platform
4. 不发送 UART
```

---

### 5.2 ProtocolManager

位置：

```text
firmware/app/Services/protocol_manager.h
firmware/app/Services/protocol_manager.c
```

职责：

```text
1. 从 PlatformUart_ReadRx() 读取 RX RingBuffer 字节
2. 将字节输入 ProtocolFrameParser
3. 获取完整协议帧
4. 将 REQ 帧交给 CommandManager_Dispatch()
5. 根据 CommandManager 返回结果发送 RESP / NACK
6. 从 CommandManager_TryGetPendingEvent() 获取待上报事件
7. 将待上报事件封装成 EVENT 帧发送给 PC
8. 维护协议收发统计
```

不允许做：

```text
1. 不直接处理 PING / GET_VERSION / GET_STATUS
2. 不直接访问 McuInfoApp 内部数据
3. 不对外暴露 App 可直接调用的 SendEvent 接口
4. 不理解业务含义，只负责帧传输
```

---

### 5.3 CommandManager

位置：

```text
firmware/app/Services/command_manager.h
firmware/app/Services/command_manager.c
```

职责：

```text
1. 作为 PC 命令与 MCU 事件的语义出口
2. 接收 ProtocolManager 下发的 REQ
3. 将 REQ 统一转发给 CommandService_Dispatch()
4. 接收 McuInfoApp_Run() 提交的 EVENT
5. 使用 RingBuffer 缓存待发送 EVENT
6. 提供 CommandManager_TryGetPendingEvent() 给 ProtocolManager
7. 维护命令分发统计、事件队列统计、错误统计
```

不允许做：

```text
1. 不直接访问 Platform
2. 不直接读取 UART / Reset / Fault / Time
3. 不主动发送协议帧
4. 不实现具体命令 handler
5. 不直接调用 McuInfoApp_HandleCommand() 这类业务 switch
```

---

### 5.4 CommandService

位置：

```text
firmware/app/Services/command_service.h
firmware/app/Services/command_service.c
```

职责：

```text
1. 维护统一命令注册表
2. 提供 CommandService_Register()
3. 提供 CommandService_Dispatch()
4. 根据 cmd 查找 handler / ctx / name / category / flags
5. 调用各 App 内部注册的 handler
6. 提供 CommandService_SetResp() / CommandService_SetNack() 辅助封装
7. 维护 init / register / dispatch / unknown / handler_error 等统计
8. 支持 GET_COMMAND_STATS 观测命令
```

不允许做：

```text
1. 不实现具体业务命令逻辑
2. 不直接访问 Platform
3. 不直接发送协议帧
4. 不管理 EVENT 队列
```

设计规则：

```text
CommandService 可以作为较大的命令注册与分发表。
具体 handler 应集成在各自 App 内部。
普通 App 推荐通过 McuInfoApp_RegisterCommand() 进入 CommandService，而不是直接接触 CommandManager / ProtocolManager。
```

---

### 5.5 McuInfoApp

位置：

```text
firmware/app/Apps/mcu_info_app.h
firmware/app/Apps/mcu_info_app.c
```

职责：

```text
1. 作为 MCU 与 PC 信息交互的中心 App
2. 作为其他 App command handler 与 CommandService 的牵手入口
3. 对外提供 McuInfoApp_RegisterCommand()
4. 接收其他 App / 模块上报的事件
5. 接收其他 App / 模块更新的快照
6. 使用 RingBuffer 缓存内部事件
7. 在 McuInfoApp_Run() 中节流、聚合、转发事件到 CommandManager
8. 对外提供 McuInfoApp_PostEvent()
9. 对外提供 McuInfoApp_UpdateSnapshot()
10. 对外提供 McuInfoApp_UpdateRuntimeStatus()
11. 当前基础命令 handler 暂时集成在 mcu_info_app.c 内部，并通过 McuInfoApp_RegisterCommand() 挂入 CommandService
```

当前支持命令：

```text
PING
GET_VERSION
GET_STATUS
GET_RESET_INFO
GET_TIME_INFO
GET_UART_STATS
GET_APP_STATS
GET_COMMAND_STATS
```

当前预留命令：

```text
GET_FAULT_INFO
```

设计原则：

```text
其他 App 若需要与 PC 交互，不能直接调用 CommandManager 或 ProtocolManager。
普通 App 的命令 handler 可以在自己 App 内部实现，但应通过 McuInfoApp_RegisterCommand() 进行挂载。
App 主动上报使用 McuInfoApp_PostEvent()。
当前状态缓存使用 McuInfoApp_UpdateSnapshot()。
```

---

### 5.6 RingBuffer Middleware

位置：

```text
firmware/app/Middleware/ring_buffer.h
firmware/app/Middleware/ring_buffer.c
```

职责：

```text
1. 作为 UART RX 字节流缓存
2. 作为 McuInfoApp 内部事件队列缓存
3. 作为 CommandManager 待发送 EVENT 队列缓存
4. 提供 overflow / high watermark / write_bytes / read_bytes 统计
```

RingBuffer 是字节环形缓冲区，因此事件队列采用固定长度记录方式写入。

示例：

```c
typedef struct
{
    uint8_t app_id;
    uint8_t event_id;
    uint16_t payload_len;
    uint32_t tick_ms;
    uint8_t payload[64];
} McuInfoEventRecord_t;
```

入队时将整个结构体作为字节块写入 RingBuffer。

出队时必须确保 RingBuffer 中至少存在一个完整记录长度。

## 6. UART DMA Receive Design

### 6.1 Why DMA Receive

UART 是字节流接口，如果使用单字节中断接收，高波特率或大量数据时 CPU 中断压力较大。

因此 Stage 2 使用：

```text
UART RX DMA Circular Mode
Half Transfer Interrupt
Transfer Complete Interrupt
Idle Line Interrupt
```

目标：

```text
1. 降低 UART 接收中断频率
2. 支持连续字节流接收
3. 支持半包、粘包场景
4. 支持后续大数据传输
5. 为固件升级和滑动窗口打基础
```

---

### 6.2 DMA RX Buffer

建议第一版 DMA RX buffer：

```c
#define UART_DMA_RX_BUFFER_SIZE 256
```

DMA 工作模式：

```text
Circular Mode
```

DMA buffer 分成两半：

```text
[0 ... 127]     first half
[128 ... 255]   second half
```

触发事件：

| Event             | Meaning           |
| ----------------- | ----------------- |
| Half Transfer     | DMA 写满前半区         |
| Transfer Complete | DMA 写满后半区         |
| UART IDLE         | 串口空闲，表示当前一批数据可能结束 |

---

### 6.3 DMA Event Handling

接收事件处理思路：

```text
DMA Half Transfer:
  把 DMA buffer 前半区新数据搬入 RX RingBuffer

DMA Transfer Complete:
  把 DMA buffer 后半区新数据搬入 RX RingBuffer

UART IDLE:
  计算 DMA 当前写入位置
  把上次处理位置到当前写入位置之间的新数据搬入 RX RingBuffer
```

关键变量：

```c
static uint8_t uart_dma_rx_buf[UART_DMA_RX_BUFFER_SIZE];
static uint16_t uart_dma_last_pos;
```

处理逻辑：

```text
current_pos = UART_DMA_RX_BUFFER_SIZE - DMA_NDTR

if current_pos >= last_pos:
    new data = [last_pos, current_pos)
else:
    new data = [last_pos, end) + [0, current_pos)

last_pos = current_pos
```

---

### 6.4 DMA + D-Cache Issue

STM32H7 需要特别关注 D-Cache 和 DMA 一致性。

如果 D-Cache 开启，DMA 写入内存后 CPU 可能读到旧 cache 数据。

Stage 2 初期可采用两种策略之一：

```text
Strategy A:
  暂时关闭 D-Cache，优先跑通 UART DMA 协议链路。

Strategy B:
  DMA buffer 放到 non-cacheable 区域，或者每次读取前 invalidate cache。
```

后续要求：

```text
1. DMA buffer 必须明确 cache 策略
2. DMA buffer 地址尽量 cache line 对齐
3. Platform UART 层封装 cache 维护细节
```

---

## 7. Protocol Frame Format

协议采用二进制帧格式。

当前 V1 帧格式：

```text
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
| SOF1 | SOF2  | VER | TYPE | FLAGS | SEQ | CMD | LEN    | PAYLOAD | CRC16 |
| 0xA5 | 0x5A  | 1B  | 1B   | 1B    | 1B  | 1B  | 2B LE  | N bytes | 2B LE |
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
```

字段说明：

| Field   | Size | Description                         |
| ------- | ---: | ----------------------------------- |
| SOF1    |    1 | Start of frame byte 1, fixed `0xA5` |
| SOF2    |    1 | Start of frame byte 2, fixed `0x5A` |
| VER     |    1 | Protocol version, current `0x01`    |
| TYPE    |    1 | Frame type                          |
| FLAGS   |    1 | Frame flags                         |
| SEQ     |    1 | Sequence number                     |
| CMD     |    1 | Command ID or Event ID              |
| LEN     |    2 | Payload length, little-endian       |
| PAYLOAD |    N | Payload bytes                       |
| CRC16   |    2 | CRC16 little-endian                 |

Minimum frame length:

```text
SOF1 + SOF2 + VER + TYPE + FLAGS + SEQ + CMD + LEN + CRC16 = 11 bytes
```

CRC 覆盖范围：

```text
VER + TYPE + FLAGS + SEQ + CMD + LEN + PAYLOAD
```

不包含：

```text
SOF1
SOF2
CRC16
```

注意：

```text
CMD 字段在 REQ/RESP/NACK 中表示 Command ID。
CMD 字段在 EVENT 中表示 Event ID。
ProtocolFrame 不解释 CMD 的业务含义。
```

---

## 8. Frame Type

| Type       |  Value | Description                           |
| ---------- | -----: | ------------------------------------- |
| REQ        | `0x01` | PC request to MCU                     |
| RESP       | `0x02` | MCU response to PC                    |
| ACK        | `0x03` | ACK frame, later use                  |
| NACK       | `0x04` | Error response                        |
| EVENT      | `0x05` | MCU asynchronous event                |
| DATA       | `0x06` | Sliding-window data frame, future     |
| WINDOW_ACK | `0x07` | Sliding-window cumulative ACK, future |

V1 主要使用：

```text
REQ
RESP
NACK
EVENT
```

V2/V3 再逐步启用：

```text
ACK
DATA
WINDOW_ACK
```

---

## 9. Frame Flags

| Flag          |    Bit | Description                     |
| ------------- | -----: | ------------------------------- |
| ACK_REQ       |   bit0 | Sender requests ACK             |
| IS_RETRY      |   bit1 | Retransmitted frame             |
| MORE_FRAG     |   bit2 | More fragments follow           |
| ENCRYPTED     |   bit3 | Payload encrypted, future       |
| AUTH_REQUIRED |   bit4 | Authentication required, future |
| RESERVED      | bit5~7 | Reserved                        |

V1 可以先保留字段但不全部使用。

---

## 10. Command ID Design

具体命令 ID 不在 ProtocolFrame 层定义。

当前命令 ID 由 CommandService 统一注册和分发。当前基础命令 handler 由 McuInfoApp 内部实现，并通过 `McuInfoApp_RegisterCommand()` 挂载到 CommandService。

当前已实现命令：

| Command                          |  Value | Direction | Description                    |
| -------------------------------- | -----: | --------- | ------------------------------ |
| MCU_INFO_CMD_PING                | `0x01` | PC → MCU  | Communication test             |
| MCU_INFO_CMD_GET_VERSION         | `0x02` | PC → MCU  | Get firmware version           |
| MCU_INFO_CMD_GET_STATUS          | `0x03` | PC → MCU  | Get runtime status             |
| MCU_INFO_CMD_GET_RESET_INFO      | `0x04` | PC → MCU  | Get reset information snapshot |
| MCU_INFO_CMD_GET_TIME_INFO       | `0x05` | PC → MCU  | Get tick information           |
| MCU_INFO_CMD_GET_FAULT_INFO      | `0x06` | PC → MCU  | Reserved, fault integration next |
| MCU_INFO_CMD_GET_UART_STATS      | `0x07` | PC → MCU  | Get UART RX statistics         |
| MCU_INFO_CMD_GET_APP_STATS       | `0x08` | PC → MCU  | Get McuInfoApp statistics      |
| MCU_INFO_CMD_GET_COMMAND_STATS   | `0x0D` | PC → MCU  | Get CommandService statistics  |

`GET_COMMAND_STATS` 当前 payload 为 ASCII，示例：

```text
init=1,reg=10,disp=7,unk=0,err=0,last=0x0D
```

字段含义：

| Field | Meaning                                |
| ----- | -------------------------------------- |
| init  | CommandService 初始化次数              |
| reg   | 当前注册命令数量                       |
| disp  | 已分发命令次数                         |
| unk   | 未知命令计数                           |
| err   | handler 错误计数                       |
| last  | 最近一次处理的命令 ID                  |

后续命令分类规划：

| Range       | Category  | Future Usage                         |
| ----------- | --------- | ------------------------------------ |
| `0x01~0x1F` | System / Info | 基础信息、系统状态、统计查询       |
| `0x20~0x2F` | Diagnostic | 诊断、Trace、Buffer、健康监控       |
| `0x30~0x3F` | IMU / Algo | IMU、滤波、算法闭环                 |
| `0x40~0x4F` | FDCAN      | CAN 状态、报文、诊断                 |
| `0x50~0x5F` | Parameter  | 参数管理、Flash 保存、导入导出       |
| `0x60~0x6F` | Boot       | Bootloader、固件升级                 |
| `0x70~0x7F` | Security   | Challenge、HMAC、认证、防重放        |
| `0x80~0x8F` | Power / HW | 低功耗、硬件诊断                     |
| `0x90~0x9F` | Chaos Test | 异常注入、自动化测试                 |
| `0xF0~0xFF` | Debug      | 调试保留                             |

后续如果引入其他 App，具体 handler 可以集成在对应 App 内部，但应通过 McuInfoApp 牵手注册到 CommandService。

## 11. Event ID Design

EVENT 帧的 CMD 字段表示 Event ID。

当前 McuInfoApp 事件 ID：

| Event                         |  Value | Source            | Description            |
| ----------------------------- | -----: | ----------------- | ---------------------- |
| MCU_INFO_EVENT_BOOT           | `0x81` | McuInfoApp        | Boot event             |
| MCU_INFO_EVENT_HEARTBEAT      | `0x82` | McuInfoApp        | Heartbeat event        |
| MCU_INFO_EVENT_RUNTIME_STATUS | `0x83` | Other App         | Runtime status changed |
| MCU_INFO_EVENT_UART_WARNING   | `0x84` | UART / McuInfoApp | UART warning           |
| MCU_INFO_EVENT_APP_MESSAGE    | `0x85` | Other App         | Generic app message    |
| MCU_INFO_EVENT_FAULT          | `0x86` | Fault module      | Fault event            |

EVENT payload 第一版格式：

```text
byte0      app_id
byte1      original_event_id
byte2~5    tick_ms little-endian
byte6..N   event payload
```

说明：

```text
EVENT frame 的 CMD 已经是 event_id。
payload 中仍保留 original_event_id，方便 PC 工具统一解析和交叉检查。
```

---

## 12. Error Code

|   Code | Name                | Description                  |
| -----: | ------------------- | ---------------------------- |
| `0x00` | OK                  | Success                      |
| `0x01` | UNKNOWN_CMD         | Unknown command              |
| `0x02` | INVALID_LEN         | Invalid payload length       |
| `0x03` | CRC_ERROR           | CRC check failed             |
| `0x04` | INVALID_STATE       | Invalid current state        |
| `0x05` | BUSY                | Device busy                  |
| `0x06` | INTERNAL_ERROR      | Internal error               |
| `0x07` | AUTH_REQUIRED       | Authentication required      |
| `0x08` | TIMEOUT             | Timeout                      |
| `0x09` | INVALID_PARAM       | Invalid parameter            |
| `0x0A` | WINDOW_FULL         | Receive window full          |
| `0x0B` | DUPLICATE_FRAME     | Duplicate frame              |
| `0x0C` | UNSUPPORTED_VERSION | Unsupported protocol version |

NACK payload 格式：

```text
payload[0] = error_code
payload[1] = original_cmd
```

---

## 13. Frame Parser State Machine

Frame Parser 状态机负责从字节流中恢复完整协议帧。

状态：

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

状态含义：

| State          | Description                             |
| -------------- | --------------------------------------- |
| WAIT_SOF1      | 等待 `0xA5`                               |
| WAIT_SOF2      | 等待 `0x5A`                               |
| READ_HEADER    | 读取 VER / TYPE / FLAGS / SEQ / CMD / LEN |
| READ_PAYLOAD   | 根据 LEN 读取 Payload                       |
| READ_CRC       | 读取 CRC16                                |
| VERIFY_CRC     | 校验 CRC                                  |
| FRAME_READY    | 输出完整帧                                   |
| ERROR_RECOVERY | 错误恢复                                    |

异常恢复策略：

```text
SOF1 错误：
  继续等待 0xA5

SOF2 错误：
  如果当前字节是 0xA5，则保持 WAIT_SOF2
  否则回到 WAIT_SOF1

LEN 超限：
  丢弃当前帧
  len_error_count++
  回到 WAIT_SOF1

CRC 错误：
  crc_error_count++
  丢弃当前帧
  回到 WAIT_SOF1

Payload 不完整：
  保持 READ_PAYLOAD，等待更多字节

垃圾字节：
  不影响状态机长期恢复
```

---

## 14. Command / Event / Snapshot Processing Model

### 14.1 PC 查询模型

```text
PC sends REQ
  ↓
ProtocolManager parses frame
  ↓
CommandManager_Dispatch()
  ↓
CommandService_Dispatch()
  ↓
cmd table lookup
  ↓
App internal handler
  ↓
CommandManagerResponse_t
  ↓
ProtocolManager sends RESP / NACK
```

该模型的核心规则：

```text
CommandManager 不实现具体业务命令。
CommandService 只维护表和分发，不实现具体业务逻辑。
具体 handler 集成在各自 App 内部。
McuInfoApp 提供 McuInfoApp_RegisterCommand() 作为牵手入口。
```

### 14.2 MCU 主动上报模型

```text
Other App posts event
  ↓
McuInfoApp_PostEvent()
  ↓
McuInfoApp event RingBuffer
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
PC Tool
```

该模型的核心规则：

```text
App 不直接调用 ProtocolManager。
其他 App 不直接调用 CommandManager。
所有主动上报必须进入 McuInfoApp。
CommandManager 管理语义队列。
ProtocolManager 只做帧发送。
```

### 14.3 Snapshot 查询模型

Snapshot 表示“最近状态缓存”，不是实时数据流。

```text
App / Platform captures current state
  ↓
McuInfoApp_UpdateSnapshot() or specialized update API
  ↓
McuInfoApp snapshot store
  ↓
PC sends GET_xxx_INFO / GET_xxx_STATS
  ↓
App handler reads latest snapshot
  ↓
RESP returns latest state
```

适合使用 Snapshot 的信息：

```text
1. Reset info：启动早期捕获，后续 RCC flags 会被清除
2. Fault info：故障现场需要保留，后续 PC 查询最近一次 fault
3. IMU / CAN / Power 最近状态：后续阶段可按周期更新
4. 复杂统计信息：避免 PC 查询时进行重计算
```

不适合使用 Snapshot 的命令：

```text
PING / GET_TIME_INFO / PARAM_SET / ENTER_BOOTLOADER / TRIGGER_RESET
```

这些命令可由 handler 直接实时执行。

### 14.4 Stream / Bulk 预留模型

后续通信模型将扩展为五类：

```text
CMD       PC -> MCU       请求、配置、控制、查询
EVENT     MCU -> PC       离散重要事件
SNAPSHOT  MCU 内部缓存     最近状态，PC 通过 CMD 查询
STREAM    MCU -> PC       高频实时数据流，例如 IMU / 控制环 / ADC 波形
BULK      双向/单向        大块可靠传输，例如固件升级、日志、参数表
```

当前 Stage 2 V1 已实现 CMD / EVENT / SNAPSHOT 基础能力，STREAM 与 BULK 后续实现。

## 15. Reliability Mechanism Roadmap

### 15.1 V1: Request / Response / Event

V1 采用基础请求响应模型和异步事件模型：

```text
PC sends REQ
MCU sends RESP or NACK

MCU internal app posts event
MCU sends EVENT
```

特点：

```text
简单
稳定
容易调试
适合 PING / GET_VERSION / GET_STATUS / EVENT 上报
```

---

### 15.2 V2: Stop-and-Wait ARQ

V2 增加：

```text
SEQ
ACK_REQ
timeout
retry
duplicate detection
```

流程：

```text
PC sends frame with SEQ=N and ACK_REQ
PC waits response
If timeout, PC retries with IS_RETRY flag
MCU detects duplicate SEQ and avoids repeated side effects
```

默认参数建议：

```c
#define PROTO_RETRY_MAX        3
#define PROTO_RESP_TIMEOUT_MS  100
```

---

### 15.3 V3: Small Sliding Window

V3 增加小窗口机制，用于大数据传输。

建议窗口大小：

```c
#define PROTO_TX_WINDOW_SIZE   4
#define PROTO_RX_WINDOW_SIZE   4
```

窗口机制：

```text
Sender can send multiple DATA frames before receiving ACK.
Receiver returns WINDOW_ACK with next_expected_seq.
Sender removes acknowledged frames from window.
Sender retransmits from next_expected_seq on timeout.
```

第一版滑动窗口不支持乱序提交。

如果收到乱序帧：

```text
discard frame
return WINDOW_ACK with current next_expected_seq
```

后续可扩展 bitmap selective ACK。

---

### 15.4 V4: Fragment and File Transfer

V4 用于固件升级或大数据传输。

扩展：

```text
MORE_FRAG
fragment index
total length
file offset
chunk CRC
session ID
```

用途：

```text
Firmware upgrade
Parameter import/export
Log dump
Blackbox upload
```

---

### 15.5 V5: Security Extension

V5 用于危险命令、安全认证和固件升级。

扩展：

```text
Challenge-Response
HMAC
Nonce
Replay Protection
Encrypted Payload
Firmware Signature
```

安全相关字段可通过 FLAGS 和 Payload TLV 扩展实现。

---

## 16. Module Plan

Stage 2 代码模块规划：

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

PC 工具规划：

```text
pc_tool/
└── h7_uart_ui/
    ├── proto_ping_test.py
    ├── proto_command_test.py
    ├── proto_robust_test.py
    ├── proto_event_listen.py
    └── h7_ui/
```

---

## 17. Stage 2 Implementation Plan

推荐实现顺序与当前进度：

| Step | Task                     | Status | Output                                        |
| ---- | ------------------------ | ------ | --------------------------------------------- |
| 2.1  | Protocol design document | Done   | `docs/04_uart_reliable_protocol_design.md`    |
| 2.2  | RingBuffer middleware    | Done   | `ring_buffer.h/.c`                            |
| 2.3  | CRC16 middleware         | Done   | `crc16.h/.c`                                  |
| 2.4  | Generic state machine    | Done   | `state_machine.h/.c`                          |
| 2.5  | UART DMA RX path         | Done   | `platform_uart` RX extension                  |
| 2.6  | Protocol frame parser    | Done   | `protocol_frame.h/.c`                         |
| 2.7  | Protocol Manager         | Done   | `protocol_manager.h/.c`                       |
| 2.8  | Command Manager          | Done   | `command_manager.h/.c`                        |
| 2.9  | McuInfoApp               | Done   | `mcu_info_app.h/.c`                           |
| 2.10 | Basic commands           | Done   | PING / VERSION / STATUS / TIME / UART / APP   |
| 2.11 | EVENT path               | Done   | McuInfoApp → CommandManager → ProtocolManager |
| 2.12 | Python PC Tool           | Done   | ping / command / robust / event listen        |
| 2.13 | Async EVENT compatibility| Done   | Command/robust tests ignore asynchronous EVENT |
| 2.14 | RESET snapshot integration | Done | GET_RESET_INFO returns real reset snapshot    |
| 2.15 | CommandService registry refactor | Done | CommandService + App handler registration     |
| 2.16 | CommandService observability | Done | GET_COMMAND_STATS + Python command test       |
| 2.17 | Fault snapshot/event integration | Next | GET_FAULT_INFO + fault event                  |
| 2.18 | Reliability V2           | Planned | ACK / NACK / timeout / retry                  |
| 2.19 | Sliding window V3        | Planned | small window for data transfer                |
```

当前 Stage 2 V1 的主要链路已经闭环，后续进入 Fault 集成与 V2 可靠性增强。

## 18. Stage 2 Test Plan

### 18.1 基础命令测试

```text
1. PC sends PING, MCU returns PONG
2. PC sends GET_VERSION, MCU returns version
3. PC sends GET_STATUS, MCU returns status
4. PC sends GET_TIME_INFO, MCU returns tick
5. PC sends GET_UART_STATS, MCU returns UART stats
6. PC sends GET_APP_STATS, MCU returns McuInfoApp stats
7. PC sends GET_RESET_INFO, MCU returns real reset snapshot
8. PC sends GET_COMMAND_STATS, MCU returns CommandService stats
9. PC sends invalid CMD, MCU returns UNKNOWN_CMD
```

### 18.2 Frame Parser 健壮性测试

```text
1. PC sends invalid CRC, MCU rejects frame
2. PC sends half packet, parser waits
3. PC sends sticky packets, parser extracts multiple frames
4. PC sends garbage bytes before valid frame, parser recovers
5. PC sends payload length overflow, parser rejects
6. RX RingBuffer overflow counter increments correctly
```

### 18.3 DMA 测试

```text
1. DMA half transfer event works
2. DMA transfer complete event works
3. UART IDLE event works
4. DMA circular wrap works
5. DMA data can be moved to RingBuffer correctly
6. Protocol parser works with DMA source
```

### 18.4 EVENT 上报测试

```text
1. McuInfoApp_PostEvent() can enqueue event
2. McuInfoApp_Run() can forward event to CommandManager
3. CommandManager_PostEvent() can enqueue pending event
4. ProtocolManager_Process() can fetch pending event
5. ProtocolManager sends EVENT frame
6. PC event listener can parse EVENT frame
7. Event RingBuffer high watermark can be observed
8. Event queue full condition increments drop counter
9. PC command tests can still pass while EVENT frames are mixed in
```

### 18.5 CommandService 测试

```text
1. CommandService_Init() is idempotent
2. McuInfoApp_RegisterCommand() can mount built-in handlers
3. CommandService_Dispatch() can find registered handlers
4. Unknown commands return NACK UNKNOWN_CMD
5. GET_COMMAND_STATS returns init / reg / disp / unk / err / last fields
6. Payload length matches valid ASCII string length and does not include stack garbage
```

### 18.6 可靠性测试

```text
1. SEQ matches request and response
2. Timeout retry works
3. Duplicate request is detected
4. NACK is generated for invalid frame or state
5. Sliding window cumulative ACK works in V3
```

## 19. Stage 2 Acceptance Criteria

Stage 2 V1 当前完成标准：

```text
1. UART RX DMA can receive continuous bytes —— Done
2. RX RingBuffer works —— Done
3. Frame parser can recover valid frames from byte stream —— Done
4. CRC16 can detect corrupted frames —— Done
5. PING / GET_VERSION / GET_STATUS work —— Done
6. GET_TIME_INFO / GET_UART_STATS / GET_APP_STATS work —— Done
7. GET_RESET_INFO returns real reset snapshot —— Done
8. GET_COMMAND_STATS returns CommandService stats —— Done
9. McuInfoApp can register command handlers into CommandService —— Done
10. McuInfoApp can post internal events —— Done
11. CommandManager can cache pending events —— Done
12. ProtocolManager can send EVENT frames —— Done
13. PC Python tool can send commands and parse responses —— Done
14. PC Python event listener can receive EVENT frames —— Done
15. Half packet / sticky packet / garbage bytes can be handled —— Done
16. Async EVENT mixed with command response does not break PC tests —— Done
17. Protocol statistics can be printed or queried —— Done
```

Stage 2 V1 剩余增强项：

```text
1. GET_FAULT_INFO 接入真实 fault snapshot/event —— Next
2. 协议栈空间占用优化 —— Planned
3. CommandService 命令列表查询 —— Planned
4. UI 增加 GET_COMMAND_STATS 按钮 —— Optional
```

Stage 2 V2 增强阶段完成标准：

```text
1. SEQ request-response matching works
2. ACK/NACK frame works
3. Timeout retry works
4. Duplicate detection works
5. Small sliding window design is implemented or partially implemented
```

## 20. Design Rules

后续实现中遵循以下规则：

```text
1. 中断中只做最少工作
2. DMA callback 只搬运数据或记录事件
3. 协议解析在主循环或 ProtocolManager_Process 中执行
4. Parser 不直接执行命令
5. ProtocolFrame 不定义业务命令
6. ProtocolManager 不处理业务命令
7. CommandManager 不直接操作 UART
8. CommandManager 不直接读取 Platform 数据
9. CommandService 只管理命令表和分发，不实现具体业务逻辑
10. 具体命令 handler 集成在各自 App 内部
11. McuInfoApp 是 MCU 与 PC 信息交互中心，也是 App handler 与 CommandService 的牵手入口
12. 其他 App 不直接调用 CommandManager / ProtocolManager
13. 其他 App 通过 McuInfoApp_RegisterCommand / PostEvent / UpdateSnapshot 与 PC 间接交互
12. 所有错误必须有计数器
13. 所有 buffer 必须有 high watermark 和 overflow 统计
14. 所有可靠性机制必须可关闭或分阶段启用
15. 所有测试宏默认安全关闭
16. 测试代码集成在 app_main.c 中，通过 ENABLE_xxx_TEST 宏控制
17. 协议热路径尽量减少 printf / BoardLog
18. 大局部结构体优先放入模块上下文，避免栈压力过大
```

---

## 21. Current Decision

当前决策：

```text
1. 使用二进制帧协议
2. 使用 SOF + LEN + CRC16
3. 使用 FLAGS + SEQ 为可靠机制预留空间
4. 使用 UART DMA circular receive
5. 使用 half transfer / transfer complete / idle event 处理 DMA 接收
6. 使用 RingBuffer 解耦 DMA 接收和协议解析
7. 使用 RingBuffer 实现 McuInfoApp 内部事件队列
8. 使用 RingBuffer 实现 CommandManager 待发送 EVENT 队列
9. 使用通用状态机框架承载 Frame Parser 和后续升级状态机
10. ProtocolFrame 不定义具体业务命令
11. ProtocolManager 只负责协议帧收发
12. CommandManager 只负责 REQ/RESP/NACK/EVENT 语义流程
13. CommandService 作为统一命令注册表和分发器
14. 具体命令 handler 集成在各自 App 内部
15. McuInfoApp 作为 MCU 信息中枢 App，并提供命令挂载、事件、快照接口
16. 第一版实现 REQ / RESP / NACK / EVENT
17. Snapshot 定位为最近状态缓存，PC 通过 CMD 查询
18. Stream 定位为后续高频实时数据流通道
19. Bulk 定位为后续大块可靠传输通道
20. 第二版加入 ACK / retry
21. 第三版加入 small sliding window
```

当前已完成的重要重构：

```text
CommandManager -> CommandService_Dispatch() -> App internal handler
```

不再采用：

```text
CommandManager -> McuInfoApp_HandleCommand() switch-case
```

当前已验证命令包括：

```text
PING / VERSION / STATUS / TIME / UART_STATS / APP_STATS / RESET_INFO / COMMAND_STATS
```

下一步：

```text
GET_FAULT_INFO snapshot + fault event integration
```
