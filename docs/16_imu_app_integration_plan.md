# 16. IMU App Integration Plan

## 1. 文档目的

本文档用于指导 **Stage 4: IMU and Algorithm Loop** 的开发。

Stage 2 和 Stage 3 已经完成了通信与诊断基础设施。进入 Stage 4 后，`IMU App` 不应该重新发明通信、事件、诊断、统计、测试机制，而应该直接接入现有框架。

本文档的目标是：

```text
1. 记录当前系统已经完成的通信与诊断能力
2. 说明 IMU App 应该如何接入这些能力
3. 规划 IMU App 的模块边界、命令、事件、快照、诊断字段
4. 规划 IMU 姿态解算算法、滤波算法、姿态角输出和调试方式
5. 明确当前阶段 IMU 高频数据先走 EVENT，上位机后续支持 3D 姿态显示
6. 减少后续开发时重复阅读通信和诊断源码的成本
7. 作为后续让 AI 或开发者快速理解框架的上下文文档
```

后续开发 IMU App 时，优先参考本文档。只有当本文档无法覆盖某个接口细节时，再重新查看对应源码。

---

## 2. 当前项目阶段状态

当前项目整体规划：

```text
Stage 0  Repository and Documentation       Done
Stage 1  Board Bring-up                     Done
Stage 2  UART Reliable Protocol             Mostly Done
Stage 3  Diagnostic Framework               V1 Done
Stage 4  IMU and Algorithm Loop             Next
Stage 5  FDCAN Communication                Not Started
Stage 6  Parameter and Flash System         Not Started
Stage 7  Bootloader and Upgrade             Not Started
Stage 8  Security                           Not Started
Stage 9  Low Power and Hardware Diagnostics Not Started
Stage 10 Chaos and Automated Test           Not Started
```

Stage 4 的核心目标：

```text
1. 建立 IMU Driver / IMU App / Algorithm Loop 的基础结构
2. 实现 IMU 数据采集、状态管理、错误统计和基础滤波入口
3. 实现姿态角解算算法的接口与第一版算法闭环
4. 将 IMU 原始数据、滤波数据、姿态角数据通过现有 UART 协议提供给 PC 查询
5. 将 IMU 高频数据第一版通过 EVENT 上报 PC
6. 将 IMU 关键状态和异常通过 EVENT 主动上报
7. 将 IMU 状态接入 DiagnosticApp 的健康、错误、buffer、timing、pipeline 视图
8. 为后续 Stream 通道和 3D 姿态 UI 做接口准备
```

---

## 3. 当前通信框架总览

当前 PC 与 MCU 通信基于 UART Reliable Protocol。

主链路：

```text
PC Tool / UI
  ↓
UART
  ↓
PlatformUart DMA RX
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
App Command Handler
  ↓
CommandManagerResponse
  ↓
ProtocolManager Send RESP / NACK / EVENT
```

当前已经实现的协议帧类型：

```text
REQ    PC -> MCU 请求
RESP   MCU -> PC 正常响应
NACK   MCU -> PC 错误响应
EVENT  MCU -> PC 主动事件
```

当前通信模型分为五类：

```text
CMD       PC -> MCU，请求、查询、配置、控制
EVENT     MCU -> PC，离散事件或低/中频主动上报
SNAPSHOT  MCU 内部保存最近状态，PC 通过 CMD 查询
STREAM    MCU -> PC，高频实时数据流，后续扩展
BULK      大块可靠传输，后续扩展
```

Stage 4 的 IMU App 第一版应主要使用：

```text
CMD
EVENT
SNAPSHOT
DIAGNOSTIC STATS
```

其中 **IMU 高频数据第一版先走 EVENT**，等后续协议层扩展 Stream 通道后，再将持续高频姿态流迁移到 Stream。

---

## 4. 当前命令系统架构

当前命令链路：

```text
ProtocolManager
  ↓
CommandManager
  ↓
CommandService
  ↓
App handler
```

### 4.1 CommandService

`CommandService` 是统一命令注册表。

它负责：

```text
1. 注册命令 cmd -> handler
2. 根据命令号查找 handler
3. 统计注册数量、分发次数、未知命令、handler 错误
4. 调用具体 App handler
5. 设置 RESP / NACK
```

核心接口概念：

```c
CommandService_Init();
CommandService_Register(cmd, category, flags, handler, ctx, name);
CommandService_Dispatch(req_frame, resp);
CommandService_GetStats();
CommandService_ResetStats();
CommandService_SetResp(resp, cmd, payload, payload_len);
CommandService_SetNack(resp, cmd, error_code);
CommandService_GetCategoryByCmd(cmd);
```

### 4.2 McuInfoApp_RegisterCommand

当前约定是：**App 不直接绕过系统发帧，也不直接操作 ProtocolManager。**

如果某个 App 需要暴露 PC 命令，推荐通过：

```c
McuInfoApp_RegisterCommand(cmd, handler, ctx, name, flags);
```

这会间接注册到 `CommandService`。

Stage 4 的 IMU App 也应遵守这个约定：

```text
IMU App 内部实现 handler
IMU App 初始化时通过 McuInfoApp_RegisterCommand() 注册命令
CommandService 负责统一路由
ProtocolManager 负责统一收发帧
```

---

## 5. 命令号规划

当前命令域规划：

```text
0x01 ~ 0x1F  System / McuInfo
0x20 ~ 0x2F  Diagnostic
0x30 ~ 0x3F  IMU
0x40 ~ 0x4F  FDCAN
0x50 ~ 0x5F  Parameter
0x60 ~ 0x6F  Bootloader
0x70 ~ 0x7F  Security
0x80 ~ 0x8F  Power
0x90 ~ 0x9F  Chaos
0xF0 ~ 0xFF  Debug
```

Stage 4 IMU App 命令建议使用：

```text
0x30 ~ 0x3F
```

推荐 IMU 命令表：

```c
typedef enum
{
    IMU_CMD_GET_STATUS        = 0x30U,
    IMU_CMD_GET_RAW           = 0x31U,
    IMU_CMD_GET_SCALED        = 0x32U,
    IMU_CMD_GET_FILTERED      = 0x33U,
    IMU_CMD_GET_STATS         = 0x34U,
    IMU_CMD_GET_CONFIG        = 0x35U,
    IMU_CMD_SET_CONFIG        = 0x36U,
    IMU_CMD_START             = 0x37U,
    IMU_CMD_STOP              = 0x38U,
    IMU_CMD_CALIBRATE         = 0x39U,
    IMU_CMD_CLEAR_STATS       = 0x3AU,
    IMU_CMD_GET_ATTITUDE      = 0x3BU,
    IMU_CMD_START_EVENT_STREAM = 0x3CU,
    IMU_CMD_STOP_EVENT_STREAM  = 0x3DU
} ImuCommandId_t;
```

Stage 4 V1 建议优先实现：

```text
0x30 IMU_CMD_GET_STATUS
0x31 IMU_CMD_GET_RAW
0x34 IMU_CMD_GET_STATS
0x37 IMU_CMD_START
0x38 IMU_CMD_STOP
0x3B IMU_CMD_GET_ATTITUDE
0x3C IMU_CMD_START_EVENT_STREAM
0x3D IMU_CMD_STOP_EVENT_STREAM
```

如果暂时还没有真实 IMU 硬件驱动，可以先实现模拟数据或空状态返回。

---

## 6. 当前 Diagnostic 命令集

Stage 3 当前诊断命令集：

```text
0x20 GET_HEALTH
0x21 GET_ERROR_COUNTERS
0x22 GET_BUFFER_STATS
0x23 GET_TIMING_STATS
0x24 GET_LAST_RECORDS
0x25 GET_PIPELINE_STATS
0x26 CLEAR_COUNTERS
```

当前典型 payload：

```text
GET_HEALTH:
health=OK,uptime=125430,err=0,drop=0,rx_ovf=0,last=0x00

GET_ERROR_COUNTERS:
unknown=1,handler=0,cmd_err=0,mcu_err=0,parser=0,tx=0,uart=0,ovf=0

GET_BUFFER_STATS:
uart_avail=0,uart_high=180,uart_ovf=0,mcu_drop=0,cmd_drop=0

GET_TIMING_STATS:
loop=1583765,max_gap=28,proto_us=16518,cmd_us=720,svc_us=686,mcu_us=113,diag_us=6

GET_LAST_RECORDS:
last_cmd=0x0D,last_evt=0x85,last_err=0x00,last_rx=0x0D,last_tx=0x0D

GET_PIPELINE_STATS:
rx=10240,parser_ok=120,proto_rx=120,req=80,resp=78,nack=2,cmd=80,unk=1,herr=0,event=40

CLEAR_COUNTERS:
cleared=1
```

Stage 4 的 IMU App 应继续复用这些诊断视图。

如果后续加入 IMU，建议逐步扩展 DiagnosticApp 聚合：

```text
GET_HEALTH:
增加 imu_state / imu_err / imu_stream / imu_alive

GET_ERROR_COUNTERS:
增加 imu_err / imu_timeout / imu_bus_err / imu_invalid_sample / imu_algo_err

GET_BUFFER_STATS:
增加 imu_evt_drop / imu_sample_drop / imu_stream_drop

GET_TIMING_STATS:
增加 imu_us / imu_read_us / algo_us / attitude_us

GET_LAST_RECORDS:
增加 last_imu_event / last_imu_error / last_attitude_tick

GET_PIPELINE_STATS:
可选增加 imu_sample / imu_drop / imu_evt / imu_att
```

第一版 IMU App 不必立即修改所有诊断命令。可以先让 IMU 自己提供 `IMU_CMD_GET_STATS`，后续再让 DiagnosticApp 聚合它。

---

## 7. 当前 McuInfoApp 能力

`McuInfoApp` 当前是系统信息交互中心。

它的职责：

```text
1. 注册系统基础命令
2. 作为 App 命令挂载入口
3. 保存 snapshot
4. 管理 event queue
5. 将 event 转发给 CommandManager
6. 提供 reset / uart / app / command stats 查询
```

IMU App 后续应使用的能力：

### 7.1 命令注册

```c
McuInfoApp_RegisterCommand(cmd, handler, ctx, name, flags);
```

### 7.2 事件上报

```c
McuInfoApp_PostEvent(app_id, event_id, payload, payload_len);
```

### 7.3 快照更新

```c
McuInfoApp_UpdateSnapshot(snapshot_id, snapshot_data, snapshot_len);
McuInfoApp_GetSnapshot(snapshot_id, out_buf, out_buf_size, out_len);
```

Stage 4 建议新增 IMU snapshot id：

```c
typedef enum
{
    MCU_INFO_SNAPSHOT_IMU_STATUS   = 0x10U,
    MCU_INFO_SNAPSHOT_IMU_RAW      = 0x11U,
    MCU_INFO_SNAPSHOT_IMU_STATS    = 0x12U,
    MCU_INFO_SNAPSHOT_IMU_ATTITUDE = 0x13U
} ImuSnapshotId_t;
```

或者统一放在 `mcu_info_app.h` 的 snapshot enum 中。

---

## 8. 当前 EVENT 机制与 IMU 高频数据策略

当前 EVENT 链路：

```text
App
  ↓
McuInfoApp_PostEvent()
  ↓
McuInfoApp Event RingBuffer
  ↓
McuInfoApp_Run()
  ↓
CommandManager_PostEvent()
  ↓
CommandManager Event RingBuffer
  ↓
ProtocolManager_ProcessPendingEvents()
  ↓
UART EVENT frame
  ↓
PC UI / Python event callback
```

关键原则：

```text
App 不直接调用 ProtocolManager 发 EVENT
App 只调用 McuInfoApp_PostEvent()
ProtocolManager 是唯一帧发送者
```

### 8.1 当前阶段 IMU 高频数据先走 EVENT

虽然 EVENT 原本更适合离散事件，但 Stage 4 第一版为了快速闭环，可以先让 IMU 高频或中频数据通过 EVENT 上报 PC。

原因：

```text
1. 当前系统已经有 EVENT 链路
2. 上位机已经能接收和显示 EVENT
3. 可以快速验证 IMU 数据采集、姿态解算和 PC 显示链路
4. 后续 Stream 通道成熟后，再迁移到 Stream
```

但必须明确限制：

```text
1. EVENT 上报频率必须可配置
2. 默认频率不能太高
3. 不建议每个 IMU sample 都 EVENT 上报
4. 115200 波特率下 ASCII payload 很容易吃满 UART
5. 事件队列满时允许 drop，但必须统计 drop_count
```

建议第一版：

```text
IMU 内部采样频率：100 Hz 或根据硬件能力设置
姿态算法更新频率：50 Hz ~ 100 Hz
EVENT 上报频率：10 Hz ~ 20 Hz
PC 查询命令：任意时刻可查询最近一次 raw / attitude / stats
```

### 8.2 后续迁移到 STREAM

后续当协议扩展 `STREAM` 后，IMU 高频数据应迁移到 Stream：

```text
EVENT:
  STARTED / STOPPED / ERROR / CALIB_DONE / WARNING

STREAM:
  raw sample stream
  scaled sample stream
  attitude stream
  quaternion stream
```

Stream 模式建议支持：

```text
1. 固定频率
2. 二进制 payload
3. sequence number
4. drop counter
5. PC 端解码和绘图
6. 3D UI 姿态显示
```

---

## 9. IMU Event ID 规划

推荐 IMU Event ID：

```c
typedef enum
{
    IMU_EVENT_STARTED          = 0x90U,
    IMU_EVENT_STOPPED          = 0x91U,
    IMU_EVENT_SAMPLE_READY     = 0x92U,
    IMU_EVENT_DATA_LOST        = 0x93U,
    IMU_EVENT_CALIB_DONE       = 0x94U,
    IMU_EVENT_ERROR            = 0x95U,
    IMU_EVENT_HEALTH_WARNING   = 0x96U,
    IMU_EVENT_ATTITUDE         = 0x97U,
    IMU_EVENT_RAW_SAMPLE       = 0x98U
} ImuEventId_t;
```

第一版建议使用：

```text
IMU_EVENT_STARTED
IMU_EVENT_STOPPED
IMU_EVENT_ERROR
IMU_EVENT_ATTITUDE
IMU_EVENT_RAW_SAMPLE
```

其中：

```text
IMU_EVENT_ATTITUDE    低/中频上报姿态角或四元数
IMU_EVENT_RAW_SAMPLE  可选，调试阶段低频上报原始数据
```

### 9.1 EVENT payload 建议

第一版使用 ASCII payload，便于调试：

```text
IMU_EVENT_ATTITUDE:
att,t=123456,roll=1.23,pitch=-0.45,yaw=23.4,q0=0.99,q1=0.01,q2=-0.02,q3=0.03

IMU_EVENT_RAW_SAMPLE:
raw,t=123456,ax=123,ay=-22,az=16384,gx=1,gy=-3,gz=2,temp=2410

IMU_EVENT_ERROR:
err,t=123456,code=2,bus=1,read=18
```

注意：

```text
1. ASCII payload 会比较长，115200 下要控制频率
2. 如果 payload 超过 EVENT 最大长度，需要缩短字段名或拆分
3. 后续 Stream 应改用 binary payload
```

---

## 10. Stage 4 IMU App 分层建议

建议采用如下分层：

```text
Platform / BSP Layer
  ↓
IMU Driver Layer
  ↓
IMU App
  ↓
Attitude Algorithm Layer
  ↓
McuInfoApp / DiagnosticApp / CommandService
```

### 10.1 Platform / BSP Layer

负责底层硬件接口：

```text
I2C / SPI
GPIO interrupt
DMA
delay
timestamp
chip select
```

如果目前 CubeMX 已经生成 I2C/SPI，可以先做轻封装。

### 10.2 IMU Driver Layer

负责具体 IMU 芯片寄存器交互。

建议文件：

```text
firmware/app/Drivers/imu_driver.h
firmware/app/Drivers/imu_driver.c
```

职责：

```text
1. Probe / WhoAmI
2. Init registers
3. Read raw accel / gyro / temp
4. Convert raw to physical units
5. Detect bus error / data not ready
```

不建议 Driver 层知道 CommandService、McuInfoApp、DiagnosticApp。

### 10.3 IMU App Layer

建议文件：

```text
firmware/app/Apps/imu_app.h
firmware/app/Apps/imu_app.c
```

职责：

```text
1. 管理 IMU 状态机
2. 周期采样
3. 保存最近 raw / scaled / filtered / attitude 数据
4. 管理 IMU stats
5. 处理 PC 命令
6. 通过 McuInfoApp PostEvent
7. 更新 IMU snapshot
8. 给 DiagnosticApp 提供 stats 接口
9. 调用姿态解算算法
```

### 10.4 Attitude Algorithm Layer

建议新增：

```text
firmware/app/Algorithms/attitude_estimator.h
firmware/app/Algorithms/attitude_estimator.c
```

可选再细分：

```text
firmware/app/Algorithms/imu_filter.h/.c
firmware/app/Algorithms/mahony_filter.h/.c
firmware/app/Algorithms/madgwick_filter.h/.c
```

第一版可以先用一个 `attitude_estimator` 封装算法接口，内部先实现简单算法，后续再替换。

---

## 11. IMU App 状态机设计

IMU App 状态机应复用当前工程已有的 **通用 StateMachine 模块**，而不是自己写一套新的状态机框架。

推荐状态：

```c
typedef enum
{
    IMU_APP_STATE_UNINIT = 0,
    IMU_APP_STATE_IDLE,
    IMU_APP_STATE_PROBING,
    IMU_APP_STATE_READY,
    IMU_APP_STATE_RUNNING,
    IMU_APP_STATE_CALIBRATING,
    IMU_APP_STATE_ERROR
} ImuAppState_t;
```

状态含义：

```text
UNINIT       未初始化
IDLE         已初始化但未启动采样
PROBING      正在检测 IMU 芯片
READY        芯片存在，配置完成
RUNNING      正在周期采样和姿态解算
CALIBRATING  正在校准陀螺仪/加速度计零偏
ERROR        发生不可忽略错误
```

推荐事件：

```c
typedef enum
{
    IMU_APP_EVT_INIT = 1,
    IMU_APP_EVT_PROBE_OK,
    IMU_APP_EVT_PROBE_FAIL,
    IMU_APP_EVT_START,
    IMU_APP_EVT_STOP,
    IMU_APP_EVT_SAMPLE_TICK,
    IMU_APP_EVT_CALIB_START,
    IMU_APP_EVT_CALIB_DONE,
    IMU_APP_EVT_ERROR
} ImuAppEvent_t;
```

状态机上下文建议：

```c
typedef struct
{
    StateMachine_t sm;
    uint8_t state;
    uint8_t previous_state;

    uint32_t transition_count;
    uint32_t state_enter_tick_ms;
    uint32_t state_duration_ms;

    uint32_t enter_idle_count;
    uint32_t enter_ready_count;
    uint32_t enter_running_count;
    uint32_t enter_calibrating_count;
    uint32_t enter_error_count;
} ImuAppStateMachineContext_t;
```

状态迁移建议：

```text
UNINIT -> IDLE
IDLE -> PROBING
PROBING -> READY
PROBING -> ERROR
READY -> RUNNING
RUNNING -> READY
RUNNING -> ERROR
READY -> CALIBRATING
CALIBRATING -> READY
CALIBRATING -> ERROR
ERROR -> PROBING / IDLE
```

---

## 12. 姿态解算算法规划

Stage 4 不能只采集 IMU 数据，还要为后续算法闭环建立接口。

IMU 姿态解算通常输出：

```text
roll   横滚角
pitch  俯仰角
yaw    航向角
quaternion 四元数 q0 q1 q2 q3
```

需要注意：

```text
1. 仅 6 轴 IMU（acc + gyro）无法长期稳定观测绝对 yaw
2. yaw 主要来自陀螺仪积分，会随时间漂移
3. 如果有磁力计，可用 9 轴算法修正 yaw
4. roll / pitch 可利用重力方向通过加速度计修正
5. 姿态角算法必须依赖稳定的 dt
```

### 12.1 Stage 4 V1 推荐算法路线

第一版建议采用分阶段算法：

```text
阶段 A：加速度计倾角解算
  用 ax/ay/az 计算 roll_acc / pitch_acc

阶段 B：陀螺仪积分
  用 gx/gy/gz 和 dt 积分 roll_gyro / pitch_gyro / yaw_gyro

阶段 C：互补滤波 Complementary Filter
  roll  = alpha * gyro_roll  + (1-alpha) * acc_roll
  pitch = alpha * gyro_pitch + (1-alpha) * acc_pitch
  yaw   = gyro yaw 积分值，暂时不做绝对修正

阶段 D：四元数接口预留
  后续可切换 Mahony / Madgwick
```

V1 推荐原因：

```text
1. 实现简单
2. 易于调试
3. 对本科/工程验证足够直观
4. 上位机可以立刻显示 roll / pitch / yaw
5. 后续能平滑替换 Mahony / Madgwick
```

### 12.2 后续算法路线

后续可扩展：

```text
Mahony Filter:
  基于 PI 反馈修正陀螺仪漂移，计算量较低，嵌入式常用

Madgwick Filter:
  基于梯度下降，收敛快，适合 6 轴/9 轴姿态融合

EKF / UKF:
  更复杂，适合高精度导航，不建议当前阶段直接上
```

当前建议：

```text
V1: Complementary Filter
V2: Mahony
V3: Madgwick 或根据场景决定
```

### 12.3 姿态算法模块接口

建议新增：

```c
typedef enum
{
    ATTITUDE_ESTIMATOR_OK = 0,
    ATTITUDE_ESTIMATOR_ERROR = -1,
    ATTITUDE_ESTIMATOR_INVALID_PARAM = -2
} AttitudeEstimatorResult_t;

typedef struct
{
    float roll_deg;
    float pitch_deg;
    float yaw_deg;

    float q0;
    float q1;
    float q2;
    float q3;

    uint32_t update_count;
    uint32_t last_update_tick_ms;
    uint32_t last_dt_us;
    uint32_t max_dt_us;
    uint32_t last_update_us;
    uint32_t max_update_us;

    uint8_t initialized;
    uint8_t last_error;
} AttitudeEstimatorState_t;

void AttitudeEstimator_Init(void);
void AttitudeEstimator_Reset(void);

int AttitudeEstimator_Update6Axis(float ax_g,
                                  float ay_g,
                                  float az_g,
                                  float gx_dps,
                                  float gy_dps,
                                  float gz_dps,
                                  float dt_s);

const AttitudeEstimatorState_t *AttitudeEstimator_GetState(void);
```

### 12.4 姿态输出 payload

CMD 查询：

```text
IMU_CMD_GET_ATTITUDE:
roll=1.23,pitch=-0.45,yaw=23.40,q0=0.999,q1=0.010,q2=-0.020,q3=0.030,tick=123456
```

EVENT 上报：

```text
IMU_EVENT_ATTITUDE:
att,t=123456,r=1.23,p=-0.45,y=23.40
```

为了降低 EVENT payload 长度，EVENT 第一版可以只发：

```text
att,t=123456,r=1.23,p=-0.45,y=23.40
```

四元数可通过 CMD 查询或后续 Stream 发送。

---

## 13. IMU App 数据结构建议

### 13.1 原始数据

```c
typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t temp;
    uint32_t tick_ms;
} ImuRawSample_t;
```

### 13.2 换算后数据

```c
typedef struct
{
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float temp_c;
    uint32_t tick_ms;
} ImuScaledSample_t;
```

### 13.3 滤波后数据

```c
typedef struct
{
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    uint32_t tick_ms;
} ImuFilteredSample_t;
```

### 13.4 姿态数据

```c
typedef struct
{
    float roll_deg;
    float pitch_deg;
    float yaw_deg;

    float q0;
    float q1;
    float q2;
    float q3;

    uint32_t tick_ms;
} ImuAttitude_t;
```

### 13.5 统计信息

```c
typedef struct
{
    uint32_t init_count;
    uint32_t run_count;
    uint32_t start_count;
    uint32_t stop_count;

    uint32_t sample_count;
    uint32_t sample_drop_count;
    uint32_t read_error_count;
    uint32_t bus_error_count;
    uint32_t invalid_sample_count;
    uint32_t calibrate_count;

    uint32_t attitude_update_count;
    uint32_t attitude_error_count;

    uint32_t event_stream_start_count;
    uint32_t event_stream_stop_count;
    uint32_t event_stream_send_count;
    uint32_t event_stream_drop_count;

    uint32_t last_sample_tick_ms;
    uint32_t last_sample_interval_ms;
    uint32_t max_sample_interval_ms;

    uint32_t last_run_us;
    uint32_t max_run_us;
    uint32_t last_read_us;
    uint32_t max_read_us;
    uint32_t last_algo_us;
    uint32_t max_algo_us;

    uint8_t state;
    uint8_t last_error;
    uint8_t last_event;
} ImuAppStats_t;
```

---

## 14. IMU App API 建议

```c
void ImuApp_Init(void);
void ImuApp_Run(void);

int ImuApp_Start(void);
int ImuApp_Stop(void);
int ImuApp_Calibrate(void);

int ImuApp_StartEventStream(void);
int ImuApp_StopEventStream(void);

const ImuAppStats_t *ImuApp_GetStats(void);
const ImuRawSample_t *ImuApp_GetRawSample(void);
const ImuScaledSample_t *ImuApp_GetScaledSample(void);
const ImuFilteredSample_t *ImuApp_GetFilteredSample(void);
const ImuAttitude_t *ImuApp_GetAttitude(void);

void ImuApp_ResetStats(void);
void ImuApp_PrintStats(void);
```

如果 DiagnosticApp 要聚合 IMU 状态，最重要的是：

```c
const ImuAppStats_t *ImuApp_GetStats(void);
```

---

## 15. IMU 命令 handler 设计

所有 handler 建议写在 `imu_app.c` 内部。

推荐形式：

```c
static int ImuApp_HandleGetStatus(const ProtocolFrame_t *req,
                                  CommandManagerResponse_t *resp,
                                  void *ctx);

static int ImuApp_HandleGetRaw(const ProtocolFrame_t *req,
                               CommandManagerResponse_t *resp,
                               void *ctx);

static int ImuApp_HandleGetStats(const ProtocolFrame_t *req,
                                 CommandManagerResponse_t *resp,
                                 void *ctx);

static int ImuApp_HandleGetAttitude(const ProtocolFrame_t *req,
                                    CommandManagerResponse_t *resp,
                                    void *ctx);

static int ImuApp_HandleStart(const ProtocolFrame_t *req,
                              CommandManagerResponse_t *resp,
                              void *ctx);

static int ImuApp_HandleStop(const ProtocolFrame_t *req,
                             CommandManagerResponse_t *resp,
                             void *ctx);

static int ImuApp_HandleStartEventStream(const ProtocolFrame_t *req,
                                         CommandManagerResponse_t *resp,
                                         void *ctx);

static int ImuApp_HandleStopEventStream(const ProtocolFrame_t *req,
                                        CommandManagerResponse_t *resp,
                                        void *ctx);
```

---

## 16. IMU App payload 建议

第一版仍然使用 ASCII payload，保持和 Stage 2 / Stage 3 一致。

### 16.1 IMU_GET_STATUS

```text
state=RUNNING,started=1,stream=1,err=0,sample=12345,period=10
```

### 16.2 IMU_GET_RAW

```text
ax=123,ay=-22,az=16384,gx=1,gy=-3,gz=2,temp=2410,tick=123456
```

### 16.3 IMU_GET_STATS

```text
run=123456,sample=12345,drop=0,read_err=0,bus_err=0,att=12345,evt=120,max_itv=11,read_us=52,algo_us=18,run_us=8
```

### 16.4 IMU_GET_ATTITUDE

```text
roll=1.23,pitch=-0.45,yaw=23.40,q0=0.999,q1=0.010,q2=-0.020,q3=0.030,tick=123456
```

### 16.5 IMU_START

```text
started=1
```

### 16.6 IMU_STOP

```text
started=0
```

### 16.7 IMU_START_EVENT_STREAM

```text
stream=1,rate=20
```

### 16.8 IMU_STOP_EVENT_STREAM

```text
stream=0
```

---

## 17. IMU App 与 DiagnosticApp 的关系

第一版建议：

```text
IMU App 自己提供 IMU_GET_STATS
DiagnosticApp 暂时不直接聚合 IMU stats
```

第二版再把 IMU 接入 DiagnosticApp：

```text
GET_HEALTH      增加 imu_state / imu_err
GET_ERROR       增加 imu_read_err / imu_bus_err / imu_algo_err
GET_TIMING      增加 imu_us / imu_read_us / algo_us
GET_LAST        增加 last_imu_event / last_imu_err
```

这样不会一次性把 Stage 4 和 Stage 3 耦合过重。

---

## 18. IMU App 与 Timing 诊断

当前项目已经接入 DWT profiling：

```c
PlatformTime_ProfileStart();
PlatformTime_ProfileEndUs(start_cycle);
```

IMU App 应该记录：

```text
ImuApp_Run() last/max us
IMU driver read last/max us
Algorithm update last/max us
AttitudeEstimator_Update6Axis() last/max us
```

建议在 `ImuApp_Run()` 中：

```c
void ImuApp_Run(void)
{
    uint32_t start_cycle;
    uint32_t elapsed_us;

    start_cycle = PlatformTime_ProfileStart();

    /* sampling / state update / algorithm update / event stream */

    elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
    g_imu_app.stats.last_run_us = elapsed_us;

    if (elapsed_us > g_imu_app.stats.max_run_us)
    {
        g_imu_app.stats.max_run_us = elapsed_us;
    }
}
```

在 driver read 和算法更新周围分别记录：

```c
start_cycle = PlatformTime_ProfileStart();
ret = ImuDriver_ReadRaw(&raw);
elapsed_us = PlatformTime_ProfileEndUs(start_cycle);

start_cycle = PlatformTime_ProfileStart();
ret = AttitudeEstimator_Update6Axis(...);
elapsed_us = PlatformTime_ProfileEndUs(start_cycle);
```

---

## 19. IMU App 与 Snapshot

IMU App 可以周期性更新 snapshot，但不要每次采样都更新太大结构。

建议：

```text
低频更新 snapshot，例如 100ms 或 500ms
PC 查询 IMU_GET_RAW / IMU_GET_ATTITUDE 时直接读当前缓存
DiagnosticApp 查询时读 stats
```

推荐 snapshot：

```text
IMU_STATUS_SNAPSHOT
IMU_RAW_SNAPSHOT
IMU_ATTITUDE_SNAPSHOT
IMU_STATS_SNAPSHOT
```

第一版可以不强制使用 snapshot，直接从 `g_imu_app` 上下文返回即可。

---

## 20. IMU App 与当前 115200 波特率限制

当前 UART 为 115200 bps 时：

```text
实际约 11520 byte/s
1 byte 约 86.8 us
```

如果使用阻塞发送，则：

```text
发送 100 byte 约 8.68 ms
发送 128 byte 约 11.1 ms
```

所以 Stage 4 第一版即使走 EVENT，也要限制频率。

推荐策略：

```text
1. PC 主动查询 IMU_GET_RAW / IMU_GET_ATTITUDE
2. IMU EVENT 上报姿态时默认 10 Hz ~ 20 Hz
3. IMU EVENT payload 尽量短
4. IMU 统计通过 IMU_GET_STATS 查询
5. 高频数据流等 TX DMA / STREAM 机制成熟后再做
```

### 20.1 EVENT 频率粗略估算

如果 EVENT payload 约 60 byte，加上协议帧头和 CRC 约 70~80 byte。

在 115200 bps 下：

```text
80 byte ≈ 6.94 ms 发送时间
20 Hz ≈ 1600 byte/s，约占 13.9% 串口带宽
50 Hz ≈ 4000 byte/s，约占 34.7% 串口带宽
100 Hz ≈ 8000 byte/s，约占 69.4% 串口带宽
```

所以第一版建议：

```text
默认 10 Hz
调试最高 20 Hz
不建议 50 Hz 以上 ASCII EVENT
```

---

## 21. 后续 3D UI 姿态显示规划

上位机后续可以增加 IMU / Attitude 页面。

建议 UI 功能：

```text
1. IMU 控制按钮
   - IMU_STATUS
   - IMU_START
   - IMU_STOP
   - IMU_RAW
   - IMU_ATTITUDE
   - START_EVENT_STREAM
   - STOP_EVENT_STREAM

2. 实时曲线
   - ax / ay / az
   - gx / gy / gz
   - roll / pitch / yaw

3. 3D 姿态显示
   - 根据 roll / pitch / yaw 或 quaternion 更新 3D 模型
   - 第一版可以用简单 cube / board 模型
   - 后续可以切换为设备模型

4. Event Monitor
   - 显示 IMU_EVENT_ATTITUDE
   - 显示 IMU_EVENT_ERROR
   - 显示丢包、延迟和更新时间戳
```

当前 UI 第一版可以先只显示文本和曲线，不急着做 3D。3D UI 可以作为 Stage 4.5 或 Stage 4.6。

---

## 22. IMU App 初始化顺序

当前 `app_main.c` 已经包含：

```text
CommandService_Init()
McuInfoApp_Init()
DiagnosticApp_Init()
CommandManager_Init()
ProtocolManager_Init()
```

Stage 4 加入 IMU App 后，推荐顺序：

```text
CommandService_Init()

McuInfoApp_Init()
McuInfoApp_UpdateResetSnapshot()

DiagnosticApp_Init()

ImuApp_Init()

CommandManager_Init()
ProtocolManager_Init()
```

如果 DiagnosticApp 初始化时需要读取 IMU stats，则应让 `ImuApp_Init()` 早于 `DiagnosticApp_Init()`。

第一版推荐：

```text
McuInfoApp_Init()
DiagnosticApp_Init()
ImuApp_Init()
```

因为 DiagnosticApp 暂时不聚合 IMU stats，IMU App 只需要通过 McuInfoApp 注册自己的命令。

主循环推荐：

```c
while (1)
{
    ImuApp_Run();
    DiagnosticApp_Run();
    McuInfoApp_Run();
    ProtocolManager_Process();
}
```

如果 IMU 采样周期比较严格，建议把 `ImuApp_Run()` 放在 `DiagnosticApp_Run()` 前。

---

## 23. Stage 4 V1 开发顺序

推荐开发顺序：

```text
Step 4.1  建立 imu_app.h/.c 骨架
Step 4.2  定义 IMU 命令号 0x30~0x3D
Step 4.3  实现通用 StateMachine 接入
Step 4.4  实现 ImuApp_Init / Run / GetStats / ResetStats
Step 4.5  实现 IMU_GET_STATUS
Step 4.6  实现 IMU_START / IMU_STOP
Step 4.7  实现模拟 IMU raw 数据或接入真实 driver
Step 4.8  实现 Complementary Filter 姿态解算
Step 4.9  实现 IMU_GET_RAW / IMU_GET_ATTITUDE / IMU_GET_STATS
Step 4.10 实现 IMU_EVENT_ATTITUDE 低频上报
Step 4.11 实现 START_EVENT_STREAM / STOP_EVENT_STREAM
Step 4.12 上位机 constants.py / main_window.py / proto_imu_test.py
Step 4.13 回归测试 Stage 2 / Stage 3 / Stage 4 命令
```

推荐第一天目标：

```text
不用真实 IMU 硬件，先用模拟数据跑通：
IMU_STATUS
IMU_START
IMU_STOP
IMU_RAW
IMU_ATTITUDE
IMU_STATS
IMU_EVENT_ATTITUDE
```

---

## 24. Stage 4 V1 验收标准

Stage 4 V1 最小完成标准：

```text
1. ImuApp 可以初始化
2. ImuApp 可以周期 Run
3. ImuApp 状态机复用通用 StateMachine 模块
4. IMU_CMD_GET_STATUS 返回状态
5. IMU_CMD_START / STOP 可以控制 started 状态
6. IMU_CMD_GET_RAW 可以返回模拟或真实 raw 数据
7. IMU_CMD_GET_ATTITUDE 可以返回 roll / pitch / yaw / quaternion
8. IMU_CMD_GET_STATS 可以返回 run/sample/drop/error/timing 统计
9. IMU App 通过 McuInfoApp_RegisterCommand() 接入 CommandService
10. IMU App 不直接调用 ProtocolManager
11. IMU App 可以通过 McuInfoApp_PostEvent() 上报 STARTED / STOPPED / ERROR / ATTITUDE
12. IMU 高频或中频数据第一版可以通过 EVENT 上报 PC
13. 上位机可以通过 Python 脚本测试 IMU 命令
14. 原有 Stage 2 / Stage 3 测试仍然通过
```

---

## 25. 之后开发时最常用的现有接口清单

### 25.1 时间与性能

```c
PlatformTime_GetMs();
PlatformTime_ProfileStart();
PlatformTime_ProfileEndUs(start_cycle);
```

### 25.2 命令注册

```c
McuInfoApp_RegisterCommand(cmd, handler, ctx, name, flags);
```

### 25.3 返回 RESP / NACK

```c
CommandService_SetResp(resp, cmd, payload, payload_len);
CommandService_SetNack(resp, cmd, error_code);
```

### 25.4 事件上报

```c
McuInfoApp_PostEvent(app_id, event_id, payload, payload_len);
```

### 25.5 快照

```c
McuInfoApp_UpdateSnapshot(snapshot_id, snapshot_data, snapshot_len);
McuInfoApp_GetSnapshot(snapshot_id, out_buf, out_buf_size, out_len);
```

### 25.6 统计读取

```c
CommandService_GetStats();
CommandManager_GetStats();
ProtocolManager_GetStats();
McuInfoApp_GetStats();
DiagnosticApp_GetStats();
PlatformUart_GetRxSnapshot(&snapshot);
```

### 25.7 清除统计

```c
CommandService_ResetStats();
CommandManager_ResetStats();
ProtocolManager_ResetStats();
McuInfoApp_ResetStats();
DiagnosticApp_ResetStats();
```

---

## 26. 重要设计约束

后续开发 IMU App 时应遵守：

```text
1. IMU App 不直接发 UART
2. IMU App 不直接调用 ProtocolManager
3. IMU App 命令通过 McuInfoApp_RegisterCommand() 挂载
4. IMU App 事件通过 McuInfoApp_PostEvent() 上报
5. IMU 高频数据第一版允许通过 EVENT 上报，但必须限频
6. 所有 PC 查询命令第一版使用 ASCII payload
7. handler 内部不要使用过大的栈数组
8. 所有 snprintf 必须检查返回值和截断
9. 所有 payload_len 必须用真实字符串长度，不得用 sizeof(buffer)
10. 所有状态、错误、drop、timeout 都要进入 stats
11. 所有耗时路径用 DWT profiling 记录 last/max us
12. 状态机必须复用工程已有 StateMachine 模块
13. 不能因为 IMU App 引入而破坏 Stage 2 / Stage 3 命令
14. 姿态解算算法必须保留 dt、error、update_count 等诊断信息
15. 3D UI 所需的 roll / pitch / yaw / quaternion 接口要提前预留
```

---

## 27. 后续如果需要重新提供的文件

如果以后只基于本文档还无法继续开发，再请求以下文件：

```text
1. command_service.h/.c
2. mcu_info_app.h/.c
3. diagnostic_app.h/.c
4. app_main.c
5. platform_time.h/.c
6. platform_uart.h/.c
7. state_machine.h/.c
8. main_window.py
9. constants.py
10. proto_diagnostic_test.py
```

如果要接真实 IMU 硬件，再请求：

```text
1. CubeMX 生成的 i2c.c / spi.c / gpio.c
2. 对应 IMU 芯片手册
3. 当前板卡原理图或 IMU 接线说明
4. IMU 型号、I2C 地址或 SPI 片选引脚
```

---

## 28. 下一步建议

下一次开始 Stage 4 时，可以直接从以下任务开始：

```text
1. 新增 imu_app.h
2. 新增 imu_app.c
3. 新增 attitude_estimator.h
4. 新增 attitude_estimator.c
5. 在 app_main.c 中调用 ImuApp_Init() / ImuApp_Run()
6. 在 command_service.h 中确认 0x30~0x3F IMU 命令域
7. 在 constants.py 中新增 IMU 命令号
8. 新增 proto_imu_test.py
9. UI 中增加 IMU 按钮或 IMU 分组
```

推荐第一天目标：

```text
不用真实 IMU 硬件，先用模拟数据跑通：
IMU_STATUS
IMU_START
IMU_STOP
IMU_RAW
IMU_ATTITUDE
IMU_STATS
IMU_EVENT_ATTITUDE
```

这样可以先验证 IMU App 已经正确接入通信、诊断、状态机和姿态算法框架，再进入真实驱动层。

---

## 29. 一句话总结

Stage 4 的 IMU App 不应该独立成为一个孤岛。

它应该像这样接入当前系统：

```text
ImuDriver 负责读芯片
AttitudeEstimator 负责姿态解算
ImuApp 负责状态机、采样、stats、handler、event
McuInfoApp 负责命令挂载、事件转发、snapshot
CommandService 负责命令路由
ProtocolManager 负责帧收发
DiagnosticApp 负责系统级诊断聚合
PC Tool / UI 负责查询、曲线和 3D 姿态显示
```

第一版先完成：

```text
IMU App 骨架 + 通用状态机 + 模拟数据 + Complementary Filter + CMD 查询 + EVENT 姿态上报 + STATS 统计
```

再接真实 IMU driver、Mahony / Madgwick 算法和 Stream 通道。
