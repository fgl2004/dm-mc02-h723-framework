# 完备通信协议重构规划文档

> 文档版本：v1.0  
> 阶段定位：Protocol Stack Refactor  
> 当前目标：在现有 UART 协议栈基础上，重构出一个可以同时支撑命令、事件、实时流、文件块传输，并且后续可扩展到 SPI / USB / FDCAN 等传输介质的完整通信系统。

---

## 1. 为什么要重构通信协议栈

目前系统已经具备基础的命令收发能力：

```text
PC → MCU：REQ
MCU → PC：RESP / NACK
```

现有协议已经可以完成：

```text
1. 查询 MCU 状态
2. 查询 IMU 状态
3. 启动 / 停止 IMU
4. 查询 Storage 状态
5. 执行简单自测试
6. 上报低频事件
```

但是后续目标不再只是“串口命令测试”，而是要让上位机和 MCU 之间同时完成：

```text
1. 命令交互
2. 异步事件上报
3. IMU 实时数据流
4. 文件上传 / 下载
5. Flash 分区读写
6. 日志导出
7. Bootloader 固件暂存和升级
8. 后续可能扩展到 SPI / USB / FDCAN 等其他物理接口
```

因此，现在的通信栈需要从：

```text
简单 REQ / RESP 命令协议
```

升级成：

```text
CMD + EVENT + STREAM + BLOCK 的统一通信协议栈
```

---

## 2. 总体目标

本次通信协议重构的最终目标是：

```text
PC 可以一边发命令，
一边接收 MCU 事件，
一边接收 IMU 实时数据，
一边上传 / 下载文件，
MCU 端仍然可以稳定解析、调度、响应和发送。
```

最终系统需要满足：

```text
1. 所有发送统一经过 ProtocolManager
2. 所有接收统一经过 ProtocolFrameParser
3. CommandManager 只负责命令
4. EventManager 只负责事件
5. StreamManager 只负责实时流
6. BulkTransferManager 只负责可靠块传输
7. DataRouter 负责 DATA / ACK / WINDOW_ACK 的二级分发
8. ProtocolManager 内部加入 TX 优先级队列
9. 上层 App 不直接操作 UART / SPI / ProtocolFrame
10. 后续可以替换底层传输接口
```

---

## 3. 现有帧格式保持不变

当前协议帧格式继续保留：

```text
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
| SOF1 | SOF2  | VER | TYPE | FLAGS | SEQ | CMD | LEN    | PAYLOAD | CRC16 |
| 0xA5 | 0x5A  | 1B  | 1B   | 1B    | 1B  | 1B  | 2B LE  | N bytes | 2B LE |
+------+-------+-----+------+-------+-----+-----+--------+---------+-------+
```

字段含义：

| 字段 | 含义 |
|---|---|
| SOF1 / SOF2 | 帧头，用于字节流同步 |
| VER | 协议版本 |
| TYPE | 帧类型 |
| FLAGS | 传输控制标志 |
| SEQ | 帧序号 |
| CMD | 命令号 / 事件号 / 数据通道号 |
| LEN | Payload 长度，小端 |
| PAYLOAD | 数据负载 |
| CRC16 | 单帧 CRC 校验 |

当前不建议推翻帧格式，因为它已经具备扩展基础。真正需要改的是：

```text
1. ProtocolManager 的分发结构
2. DATA / ACK / WINDOW_ACK 的处理路径
3. TX 发送调度
4. Manager 层职责划分
```

---

## 4. 四类通信语义

### 4.1 CMD：命令通信

CMD 用于 PC 和 MCU 之间的控制、查询、配置。

```text
TYPE = REQ / RESP / NACK
CMD  = 命令号
SEQ  = 请求序号
```

典型流程：

```text
PC  → MCU：REQ  seq=N cmd=GET_STATUS
MCU → PC ：RESP seq=N cmd=GET_STATUS
```

或者：

```text
MCU → PC：NACK seq=N cmd=GET_STATUS error=xxx
```

CMD 适合：

```text
1. 获取版本
2. 获取状态
3. 启动 / 停止 IMU
4. 查询 Storage 信息
5. 创建 Bulk 传输会话
6. 结束 Bulk 传输会话
7. Mount / Format 文件系统
```

CMD 不适合：

```text
1. 高频 IMU 数据
2. 大文件 chunk
3. 连续日志流
```

---

### 4.2 EVENT：异步事件

EVENT 用于 MCU 主动向 PC 报告某些事情发生了。

```text
TYPE = EVENT
CMD  = event_id
SEQ  = event_seq
```

典型事件：

```text
1. 系统启动
2. 复位原因
3. 故障发生
4. Storage 初始化完成
5. Storage 错误
6. IMU 校准完成
7. IMU 异常
8. 文件上传完成
9. 文件上传失败
```

EVENT 的特点：

```text
1. 低频
2. 异步
3. 可排队
4. 可设置优先级
5. 可被丢弃但必须统计
6. 不做强制重传
```

EVENT 不应该承载高频姿态数据。IMU 姿态流后续应该迁移到 STREAM。

---

### 4.3 STREAM：实时流

STREAM 用于 MCU 向 PC 持续发送实时数据。

```text
TYPE = DATA
CMD  = stream_channel_id
SEQ  = stream_seq
FLAGS = 0
```

典型 Stream：

```text
1. IMU 姿态角
2. 加速度 / 陀螺仪曲线
3. 实时诊断曲线
4. 实时日志 tail
5. 运行状态波形
```

STREAM 的原则：

```text
实时性优先，完整性次要。
宁可丢帧，不要阻塞系统。
```

STREAM 不做强制 ACK，不做重传。SEQ 的意义是：

```text
1. PC 检测丢帧
2. PC 检测乱序
3. PC 计算丢包率
4. PC 评估通信质量
```

推荐 Payload 头：

```c
typedef struct
{
    uint8_t  stream_id;
    uint8_t  sample_count;
    uint16_t format;
    uint32_t tick_ms;
    uint8_t  data[];
} StreamPayloadHeader_t;
```

---

### 4.4 BLOCK：可靠块传输

BLOCK 用于文件、日志、Flash 数据、固件包等大块数据。

控制命令：

```text
TYPE = REQ / RESP
CMD  = BULK_BEGIN / BULK_END / BULK_CANCEL
```

数据块：

```text
TYPE = DATA
CMD  = BULK_CHUNK
FLAGS = ACK_REQ | MORE_FRAG | IS_RETRY
SEQ = chunk_seq
PAYLOAD = handle + offset + len + data
```

确认：

```text
TYPE = ACK
CMD  = BULK_CHUNK
SEQ  = 被确认 chunk seq
PAYLOAD = handle + next_offset
```

BLOCK 的原则：

```text
完整性优先，实时性次要。
宁可慢，不可错。
```

BLOCK 必须支持：

```text
1. handle
2. offset
3. len
4. seq
5. ACK
6. timeout
7. retry
8. duplicate detection
9. cancel
10. final CRC32
```

CRC16 只保证单帧正确，CRC32 用于保证整个文件 / 数据块最终正确。

---

## 5. SEQ 的意义

SEQ 在不同通信形态中有不同含义。

### 5.1 CMD SEQ

```text
REQ.seq 由 PC 生成
RESP.seq 必须等于 REQ.seq
NACK.seq 必须等于 REQ.seq
```

作用：

```text
PC 匹配请求和响应
支持超时重试
避免响应错配
```

---

### 5.2 EVENT SEQ

```text
EVENT.seq 由 MCU 自增
```

作用：

```text
PC 检测事件是否丢失
统计事件丢包
```

---

### 5.3 STREAM SEQ

```text
每个 stream channel 独立递增
```

作用：

```text
检测丢帧
检测乱序
统计实时流质量
```

STREAM 不因为丢帧而重传。

---

### 5.4 BLOCK SEQ

```text
每个 bulk session 内 chunk seq 递增
```

但 BLOCK 不能只靠 1 字节 SEQ 定位数据，因为大文件可能超过 256 个 chunk。

真正强定位依靠：

```text
handle + offset
```

SEQ 用于顺序检测和 ACK 辅助。

---

## 6. 最终通信架构

推荐最终架构如下：

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
  ├── SPI Transport   后续
  ├── USB CDC         后续
  └── FDCAN           后续
```

---

## 7. ProtocolManager 的最终职责

ProtocolManager 是整个通信系统的统一入口和统一出口。

### 7.1 接收方向

```text
Transport RX
  ↓
ProtocolFrameParser
  ↓
ProtocolManager
  ↓
根据 TYPE 分发
```

分发规则：

```text
TYPE=REQ        → CommandManager
TYPE=EVENT      → EventManager
TYPE=DATA       → DataRouter
TYPE=ACK        → DataRouter
TYPE=WINDOW_ACK → DataRouter
TYPE=RESP/NACK  → PC 侧使用；MCU 侧统计即可
```

---

### 7.2 发送方向

所有发送都必须通过 ProtocolManager：

```text
CommandManager / EventManager / StreamManager / BulkTransferManager
  ↓
ProtocolManager_SendXxx()
  ↓
TX Priority Queue
  ↓
ProtocolManager_ProcessTx()
  ↓
Transport Send
```

建议公开接口：

```c
int ProtocolManager_SendResp(uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len);

int ProtocolManager_SendNack(uint8_t seq,
                             uint8_t cmd,
                             uint8_t error_code);

int ProtocolManager_SendEvent(uint8_t event_id,
                              const uint8_t *payload,
                              uint16_t payload_len,
                              uint8_t priority);

int ProtocolManager_SendData(uint8_t flags,
                             uint8_t seq,
                             uint8_t cmd,
                             const uint8_t *payload,
                             uint16_t payload_len,
                             uint8_t priority);

int ProtocolManager_SendAck(uint8_t seq,
                            uint8_t cmd,
                            const uint8_t *payload,
                            uint16_t payload_len);

int ProtocolManager_SendWindowAck(uint8_t seq,
                                  uint8_t cmd,
                                  const uint8_t *payload,
                                  uint16_t payload_len);
```

---

## 8. TX 优先级队列设计

这是本次重构必须加入的重点。

### 8.1 为什么需要 TX 优先级队列

未来 MCU 可能同时发送：

```text
1. 命令响应 RESP
2. 错误响应 NACK
3. Bulk ACK
4. Storage 事件
5. IMU 实时流
6. Bulk 文件数据
```

如果没有统一发送队列，就会出现：

```text
1. Stream 占满串口，命令响应变慢
2. Bulk ACK 延迟，文件传输超时
3. 重要事件被低优先级数据淹没
4. 多个模块同时直接发 UART，互相抢占
```

所以 ProtocolManager 必须统一调度所有发送帧。

---

### 8.2 优先级划分

建议三档优先级：

```text
HIGH    高优先级
NORMAL  普通优先级
LOW     低优先级
```

推荐映射：

| 优先级 | 典型帧 |
|---|---|
| HIGH | ACK、NACK、RESP、关键 EVENT |
| NORMAL | 普通 EVENT、Bulk DATA |
| LOW | Stream DATA |

原因：

```text
ACK / NACK / RESP 影响协议闭环，必须尽快发送。
EVENT 是重要通知，但不能长期压制命令。
Bulk DATA 可以慢一点，但不能无声丢弃。
Stream DATA 可以丢，因为实时性优先。
```

---

### 8.3 队列元素

第一版建议队列里存完整帧字节：

```c
typedef struct
{
    uint16_t len;
    uint8_t  data[PROTO_FRAME_MAX_SIZE];
} ProtocolTxPacket_t;
```

优点：

```text
1. 简单
2. 发送时不用重新构帧
3. 好调试
4. 适合当前阶段
```

当前 `PROTO_FRAME_MAX_SIZE` 约为：

```text
2 + 7 + 128 + 2 = 139 bytes
```

如果每个优先级队列深度为 8：

```text
139 * 8 * 3 ≈ 3336 bytes
```

RAM 消耗可以接受。

---

### 8.4 发送调度规则

`ProtocolManager_ProcessTx()` 每次循环优先发送：

```text
1. HIGH 队列
2. NORMAL 队列
3. LOW 队列
```

建议第一版每次 `ProtocolManager_Process()` 最多发送 1~2 帧，避免阻塞主循环太久。

---

### 8.5 队列满时策略

不同类型有不同策略：

```text
RESP / NACK / ACK：
  不应丢弃，返回 BUSY 或进入高优先级队列

关键 EVENT：
  尽量保留，队列满则统计 critical_drop

普通 EVENT：
  可以丢弃，但必须统计 drop_count

Bulk DATA：
  不应静默丢弃，返回 BUSY，等待后续重试

Stream DATA：
  可以直接丢弃，统计 stream_drop_count
```

---

## 9. Manager 层职责

### 9.1 CommandManager

只负责：

```text
TYPE=REQ
```

职责：

```text
1. 命令调度
2. 调用 CommandService
3. 返回 RESP/NACK
4. 统计命令耗时
```

不负责 DATA、STREAM、BLOCK。

---

### 9.2 EventManager

负责：

```text
TYPE=EVENT
```

职责：

```text
1. EventManager_Post()
2. 事件队列
3. 事件优先级
4. 事件订阅/过滤
5. 事件发送统计
6. 事件丢弃统计
```

App 不直接发 EVENT 帧，只调用 EventManager。

---

### 9.3 DataRouter

负责：

```text
TYPE=DATA / ACK / WINDOW_ACK
```

职责：

```text
1. 按 CMD 分发 DATA
2. 按 CMD 分发 ACK
3. 按 CMD 分发 WINDOW_ACK
4. 不处理复杂业务状态
```

DataRouter 后面连接：

```text
StreamManager
BulkTransferManager
```

---

### 9.4 StreamManager

负责实时流。

职责：

```text
1. 注册 stream channel
2. start / stop stream
3. 设置周期
4. 维护 stream seq
5. 调用 App payload builder
6. 通过 ProtocolManager_SendData() 发送
7. 统计发送数 / 丢弃数 / 最大耗时
```

App 不直接发 DATA stream。

---

### 9.5 BulkTransferManager

负责可靠块传输。

职责：

```text
1. 创建传输会话
2. 分配 handle
3. 管理 offset
4. 管理 chunk seq
5. 发送 / 接收 DATA chunk
6. 发送 / 接收 ACK
7. 超时重传
8. 取消传输
9. CRC32 最终校验
10. 调用 Storage / FileSystem 回调
```

App 不直接处理 ACK / RETRY / WINDOW。

---

## 10. App 层应该关注什么

App 不应该关心：

```text
SOF
CRC16
SEQ
ACK
UART
SPI
ProtocolFrame_Build
TX Queue
```

App 只关心业务。

### 10.1 对 CMD

App 关注：

```text
命令是什么？
参数是什么？
当前状态能否执行？
结果是什么？
```

---

### 10.2 对 EVENT

App 关注：

```text
发生了什么事件？
事件 payload 是什么？
事件重要性如何？
```

调用：

```c
EventManager_Post(event_id, payload, len, priority);
```

---

### 10.3 对 STREAM

App 关注：

```text
当前样本是什么？
如何填充 payload？
数据是否有效？
```

注册：

```c
StreamManager_RegisterChannel(stream_cmd,
                              period_ms,
                              App_BuildPayload,
                              ctx,
                              name);
```

---

### 10.4 对 BLOCK

App 关注：

```text
数据写到哪里？
从哪里读数据？
offset 是否合法？
最终文件是否 commit？
```

注册：

```c
BulkTransferManager_RegisterTarget(target_id,
                                   write_cb,
                                   read_cb,
                                   finish_cb,
                                   ctx);
```

---

## 11. 传输层抽象

虽然当前主要使用 UART，但后续可能扩展 SPI、USB CDC、FDCAN 等。

因此建议后续抽象一层：

```c
typedef struct
{
    int (*init)(void);
    int (*read)(uint8_t *buf, uint16_t max_len, uint16_t *read_len);
    int (*send)(const uint8_t *buf, uint16_t len);
    int (*is_tx_busy)(void);
    void *ctx;
} ProtocolTransport_t;
```

当前阶段可先继续使用 `PlatformUart_ReadRx()` 和 `PlatformUart_SendBuffer()`。

但设计上要避免上层直接依赖 UART。

---

## 12. PC 端也需要相同分层

PC 端最终也应该有类似结构：

```text
SerialPort / Transport
  ↓
ProtocolFrameParser
  ↓
ProtocolClient
  ├── CommandClient
  ├── EventClient
  ├── StreamClient
  └── BulkFileClient
```

PC 端需要实现：

```text
1. CMD 请求 / 响应匹配
2. EVENT 事件显示
3. STREAM 数据解析、SEQ 检测、丢包统计
4. BULK 上传 / 下载状态机
5. 超时重试
6. CRC32 校验
7. 文件保存 / 读取
```

否则 MCU 端协议再完整，PC 端也无法真正配合。

---

## 13. 推荐开发顺序

### Step 1：ProtocolManager 加 TX 优先级队列

修改：

```text
protocol_manager.h
protocol_manager.c
```

新增：

```text
SendResp
SendNack
SendEvent
SendData
SendAck
SendWindowAck
ProcessTx
TX Priority Queue
```

---

### Step 2：新增 DataRouter

新增：

```text
data_router.h
data_router.c
```

实现：

```text
DATA / ACK / WINDOW_ACK 分发
```

---

### Step 3：新增 EventManager

新增：

```text
event_manager.h
event_manager.c
```

逐步替代 CommandManager 内部 event queue。

---

### Step 4：新增 StreamManager

新增：

```text
stream_manager.h
stream_manager.c
```

先做测试流：

```text
TYPE=DATA
CMD=0xA0
payload=tick+counter
period=100ms
```

PC 端验证：

```text
seq 是否连续
drop_count
rx_rate
```

---

### Step 5：迁移 IMU 到 Stream

将 IMU 姿态数据从 EVENT 路线迁移到：

```text
TYPE=DATA + StreamManager
```

---

### Step 6：新增 BulkTransferManager

新增：

```text
bulk_transfer_manager.h
bulk_transfer_manager.c
```

第一版：

```text
单会话
stop-and-wait
chunk=64B
每 chunk ACK
final CRC32
支持 cancel
```

---

### Step 7：Raw Storage 小读写

实现：

```text
STORAGE_RAW_READ
STORAGE_RAW_WRITE
STORAGE_RAW_ERASE
```

先验证：

```text
PC → MCU → StorageManager → Flash
```

---

### Step 8：Raw Bulk 文件传输

实现：

```text
RAW_UPLOAD_BEGIN
RAW_UPLOAD_CHUNK
RAW_UPLOAD_END
RAW_DOWNLOAD_BEGIN
RAW_DOWNLOAD_CHUNK
RAW_DOWNLOAD_END
```

---

### Step 9：LittleFS / 文件系统

最后再上：

```text
FS_MOUNT
FS_FORMAT
FS_LIST
FS_READ
FS_WRITE
FS_DELETE
FS_MKDIR
FS_RENAME
```

---

## 14. 最终结论

本次通信重构不是推翻已有协议，而是在现有帧格式上补齐通信层次。

最终目标是形成：

```text
CommandManager：命令
EventManager：事件
StreamManager：实时流
BulkTransferManager：可靠块
DataRouter：DATA 分发
ProtocolManager：统一收发和优先级调度
Transport：底层物理通道
```

完成后系统可以支持：

```text
1. PC 命令控制 MCU
2. MCU 事件主动上报 PC
3. MCU 实时发送 IMU 数据
4. PC 与 MCU 上传 / 下载文件
5. Flash / LittleFS / Bootloader 共享同一个 Bulk 传输底座
6. 后续扩展 SPI / USB / FDCAN 不影响 App 层
```

这套架构已经不是简单串口协议，而是一个完整的嵌入式通信中间件。
