# UART Reliable Protocol Design

## 1. Document Purpose

本文档用于设计 `DM-MC02 H723 Embedded Framework` 项目的 Stage 2 UART 可靠通信协议。

Stage 1 已经完成：

```text
USART1 printf
Platform UART
Platform Time
Platform Reset
Platform Fault
Board Log
App_Init / App_Run
```

Stage 2 的目标是在已有 USART1 生命线基础上，构建一个可扩展、可诊断、可测试、可逐步增强的 PC ↔ MCU 可靠通信协议。

本协议不是简单的串口字符串命令，而是面向后续参数管理、诊断查询、固件升级、安全认证、自动化测试等功能设计的二进制通信协议。

---

## 2. Stage 2 Goal

Stage 2 的核心目标：

```text
1. 建立 UART DMA 高效接收路径
2. 建立 RingBuffer 字节流缓存
3. 建立通用状态机框架
4. 建立协议帧格式
5. 实现 Frame Parser
6. 实现 CRC16 校验
7. 实现基础命令：PING / GET_VERSION / GET_STATUS
8. 实现 PC Python Tool
9. 支持半包、粘包、CRC 错误、垃圾字节恢复
10. 为 ACK / NACK / 重传 / 滑动窗口预留升级空间
```

Stage 2 的第一版不追求一次实现完整 TCP 式可靠传输，而是采用分阶段演进方式：

```text
V1: Reliable Frame + CRC + REQ/RESP
V2: SEQ + ACK/NACK + Timeout Retry
V3: Small Sliding Window
V4: Fragment / File Transfer / Upgrade Channel
V5: Security / HMAC / Encryption Extension
```

---

## 3. Protocol Layering

UART 协议按以下层次设计：

```text
PC Tool
  ↓
Command Layer
  ↓
Reliability Layer
  ↓
Frame Layer
  ↓
Byte Stream Layer
  ↓
UART DMA / RingBuffer
```

各层职责：

| Layer                 | Responsibility                    |
| --------------------- | --------------------------------- |
| UART DMA / RingBuffer | 高效接收 UART 字节流                     |
| Byte Stream Layer     | 从 RingBuffer 中持续取字节               |
| Frame Layer           | 通过状态机解析完整协议帧                      |
| Reliability Layer     | SEQ、ACK/NACK、超时、重传、窗口机制           |
| Command Layer         | PING、GET_VERSION、GET_STATUS 等命令处理 |
| PC Tool               | 组帧、发送、接收、测试、自动化报告                 |

设计原则：

```text
字节流层不理解命令。
帧解析层不处理业务。
可靠性层不直接操作硬件。
命令层不关心 UART DMA 和 RingBuffer。
```

---

## 4. UART DMA Receive Design

### 4.1 Why DMA Receive

UART 是字节流接口，如果使用单字节中断接收，高波特率或大量数据时 CPU 中断压力较大。

因此 Stage 2 计划采用：

```text
UART RX DMA Circular Mode
Half Transfer Interrupt
Transfer Complete Interrupt
Idle Line Interrupt
```

目标是：

```text
1. 降低 UART 接收中断频率
2. 支持连续字节流接收
3. 支持半包、粘包场景
4. 支持后续大数据传输
5. 为固件升级和滑动窗口打基础
```

---

### 4.2 DMA RX Buffer

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

### 4.3 DMA Event Handling

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

这样可以处理 DMA 环形回绕。

---

### 4.4 DMA + D-Cache Issue

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

## 5. RingBuffer Design

UART DMA 只负责把字节搬到 DMA buffer，协议解析不应直接处理 DMA buffer。

中间增加 RX RingBuffer：

```text
UART DMA Buffer
  ↓
RX RingBuffer
  ↓
Frame Parser
```

RingBuffer 作用：

```text
1. 解耦 UART 接收和协议解析
2. 支持半包和粘包
3. 支持不同速率的生产者/消费者
4. 统计 overflow 和 high water mark
5. 后续可被 Buffer Manager 统一管理
```

建议第一版：

```c
#define UART_RX_RING_SIZE 1024
#define UART_TX_RING_SIZE 1024
```

RingBuffer 基础接口：

```c
void RingBuffer_Init(RingBuffer_t *rb, uint8_t *buf, uint16_t size);
uint16_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, uint16_t len);
uint16_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, uint16_t len);
uint16_t RingBuffer_Available(const RingBuffer_t *rb);
uint16_t RingBuffer_Free(const RingBuffer_t *rb);
void RingBuffer_Clear(RingBuffer_t *rb);
```

统计信息：

```c
typedef struct
{
    uint32_t write_bytes;
    uint32_t read_bytes;
    uint32_t overflow_count;
    uint16_t high_watermark;
} RingBufferStats_t;
```

---

## 6. Generic State Machine Framework

Stage 2 中至少会出现多个状态机：

```text
Frame Parser State Machine
Protocol Reliability State Machine
Command Processing State Machine
Future Upgrade State Machine
Future Security Authentication State Machine
```

因此需要提前设计一个通用状态机框架。

---

### 6.1 State Machine Goal

通用状态机框架目标：

```text
1. 统一状态切换风格
2. 统一事件驱动方式
3. 支持 enter / exit / event handler
4. 支持状态切换日志
5. 支持状态停留时间统计
6. 支持后续 Diagnostic Manager 查询
```

---

### 6.2 Generic State Machine Concept

建议抽象如下：

```c
typedef uint16_t StateId_t;
typedef uint16_t EventId_t;

typedef struct
{
    StateId_t current_state;
    StateId_t previous_state;
    uint32_t state_enter_time_ms;
    uint32_t transition_count;
    uint32_t error_count;
} StateMachine_t;
```

状态处理函数：

```c
typedef void (*StateEnterFunc_t)(void *ctx);
typedef void (*StateExitFunc_t)(void *ctx);
typedef void (*StateEventFunc_t)(void *ctx, EventId_t event, const void *event_data);
```

状态描述：

```c
typedef struct
{
    StateId_t state;
    StateEnterFunc_t on_enter;
    StateExitFunc_t on_exit;
    StateEventFunc_t on_event;
} StateDef_t;
```

基础接口：

```c
void StateMachine_Init(StateMachine_t *sm, StateId_t init_state);
void StateMachine_Transition(StateMachine_t *sm, StateId_t next_state);
void StateMachine_Dispatch(StateMachine_t *sm, EventId_t event, const void *event_data);
StateId_t StateMachine_GetState(const StateMachine_t *sm);
```

第一版可以先实现最小框架，后续逐步扩展。

---

### 6.3 Where to Put State Machine

通用状态机应放在 Middleware：

```text
firmware/app/Middleware/
  state_machine.h
  state_machine.c
```

原因：

```text
它不是某个具体业务。
它不是某个硬件平台能力。
它是通用机制，可复用于 Protocol、Upgrade、Security、Device Manager。
```

---

## 7. Protocol Frame Format

协议采用二进制帧格式。

第一版帧格式：

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
| CMD     |    1 | Command ID                          |
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
```

V2/V3 再逐步启用：

```text
ACK
DATA
WINDOW_ACK
EVENT
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

## 10. Command ID

第一版命令：

| CMD            |  Value | Direction | Description                |
| -------------- | -----: | --------- | -------------------------- |
| PING           | `0x01` | PC → MCU  | Communication test         |
| GET_VERSION    | `0x02` | PC → MCU  | Get firmware version       |
| GET_STATUS     | `0x03` | PC → MCU  | Get runtime status         |
| GET_RESET_INFO | `0x04` | PC → MCU  | Get reset information      |
| GET_TIME_INFO  | `0x05` | PC → MCU  | Get tick / DWT information |

后续扩展命令：

| CMD                |  Value | Description                |
| ------------------ | -----: | -------------------------- |
| GET_FAULT_INFO     | `0x10` | Get last fault information |
| PARAM_GET          | `0x20` | Get parameter              |
| PARAM_SET          | `0x21` | Set parameter              |
| ENTER_BOOTLOADER   | `0x30` | Enter bootloader           |
| FW_TRANSFER        | `0x31` | Firmware transfer          |
| SECURITY_CHALLENGE | `0x40` | Security challenge         |
| SECURITY_AUTH      | `0x41` | Security authentication    |

---

## 11. Error Code

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

---

## 12. Frame Parser State Machine

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
  error_count++
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

## 13. Reliability Mechanism Roadmap

### 13.1 V1: Request / Response

V1 采用基础请求响应模型：

```text
PC sends REQ
MCU parses frame
MCU executes command
MCU sends RESP or NACK
```

特点：

```text
简单
稳定
适合 PING / GET_VERSION / GET_STATUS
容易调试
```

---

### 13.2 V2: Stop-and-Wait ARQ

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

### 13.3 V3: Small Sliding Window

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

示例：

```text
PC sends DATA seq=10
PC sends DATA seq=11
PC sends DATA seq=12
PC sends DATA seq=13

MCU receives 10, 11, 12, 13
MCU sends WINDOW_ACK next_expected_seq=14
```

丢包示例：

```text
PC sends DATA seq=10
PC sends DATA seq=11
PC sends DATA seq=12
PC sends DATA seq=13

MCU receives 10, 11, 13
MCU expects 12
MCU sends WINDOW_ACK next_expected_seq=12

PC retransmits from seq=12
```

第一版滑动窗口不支持乱序提交。

如果收到乱序帧：

```text
discard frame
return WINDOW_ACK with current next_expected_seq
```

后续可扩展 bitmap selective ACK。

---

### 13.4 V4: Fragment and File Transfer

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

### 13.5 V5: Security Extension

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

## 14. Module Plan

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

PC 工具规划：

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

## 15. Stage 2 Implementation Plan

推荐实现顺序：

| Step | Task                     | Output                                     |
| ---- | ------------------------ | ------------------------------------------ |
| 2.1  | Protocol design document | `docs/04_uart_reliable_protocol_design.md` |
| 2.2  | RingBuffer middleware    | `ring_buffer.h/.c`                         |
| 2.3  | CRC16 middleware         | `crc16.h/.c`                               |
| 2.4  | Generic state machine    | `state_machine.h/.c`                       |
| 2.5  | UART DMA RX path         | `platform_uart` RX extension               |
| 2.6  | Protocol frame parser    | `protocol_frame.h/.c`                      |
| 2.7  | Protocol Manager         | `protocol_manager.h/.c`                    |
| 2.8  | Command Manager          | `command_manager.h/.c`                     |
| 2.9  | Basic commands           | PING / GET_VERSION / GET_STATUS            |
| 2.10 | Python PC Tool           | `h7tool`                                   |
| 2.11 | Robustness test          | half packet / sticky packet / CRC error    |
| 2.12 | Reliability V2           | ACK / NACK / timeout / retry               |
| 2.13 | Sliding window V3        | small window for data transfer             |

---

## 16. Stage 2 Test Plan

基础测试：

```text
1. PC sends PING, MCU returns PONG
2. PC sends GET_VERSION, MCU returns version
3. PC sends GET_STATUS, MCU returns status
4. PC sends invalid CMD, MCU returns UNKNOWN_CMD
5. PC sends invalid CRC, MCU rejects frame
6. PC sends half packet, parser waits
7. PC sends sticky packets, parser extracts multiple frames
8. PC sends garbage bytes before valid frame, parser recovers
9. PC sends payload length overflow, parser rejects
10. RX RingBuffer overflow counter increments correctly
```

DMA 测试：

```text
1. DMA half transfer event works
2. DMA transfer complete event works
3. UART IDLE event works
4. DMA circular wrap works
5. DMA data can be moved to RingBuffer correctly
6. Protocol parser works with DMA source
```

可靠性测试：

```text
1. SEQ matches request and response
2. Timeout retry works
3. Duplicate request is detected
4. NACK is generated for invalid frame or state
5. Sliding window cumulative ACK works in V3
```

---

## 17. Stage 2 Acceptance Criteria

Stage 2 第一阶段完成标准：

```text
1. UART RX DMA can receive continuous bytes
2. RX RingBuffer works
3. Frame parser can recover valid frames from byte stream
4. CRC16 can detect corrupted frames
5. PING / GET_VERSION / GET_STATUS work
6. PC Python tool can send commands and parse responses
7. Half packet / sticky packet / garbage bytes can be handled
8. Protocol statistics can be printed or queried
```

Stage 2 增强阶段完成标准：

```text
1. SEQ request-response matching works
2. ACK/NACK frame works
3. Timeout retry works
4. Duplicate detection works
5. Small sliding window design is implemented or partially implemented
```

---

## 18. Design Rules

后续实现中遵循以下规则：

```text
1. 中断中只做最少工作
2. DMA callback 只搬运数据或记录事件
3. 协议解析在主循环或 ProtocolManager_Process 中执行
4. Parser 不直接执行命令
5. Command Manager 不直接操作 UART
6. 所有错误必须有计数器
7. 所有 buffer 必须有 high watermark 和 overflow 统计
8. 所有可靠性机制必须可关闭或分阶段启用
9. 所有测试宏默认安全关闭
10. 每个小闭环完成后提交一次 Git
```

---

## 19. Current Decision

当前决策：

```text
1. 使用二进制帧协议
2. 使用 SOF + LEN + CRC16
3. 使用 FLAGS + SEQ 为可靠机制预留空间
4. 使用 UART DMA circular receive
5. 使用 half transfer / transfer complete / idle event 处理 DMA 接收
6. 使用 RingBuffer 解耦 DMA 接收和协议解析
7. 使用通用状态机框架承载 Frame Parser 和后续升级状态机
8. 第一版先实现 REQ / RESP / NACK
9. 第二版加入 ACK / retry
10. 第三版加入 small sliding window
```
