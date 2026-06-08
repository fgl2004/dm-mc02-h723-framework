# 通信协议栈重构开发指导文档

> 文档版本：v1.0  
> 阶段名称：Protocol Stack Refactor  
> 当前目标：把现有串口通信从“REQ/RESP 阻塞式命令协议”重构为“RX 解析分发 + TX 优先级队列 + DMA 非阻塞发送”的完整通信中间件，为后续 EVENT、STREAM、BULK、文件系统、IMU 实时流、Bootloader 固件传输打基础。

---

## 1. 本阶段为什么要重构

当前系统已经具备基础命令通信能力：

```text
PC → MCU：REQ
MCU → PC：RESP / NACK
```

但是后续项目目标会更复杂：

```text
1. PC 发普通命令
2. MCU 返回命令响应
3. MCU 主动上报事件
4. MCU 高频发送 IMU 实时数据
5. PC 与 MCU 之间上传 / 下载文件
6. PC 读取 Flash / LittleFS 文件
7. 后续 Bootloader 接收固件包
8. 后续可能扩展 SPI / USB CDC / FDCAN 等其他传输层
```

如果继续使用旧模型：

```text
收到 REQ
  ↓
处理命令
  ↓
立即阻塞发送 RESP
  ↓
发送完成后才返回
```

会带来问题：

```text
1. RX 解析被 TX 阻塞
2. 高频 Stream 会影响命令响应
3. Bulk ACK 可能被延迟
4. 文件传输容易超时
5. 多个 App 可能抢 UART 发送
6. 未来接入 SPI / USB 时上层耦合严重
```

因此，本阶段必须重构为：

```text
RX 负责解析和分发
TX 负责入队和调度
实际发送由 ProtocolManager_ProcessTx() 统一完成
```

---

## 2. 本次重构的核心原则

### 2.1 帧格式暂时不推翻

现有帧格式继续保留：

```text
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
| SOF1 | SOF2  | VER | TYPE | FLAGS | SEQ | CMD | LEN    | PAYLOAD | CRC16 |
| 0xA5 | 0x5A  | 1B  | 1B   | 1B    | 1B  | 1B  | 2B LE  | N bytes | 2B LE |
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
```

当前 `protocol_frame` 层已经能完成：

```text
1. SOF 查找
2. Header 解析
3. Payload 收集
4. CRC16 校验
5. 支持 REQ / RESP / ACK / NACK / EVENT / DATA / WINDOW_ACK 类型
6. ProtocolFrame_Build()
```

因此，本阶段暂时不大改 `protocol_frame.c/.h`。

---

### 2.2 RX 和 TX 必须解耦

旧模型：

```text
RX 解析出完整帧
  ↓
处理命令
  ↓
直接阻塞发送响应
  ↓
发送完成后才继续 RX
```

新模型：

```text
RX 解析出完整帧
  ↓
按 TYPE 分发
  ↓
Handler 如需响应，只写入 TX 优先级队列
  ↓
立即返回
  ↓
ProtocolManager_ProcessTx() 后续用 DMA 发送
```

这意味着：

```text
解析出完整帧后，只负责分发；
分发过程中产生响应时，只能入 TX Queue；
不能在 RX 路径中阻塞发送。
```

---

### 2.3 所有发送统一经过 ProtocolManager

所有模块都不应该直接调用 UART 发送。

统一路径：

```text
CommandManager / EventManager / StreamManager / BulkTransferManager
  ↓
ProtocolManager_SendResp / SendNack / SendEvent / SendData / SendAck
  ↓
ProtocolFrame_Build()
  ↓
TX Priority Queue
  ↓
ProtocolManager_ProcessTx()
  ↓
PlatformUart_SendBufferDma()
```

---

### 2.4 DMA 只负责物理发送，不负责逻辑分块

文件、日志、固件包等大数据的分块不应该下沉到 UART DMA 层。

正确分层：

```text
BulkTransferManager：
  负责文件 chunk 切分、handle、offset、len、ACK、重传、CRC32

StreamManager：
  负责实时数据周期、SEQ、payload builder、丢帧统计

ProtocolManager：
  负责构造完整协议帧、入队、调度发送

PlatformUart：
  只负责启动一次 UART TX DMA，把完整帧字节发出去
```

PlatformUart 不应该知道：

```text
文件
offset
handle
stream_id
MORE_FRAG
ACK 策略
优先级
```

---

## 3. 最终通信系统架构

目标架构：

```text
App Layer
  ├── ImuApp
  ├── StorageApp
  ├── DiagnosticApp
  ├── McuInfoApp
  └── BootApp
        ↓
Manager Layer
  ├── CommandManager
  ├── EventManager
  ├── StreamManager
  ├── BulkTransferManager
  └── DataRouter
        ↓
ProtocolManager
        ↓
ProtocolFrame
        ↓
Transport Layer
  ├── UART Transport
  ├── SPI Transport     后续
  ├── USB CDC           后续
  └── FDCAN             后续
```

---

## 4. RX 接收路径设计

### 4.1 UART DMA RX 中断路径

当前 UART RX 使用：

```text
DMA Circular Buffer
  ├── Half Transfer interrupt
  ├── Transfer Complete interrupt
  └── UART IDLE interrupt
```

中断里只做：

```text
DMA buffer 新数据
  ↓
搬运到 RX RingBuffer
```

中断里不做：

```text
1. 帧解析
2. 命令处理
3. App 调用
4. TX 发送
```

---

### 4.2 主循环 RX 解析路径

```text
ProtocolManager_ProcessRx()
  ↓
从 RX RingBuffer 取字节
  ↓
ProtocolFrameParser_InputByte()
  ↓
如果组成完整帧
  ↓
ProtocolFrameParser_GetFrame()
  ↓
ProtocolManager_DispatchFrame()
```

伪代码：

```c
static void ProtocolManager_ProcessRx(void)
{
    uint8_t byte;
    ProtocolFrame_t frame;

    while (PlatformUart_RxAvailable() > 0U)
    {
        if (PlatformUart_ReadRx(&byte, 1U) != 1U)
        {
            break;
        }

        (void)ProtocolFrameParser_InputByte(&g_protocol.parser,
                                            byte,
                                            PlatformTime_GetMs());

        if (ProtocolFrameParser_HasFrame(&g_protocol.parser) != 0U)
        {
            if (ProtocolFrameParser_GetFrame(&g_protocol.parser,
                                             &frame,
                                             PlatformTime_GetMs()) == PROTO_FRAME_RESULT_OK)
            {
                ProtocolManager_DispatchFrame(&frame);
            }
        }
    }
}
```

---

### 4.3 TYPE 分发规则

```text
TYPE=REQ        → CommandManager
TYPE=EVENT      → EventManager
TYPE=DATA       → DataRouter
TYPE=ACK        → DataRouter
TYPE=WINDOW_ACK → DataRouter
TYPE=RESP/NACK  → MCU 侧暂时统计即可，PC 侧处理
```

伪代码：

```c
static void ProtocolManager_DispatchFrame(const ProtocolFrame_t *frame)
{
    switch (frame->type)
    {
        case PROTO_FRAME_TYPE_REQ:
            ProtocolManager_HandleReq(frame);
            break;

        case PROTO_FRAME_TYPE_EVENT:
            EventManager_HandleRxEvent(frame);
            break;

        case PROTO_FRAME_TYPE_DATA:
            DataRouter_HandleData(frame);
            break;

        case PROTO_FRAME_TYPE_ACK:
            DataRouter_HandleAck(frame);
            break;

        case PROTO_FRAME_TYPE_WINDOW_ACK:
            DataRouter_HandleWindowAck(frame);
            break;

        default:
            break;
    }
}
```

注意：分发过程中如果需要发送响应，只允许写 TX Queue，不允许阻塞发送。

---

## 5. TX 发送路径设计

### 5.1 发送总路径

```text
Manager / App 产生要发送的信息
  ↓
调用 ProtocolManager_SendXxx()
  ↓
ProtocolFrame_Build()
  ↓
写入 TX Priority Queue
  ↓
立即返回
  ↓
ProtocolManager_ProcessTx()
  ↓
UART TX DMA 空闲？
  ↓
取最高优先级队列中的一帧
  ↓
PlatformUart_SendBufferDma()
  ↓
TX DMA 完成中断
  ↓
PlatformUart_OnTxComplete()
  ↓
tx_busy = 0
  ↓
下一轮 ProtocolManager_ProcessTx() 发送下一帧
```

---

### 5.2 SendXxx 的语义

虽然接口叫 `SendResp`、`SendData`，但新语义不是“立即发送”，而是：

```text
Build frame + Push TX Queue
```

例如：

```c
int ProtocolManager_SendResp(uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len)
{
    return ProtocolManager_EnqueueFrame(PROTO_FRAME_TYPE_RESP,
                                        0U,
                                        seq,
                                        cmd,
                                        payload,
                                        payload_len,
                                        PROTOCOL_TX_PRIORITY_HIGH);
}
```

真正启动 DMA 的地方只有：

```text
ProtocolManager_ProcessTx()
```

---

## 6. TX 优先级队列设计

### 6.1 为什么需要优先级队列

未来系统中可能同时存在：

```text
1. RESP / NACK
2. Bulk ACK
3. 关键 EVENT
4. 普通 EVENT
5. Bulk DATA
6. Stream DATA
```

如果没有优先级队列，低优先级高频 Stream 可能占满发送通道，导致命令响应和 Bulk ACK 延迟。

因此必须加入：

```text
TX Priority Queue
```

---

### 6.2 三档优先级

```c
typedef enum
{
    PROTOCOL_TX_PRIORITY_HIGH = 0,
    PROTOCOL_TX_PRIORITY_NORMAL,
    PROTOCOL_TX_PRIORITY_LOW
} ProtocolTxPriority_t;
```

推荐映射：

| 优先级 | 帧类型 |
|---|---|
| HIGH | RESP、NACK、ACK、关键 EVENT |
| NORMAL | 普通 EVENT、Bulk DATA |
| LOW | Stream DATA |

---

### 6.3 队列元素

第一版建议队列中存完整帧字节：

```c
typedef struct
{
    uint16_t len;
    uint8_t data[PROTO_FRAME_MAX_SIZE];
} ProtocolTxPacket_t;
```

优点：

```text
1. 简单
2. 好调试
3. 发送时不用重新构帧
4. 不依赖上层 payload 生命周期
```

当前 `PROTO_FRAME_MAX_SIZE` 大约为：

```text
2 + 7 + 128 + 2 = 139 bytes
```

如果每个优先级队列深度为 8：

```text
139 * 8 * 3 ≈ 3336 bytes
```

RAM 消耗可以接受。

---

### 6.4 队列满处理策略

| 类型 | 队列满策略 |
|---|---|
| RESP / NACK | 不应静默丢弃，返回 BUSY 或记录严重错误 |
| ACK | 不应静默丢弃，否则 Bulk 会超时 |
| 关键 EVENT | 尽量保留，满了统计 critical_drop |
| 普通 EVENT | 可丢弃，但必须统计 |
| Bulk DATA | 不应静默丢弃，返回 BUSY，等待上层重试 |
| Stream DATA | 可以丢弃，统计 stream_drop_count |

---

### 6.5 ProcessTx 逻辑

```c
static void ProtocolManager_ProcessTx(void)
{
    ProtocolTxPacket_t packet;

    if (PlatformUart_IsTxBusy() != 0U)
    {
        return;
    }

    if (ProtocolManager_TxQueuePop(PROTOCOL_TX_PRIORITY_HIGH, &packet) == OK ||
        ProtocolManager_TxQueuePop(PROTOCOL_TX_PRIORITY_NORMAL, &packet) == OK ||
        ProtocolManager_TxQueuePop(PROTOCOL_TX_PRIORITY_LOW, &packet) == OK)
    {
        if (PlatformUart_SendBufferDma(packet.data, packet.len) != PLATFORM_UART_OK)
        {
            /* 记录 DMA 启动失败 */
        }
    }
}
```

建议第一版每次 `ProtocolManager_ProcessTx()` 最多发送一帧，避免主循环被发送调度占用太久。

---

## 7. PlatformUart 需要修改的内容

当前 `platform_uart.c` 已经实现：

```text
1. RX DMA circular buffer
2. RX half callback
3. RX complete callback
4. RX idle callback
5. DMA 数据搬运到 RX RingBuffer
6. 阻塞式 HAL_UART_Transmit()
```

本次需要新增 TX DMA 能力。

---

### 7.1 新增 TX 状态

建议新增：

```c
static volatile uint8_t g_uart_tx_busy;
```

统计信息增加：

```c
uint32_t tx_dma_start_count;
uint32_t tx_dma_done_count;
uint32_t tx_dma_error_count;
uint32_t tx_busy_count;
uint32_t tx_bytes;
```

---

### 7.2 新增 TX DMA 接口

```c
int PlatformUart_SendBufferDma(const uint8_t *buf, uint16_t len);
uint8_t PlatformUart_IsTxBusy(void);
void PlatformUart_OnTxComplete(void);
void PlatformUart_OnTxError(void);
```

逻辑：

```c
int PlatformUart_SendBufferDma(const uint8_t *buf, uint16_t len)
{
    if ((buf == NULL) || (len == 0U))
    {
        return PLATFORM_UART_INVALID_PARAM;
    }

    if (g_uart_tx_busy != 0U)
    {
        g_uart_stats.tx_busy_count++;
        return PLATFORM_UART_BUSY;
    }

    g_uart_tx_busy = 1U;

    if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)buf, len) != HAL_OK)
    {
        g_uart_tx_busy = 0U;
        g_uart_stats.tx_dma_error_count++;
        return PLATFORM_UART_ERROR;
    }

    g_uart_stats.tx_dma_start_count++;
    g_uart_stats.tx_bytes += len;

    return PLATFORM_UART_OK;
}
```

---

### 7.3 TX 完成中断

```c
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        PlatformUart_OnTxComplete();
    }
}
```

```c
void PlatformUart_OnTxComplete(void)
{
    g_uart_tx_busy = 0U;
    g_uart_stats.tx_dma_done_count++;
}
```

中断里只清 busy 和统计，不要在中断里继续发送下一帧。

---

## 8. ProtocolManager 需要修改的内容

### 8.1 新增 TX Queue

新增三个队列：

```text
high_tx_queue
normal_tx_queue
low_tx_queue
```

每个队列可用固定数组环形队列实现。

---

### 8.2 新增统一发送 API

```c
int ProtocolManager_SendResp(...);
int ProtocolManager_SendNack(...);
int ProtocolManager_SendEvent(...);
int ProtocolManager_SendData(...);
int ProtocolManager_SendAck(...);
int ProtocolManager_SendWindowAck(...);
```

这些函数只负责：

```text
1. 调用 ProtocolFrame_Build()
2. 写入 TX 优先级队列
3. 返回入队结果
```

---

### 8.3 修改 HandleReq

旧逻辑：

```text
HandleReq
  ↓
CommandManager_Dispatch
  ↓
直接发送 RESP/NACK
```

新逻辑：

```text
HandleReq
  ↓
CommandManager_Dispatch
  ↓
CommandManagerResponse_t
  ↓
ProtocolManager_SendResp / SendNack
  ↓
写入 TX Queue
  ↓
立即返回
```

---

### 8.4 修改 Process

```c
void ProtocolManager_Process(void)
{
    ProtocolManager_ProcessRx();
    ProtocolManager_ProcessTx();
}
```

---

## 9. 后续 Manager 规划

### 9.1 CommandManager

只处理：

```text
TYPE=REQ
```

输出：

```text
CommandManagerResponse_t
```

它不直接发 UART。

---

### 9.2 EventManager

后续新增：

```text
event_manager.h/.c
```

负责：

```text
1. EventManager_Post()
2. Event queue
3. 事件优先级
4. 调用 ProtocolManager_SendEvent()
```

最终替代 CommandManager 内部事件队列。

---

### 9.3 DataRouter

后续新增：

```text
data_router.h/.c
```

负责：

```text
TYPE=DATA / ACK / WINDOW_ACK 分发
```

---

### 9.4 StreamManager

后续新增：

```text
stream_manager.h/.c
```

负责：

```text
1. 注册 stream channel
2. 定时调用 App payload builder
3. 调用 ProtocolManager_SendData(... LOW)
4. SEQ 管理
5. 丢帧统计
```

---

### 9.5 BulkTransferManager

后续新增：

```text
bulk_transfer_manager.h/.c
```

负责：

```text
1. handle
2. offset
3. chunk seq
4. ACK
5. retry
6. timeout
7. CRC32
8. cancel
```

---

## 10. PC 端同步修改规划

MCU 改完后，PC 端也需要同步升级。

### 10.1 PC Protocol Client

需要支持：

```text
1. REQ 发送
2. RESP/NACK 匹配
3. EVENT 接收
4. DATA 接收
5. ACK/WINDOW_ACK 接收
6. SEQ 检测
7. 超时重试
```

---

### 10.2 PC Stream Client

需要支持：

```text
1. 按 stream cmd 解析 DATA
2. 检测 stream seq 是否连续
3. 统计 drop_count
4. 更新曲线和 3D UI
```

---

### 10.3 PC Bulk Client

需要支持：

```text
1. BULK_BEGIN
2. BULK_CHUNK
3. BULK_ACK 等待
4. BULK_END
5. CRC32 校验
6. 文件保存 / 读取
7. 进度条
8. 失败重试
```

---

### 10.4 PC UI

后续需要增加：

```text
1. Protocol Monitor
2. Event Log
3. Stream Monitor
4. Storage Raw Explorer
5. File Explorer
6. Bulk Transfer Progress
```

---

## 11. Git 开发建议

建议新建分支：

```bash
git checkout -b feature/protocol-tx-dma-priority-queue
```

第一阶段提交拆分建议：

```text
commit 1:
  platform_uart 增加 TX DMA 发送接口和统计

commit 2:
  protocol_manager 增加 TX 优先级队列

commit 3:
  protocol_manager SendResp/SendNack 改为入队

commit 4:
  保持原有 CMD/RESP 测试通过

commit 5:
  新增 DataRouter 空框架
```

不要一次性改太多，避免定位困难。

---

## 12. 当前第一步具体修改清单

### 必改文件

```text
platform_uart.h
platform_uart.c
protocol_manager.h
protocol_manager.c
```

### 暂时不改或少改文件

```text
protocol_frame.h
protocol_frame.c
command_service.h
command_service.c
storage_app.h
storage_app.c
imu_app.h
imu_app.c
```

### 后续新增文件

```text
data_router.h/.c
event_manager.h/.c
stream_manager.h/.c
bulk_transfer_manager.h/.c
```

---

## 13. 当前阶段验收标准

第一步完成后，至少满足：

```text
1. 旧的 PING / GET_STATUS / IMU 命令仍然可用
2. RESP 不再阻塞发送，而是入 TX Queue
3. TX 使用 DMA 发送
4. DMA 完成后 tx_busy 清零
5. 连续发送多个命令时不会卡死
6. RX 半满 / 全满 / IDLE 中断仍然正常
7. RX RingBuffer 无明显 overflow
8. ProtocolFrameParser 统计正常
9. TX Queue 有统计信息
10. PC 脚本能正常收到 RESP/NACK
```

---

## 14. 最终结论

本阶段的重点不是马上做文件系统，也不是马上迁移 IMU Stream。

本阶段的重点是完成通信底座重构：

```text
RX：
DMA → RX RingBuffer → ProtocolFrameParser → TYPE 分发

TX：
Manager → ProtocolManager_SendXxx → TX Priority Queue → UART TX DMA
```

最关键的架构变化是：

```text
RX 分发过程中不再阻塞发送；
所有响应只进入 TX 队列；
真正发送由 ProtocolManager_ProcessTx() 统一调度；
底层使用 UART DMA 非阻塞发送。
```

这个基础完成后，后续再添加：

```text
EventManager
DataRouter
StreamManager
BulkTransferManager
Raw Storage
LittleFS
PC Storage Explorer
```

整个系统才会稳。
