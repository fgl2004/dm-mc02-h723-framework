# Stage 5: External Flash, Storage Manager and File System

## 1. Stage Goal

建立外部 QSPI Flash 存储能力，并在其上构建块设备、存储管理、文件系统和 PC 文件管理界面，为参数持久化、日志记录、IMU 数据记录、Bootloader 固件暂存和后续 OTA 升级提供基础。

本阶段不只是“读写 W25Q64JV”，而是建立一套完整的嵌入式存储子系统：

```text
外部 Flash 驱动
  ↓
块设备抽象
  ↓
StorageManager
  ↓
StorageApp
  ↓
命令接口
  ↓
PC Storage Explorer
```

最终目标是让系统具备以下能力：

```text
1. 能可靠访问外部 QSPI Flash
2. 能抽象为统一块设备
3. 能规划存储分区
4. 能接入小型文件系统
5. 能通过 PC 上位机像文件管理器一样操作 MCU 内部文件
6. 能为参数、日志、IMU 记录、Bootloader、固件升级提供基础
```

---

## 2. Overall Architecture

MCU 端总架构建议如下：

```text
qspi_platform
  ↓
bsp_w25q64jv
  ↓
flash_block_device
  ↓
storage_manager
  ↓
storage_app
  ↓
CommandService / McuInfoApp / DiagnosticApp
  ↓
ProtocolManager
  ↓
PC Storage Explorer
```

各层职责必须保持清晰：

```text
qspi_platform:
  只负责 STM32H7 QSPI 外设访问

bsp_w25q64jv:
  只负责 W25Q64JV 芯片命令、寄存器、页/扇区/块操作

flash_block_device:
  把具体 Flash 芯片抽象成统一块设备

storage_manager:
  管理分区、Raw Storage、文件系统、挂载、格式化、文件读写

storage_app:
  作为业务 App，注册命令、管理状态机、处理 PC 请求、维护统计

PC Storage Explorer:
  提供类 Windows 文件管理器体验
```

---

## 3. qspi_platform Layer

### 3.1 Layer Goal

`qspi_platform` 只管理 STM32H7 的 QSPI 外设。

这一层不应该知道：

```text
1. W25Q64JV
2. Flash 页大小
3. Flash 扇区大小
4. 文件系统
5. StorageManager
6. StorageApp
```

它只负责把 HAL QSPI 能力封装成工程内部统一接口。

### 3.2 Responsibilities

```text
1. QSPI 初始化
2. QSPI Command 发送
3. QSPI 数据发送
4. QSPI 数据接收
5. QSPI AutoPolling
6. QSPI MemoryMapped 模式，后续可选
7. QSPI 错误统计
8. QSPI 超时统计
9. QSPI busy / timeout / error 状态记录
```

### 3.3 Suggested Interfaces

```c
int PlatformQspi_Init(void);

int PlatformQspi_Command(/* command config */);
int PlatformQspi_Transmit(const uint8_t *buf, uint32_t len, uint32_t timeout_ms);
int PlatformQspi_Receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms);
int PlatformQspi_AutoPolling(/* polling config */);

int PlatformQspi_EnableMemoryMapped(void);
int PlatformQspi_DisableMemoryMapped(void);

const PlatformQspiStats_t *PlatformQspi_GetStats(void);
void PlatformQspi_ResetStats(void);
void PlatformQspi_PrintStats(void);
```

### 3.4 Design Notes

这一层类似之前的：

```text
platform_spi
```

只是把 SPI 抽象换成 QSPI 抽象。

第一版不建议马上开启 MemoryMapped 模式，先用标准 Command + Receive + Transmit 流程把 ID、擦除、写入、读回跑通。

---

## 4. bsp_w25q64jv Layer

### 4.1 Layer Goal

`bsp_w25q64jv` 才知道 W25Q64JV 的芯片细节。

这一层负责芯片级指令和寄存器，不负责文件系统，不负责 PC 命令。

### 4.2 Responsibilities

```text
1. Read JEDEC ID
2. Read Unique ID
3. Read Status Register
4. Write Enable
5. Wait Busy
6. Sector Erase
7. Block Erase
8. Chip Erase
9. Page Program
10. Read Data
11. Fast Read
12. Fast Read Quad IO，后续
13. Enter / Exit QPI，后续
```

### 4.3 Suggested Interfaces

```c
int BspW25q64_Init(void);

int BspW25q64_ReadJedecId(uint8_t *manufacturer,
                          uint8_t *memory_type,
                          uint8_t *capacity);

int BspW25q64_ReadUniqueId(uint8_t uid[8]);

int BspW25q64_ReadStatus1(uint8_t *status);
int BspW25q64_ReadStatus2(uint8_t *status);
int BspW25q64_ReadStatus3(uint8_t *status);

int BspW25q64_WriteEnable(void);
int BspW25q64_IsBusy(void);
int BspW25q64_WaitReady(uint32_t timeout_ms);

int BspW25q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);
int BspW25q64_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);

int BspW25q64_SectorErase(uint32_t addr);
int BspW25q64_BlockErase32K(uint32_t addr);
int BspW25q64_BlockErase64K(uint32_t addr);
int BspW25q64_ChipErase(void);

const BspW25q64Stats_t *BspW25q64_GetStats(void);
void BspW25q64_ResetStats(void);
void BspW25q64_PrintStats(void);
```

### 4.4 Important Constraints

W25Q 系列常见约束：

```text
1. 容量：W25Q64JV = 64 Mbit = 8 MByte
2. Page Program 粒度：256 bytes
3. Sector Erase 粒度：4 KB
4. Block Erase 常见粒度：32 KB / 64 KB
5. NOR Flash 写入只能把 bit 从 1 写成 0
6. 如果需要从 0 变回 1，必须先擦除对应扇区
7. PageProgram 不能跨页，上层必须拆分
8. 擦除耗时远大于读取和页编程
9. 写入/擦除前必须 Write Enable
10. 写入/擦除后必须 Wait Busy 直到 ready
```

第一版目标是可靠，不追求最快速度。

---

## 5. flash_block_device Layer

### 5.1 Layer Goal

`flash_block_device` 是从芯片驱动到存储系统的关键抽象层。

它把具体的 W25Q64JV 抽象成统一块设备。

这样后续如果换成：

```text
W25Q128
MX25L
GD25Q
其他 QSPI NOR Flash
```

上层 `StorageManager` 不需要大改。

### 5.2 Responsibilities

```text
1. 统一容量
2. 统一 read size
3. 统一 program size
4. 统一 erase size
5. 提供 read / program / erase / sync 接口
6. 隐藏 W25Q64JV 的 page program 拆分细节
7. 隐藏 sector erase 对齐细节
8. 为 StorageManager 或 LittleFS 提供底层块设备
```

### 5.3 Suggested Interfaces

```c
int FlashBlockDevice_Init(void);

int FlashBlockDevice_Read(uint32_t addr, void *buf, uint32_t len);
int FlashBlockDevice_Program(uint32_t addr, const void *buf, uint32_t len);
int FlashBlockDevice_Erase(uint32_t addr, uint32_t len);
int FlashBlockDevice_Sync(void);

const FlashBlockDeviceInfo_t *FlashBlockDevice_GetInfo(void);
const FlashBlockDeviceStats_t *FlashBlockDevice_GetStats(void);
void FlashBlockDevice_ResetStats(void);
```

### 5.4 Device Information

W25Q64JV 对外可抽象为：

```text
capacity      = 8 MB
read_size     = 1 byte
program_size  = 256 bytes
erase_size    = 4096 bytes
```

---

## 6. StorageManager Layer

### 6.1 Layer Goal

`StorageManager` 是存储能力中心。

它不直接挂 PC 命令，它只提供存储服务。

职责包括：

```text
1. 管理存储分区
2. 管理 Raw Storage
3. 管理 File System
4. 管理 mount / format / unmount
5. 管理文件读写接口
6. 管理存储容量信息
7. 管理存储状态和错误统计
```

### 6.2 Stage A: Raw Storage Manager

第一阶段先不急着上文件系统，先实现 Raw Storage Manager。

目标：

```text
1. 分区表
2. 测试区
3. 参数区
4. 日志区
5. 文件系统预留区
6. 固件暂存区
7. Raw read / program / erase 测试
8. 存储统计
```

### 6.3 Suggested Partition Layout

W25Q64JV 总容量为 8MB，初步分区建议：

```text
0x000000 - 0x00FFFF    Storage metadata
0x010000 - 0x01FFFF    Parameter Slot A
0x020000 - 0x02FFFF    Parameter Slot B
0x030000 - 0x0FFFFF    Persistent Logs
0x100000 - 0x3FFFFF    IMU Recorder
0x400000 - 0x7DFFFF    Firmware Staging
0x7E0000 - 0x7FFFFF    Factory / Reserved
```

分区设计原则：

```text
1. 低地址放 metadata 和参数
2. 中间放日志和记录数据
3. 高地址放固件暂存区
4. 末尾保留 factory / reserved 区域
5. 所有分区边界尽量按 4KB 扇区对齐
```

### 6.4 Stage B: File System Manager

第二阶段接入小型文件系统。

推荐优先考虑：

```text
LittleFS
```

原因：

```text
1. 面向嵌入式 NOR Flash
2. 支持掉电保护
3. 有磨损均衡
4. 比 FATFS 更适合裸 Flash
5. 不需要块设备像 SD 卡那样有 512 字节扇区语义
```

FatFS 更适合：

```text
SD 卡
U 盘
eMMC
标准块设备
```

W25Q64JV 这种裸 NOR Flash 更适合 LittleFS。

### 6.5 Suggested Interfaces

```c
int StorageManager_Init(void);

int StorageManager_Format(void);
int StorageManager_Mount(void);
int StorageManager_Unmount(void);
int StorageManager_GetInfo(StorageInfo_t *info);

int StorageManager_ListDir(const char *path,
                           StorageDirEntry_t *entries,
                           uint32_t max_entries,
                           uint32_t *count);

int StorageManager_ReadFile(const char *path,
                            uint32_t offset,
                            uint8_t *buf,
                            uint32_t len,
                            uint32_t *read_len);

int StorageManager_WriteFile(const char *path,
                             uint32_t offset,
                             const uint8_t *buf,
                             uint32_t len);

int StorageManager_DeleteFile(const char *path);
int StorageManager_Mkdir(const char *path);
int StorageManager_Rename(const char *old_path, const char *new_path);

const StorageManagerStats_t *StorageManager_GetStats(void);
```

---

## 7. StorageApp Layer

### 7.1 Layer Goal

`StorageApp` 是 Storage 子系统的业务 App。

它不做底层驱动，不直接操作 QSPI。

它负责：

```text
1. 管理 StorageManager 生命周期
2. 管理 Storage 状态机
3. 注册 Storage 命令
4. 处理 PC 文件操作请求
5. 维护 Storage 统计信息
6. 上报 Storage 事件
7. 给 DiagnosticApp 暴露存储健康状态
```

类比关系：

```text
ImuApp 管 IMU 业务
StorageApp 管存储业务
McuInfoApp 是信息中枢
CommandService 是命令挂载中心
ProtocolManager 是通信协议执行层
```

### 7.2 StorageApp State Machine

建议状态：

```c
typedef enum
{
    STORAGE_APP_STATE_UNINIT = 0,
    STORAGE_APP_STATE_PROBING,
    STORAGE_APP_STATE_READY,
    STORAGE_APP_STATE_MOUNTED,
    STORAGE_APP_STATE_FORMATTING,
    STORAGE_APP_STATE_BUSY,
    STORAGE_APP_STATE_ERROR
} StorageAppState_t;
```

启动流程：

```text
UNINIT
  ↓ StorageApp_Init()
PROBING
  ↓ 读取 JEDEC ID 成功
READY
  ↓ Mount LittleFS 成功
MOUNTED
```

错误流程：

```text
读 ID 失败       -> ERROR
Mount 失败      -> READY 或 ERROR
Format 失败     -> ERROR
文件操作失败    -> 计数，但不一定进 ERROR
```

注意：

```text
Flash ready 和 File system mounted 是两个不同状态。

文件系统未挂载不代表 Flash 坏了。
Flash 读写正常但 LittleFS 未格式化时，StorageApp 可以停留在 READY 状态。
```

---

## 8. StorageApp Command Domain

建议给 Storage 单独命令域：

```text
0x40 ~ 0x5F
```

### 8.1 Flash / Raw Storage Commands

```text
0x40  STORAGE_GET_INFO
0x41  STORAGE_GET_ID
0x42  STORAGE_GET_STATUS
0x43  STORAGE_SELF_TEST
0x44  STORAGE_ERASE_TEST
0x45  STORAGE_WRITE_READ_TEST
0x46  STORAGE_GET_STATS
0x47  STORAGE_CLEAR_STATS
```

### 8.2 File System Commands

```text
0x50  FS_MOUNT
0x51  FS_FORMAT
0x52  FS_GET_INFO
0x53  FS_LIST
0x54  FS_READ
0x55  FS_WRITE
0x56  FS_DELETE
0x57  FS_MKDIR
0x58  FS_RENAME
0x59  FS_STAT
```

### 8.3 Large File Transfer Commands

```text
0x5A  FS_UPLOAD_BEGIN
0x5B  FS_UPLOAD_CHUNK
0x5C  FS_UPLOAD_END
0x5D  FS_DOWNLOAD_BEGIN
0x5E  FS_DOWNLOAD_CHUNK
0x5F  FS_DOWNLOAD_END
```

如果命令域不够，后续可扩展到：

```text
0x60 ~ 0x6F
```

第一版 `0x40~0x5F` 已经足够。

---

## 9. PC Storage Explorer

### 9.1 UI Goal

PC 端新增独立 Tab：

```text
Storage Explorer
```

目标是让用户像操作文件管理器一样操作 MCU 外部 Flash 中的文件。

### 9.2 UI Layout

建议布局：

```text
Storage Explorer Tab
├── Top Toolbar
│   ├── GET INFO
│   ├── MOUNT
│   ├── FORMAT
│   ├── REFRESH
│   ├── UPLOAD
│   ├── DOWNLOAD
│   ├── DELETE
│   └── NEW FOLDER
│
├── Left Panel
│   └── Directory Tree
│
├── Right Panel
│   └── File Table
│       ├── Name
│       ├── Type
│       ├── Size
│       ├── Modified / Tick
│       └── Flags
│
├── Bottom Panel
│   ├── Capacity Bar
│   ├── Used / Free
│   ├── Current Path
│   └── Operation Progress
│
└── Log
```

### 9.3 First Version UI Scope

第一版可以先不做完整目录树，只做：

```text
1. 当前路径显示
2. 文件表
3. Refresh
4. Upload
5. Download
6. Delete
7. Format
8. Capacity bar
9. Operation log
```

路径初期可以固定为：

```text
/
```

等 FS 命令稳定后再扩展目录树。

---

## 10. File Transfer Protocol

### 10.1 Reason for Chunk Transfer

当前 UART 协议 payload 较小，不能一次传大文件。

因此文件上传/下载必须分块。

原则：

```text
1. 不要一次发送大 payload
2. 每个 chunk 都有 offset / len
3. 支持 ACK / NACK
4. 支持 CRC 校验
5. 后续可支持断点续传
```

### 10.2 Upload Flow

```text
PC -> MCU:
  FS_UPLOAD_BEGIN path=/imu/log001.bin,size=12345,crc=xxxx

MCU -> PC:
  ok,handle=1

PC -> MCU:
  FS_UPLOAD_CHUNK handle=1,offset=0,len=64,data=...

MCU -> PC:
  ok,next=64

PC -> MCU:
  FS_UPLOAD_CHUNK handle=1,offset=64,len=64,data=...

MCU -> PC:
  ok,next=128

...

PC -> MCU:
  FS_UPLOAD_END handle=1

MCU -> PC:
  ok,crc=pass
```

### 10.3 Download Flow

```text
PC -> MCU:
  FS_DOWNLOAD_BEGIN path=/imu/log001.bin

MCU -> PC:
  ok,handle=2,size=12345

PC -> MCU:
  FS_DOWNLOAD_CHUNK handle=2,offset=0,len=64

MCU -> PC:
  data...

PC -> MCU:
  FS_DOWNLOAD_END handle=2

MCU -> PC:
  ok
```

### 10.4 Payload Encoding

第一版可以使用：

```text
ASCII header + binary data
```

例如：

```text
offset=0,len=64;[binary data]
```

但更推荐后续使用二进制结构：

```c
typedef struct
{
    uint8_t handle;
    uint32_t offset;
    uint16_t len;
    uint8_t data[];
} FsChunkPayload_t;
```

二进制结构效率更高，也更不容易解析出错。

---

## 11. Diagnostic and Event Integration

### 11.1 DiagnosticApp Integration

Storage 子系统后续需要接入 DiagnosticApp，提供：

```text
1. Flash ready 状态
2. File system mounted 状态
3. 读次数 / 写次数 / 擦除次数
4. 读错误 / 写错误 / 擦除错误
5. 最近一次错误码
6. 最近一次操作耗时
7. 最大操作耗时
8. 空间使用率
```

### 11.2 McuInfoApp Integration

StorageApp 可以通过 McuInfoApp 提供：

```text
1. Storage 状态查询
2. Storage 事件上报
3. 文件系统错误事件
4. 格式化开始 / 完成事件
5. 上传下载完成事件
```

建议事件：

```text
STORAGE_EVENT_READY
STORAGE_EVENT_MOUNTED
STORAGE_EVENT_FORMAT_STARTED
STORAGE_EVENT_FORMAT_DONE
STORAGE_EVENT_ERROR
STORAGE_EVENT_UPLOAD_DONE
STORAGE_EVENT_DOWNLOAD_DONE
```

---

## 12. Why This Stage Matters

Storage 阶段的意义不是单个 W25Q64JV 驱动，而是让项目从普通嵌入式 demo 变成：

```text
一个具备可观测、可配置、可记录、可升级能力的小型嵌入式节点
```

加入 Storage 后，系统可以扩展：

```text
1. 文件系统浏览器
2. IMU 数据录制
3. 诊断日志持久化
4. 参数配置持久化
5. 固件升级暂存区
6. Bootloader 升级包管理
7. PC 文件上传下载
8. 类 U 盘操作体验
```

FDCAN 是通信能力扩展。

Storage 是系统能力扩展。

它对后续所有模块都有支撑作用。

---

## 13. Development Plan

### Step 5.1: QSPI + W25Q64JV Bring-up

目标：

```text
1. CubeMX QSPI 配置
2. PlatformQspi 初始化
3. 读取 JEDEC ID
4. 读取 Unique ID
5. 读取 Status Register
6. 擦除测试区
7. Page Program
8. ReadBack 校验
9. app_main 测试宏验证
```

产出：

```text
platform_qspi.h/.c
bsp_w25q64jv.h/.c
app_main W25Q64 测试宏
```

### Step 5.2: Block Device + Partition Table

目标：

```text
1. 抽象 read / program / erase / sync
2. 隐藏 page program 拆分
3. 隐藏 sector erase 对齐
4. 建立分区表
5. 提供 storage info
```

产出：

```text
flash_block_device.h/.c
storage_partition.h/.c
```

### Step 5.3: StorageApp + Raw Storage Commands

目标：

```text
1. 建立 StorageApp
2. 复用通用 StateMachine
3. 注册 Storage 命令
4. 实现 STORAGE_GET_ID
5. 实现 STORAGE_GET_INFO
6. 实现 STORAGE_SELF_TEST
7. 实现 STORAGE_GET_STATS
```

产出：

```text
storage_app.h/.c
Storage 命令测试脚本
```

### Step 5.4: LittleFS Integration

目标：

```text
1. 移植 LittleFS
2. 对接 FlashBlockDevice
3. 实现 mount
4. 实现 format
5. 实现 list
6. 实现 read
7. 实现 write
8. 实现 delete
9. 实现 stat
```

产出：

```text
storage_manager.h/.c
littlefs block adapter
FS 命令测试脚本
```

### Step 5.5: PC Storage Explorer

目标：

```text
1. 新增独立 Storage Explorer Tab
2. 文件列表
3. 上传
4. 下载
5. 删除
6. 格式化
7. 容量条
8. 进度条
9. 操作日志
```

产出：

```text
PC Storage Explorer UI
文件上传下载协议
Storage 命令集成
```

---

## 14. Acceptance Criteria

```text
1. MCU 能读取 W25Q64JV JEDEC ID
2. MCU 能读取 W25Q64JV Unique ID
3. MCU 能读取 Status Register
4. MCU 能擦除指定测试扇区
5. MCU 能 Page Program 写入测试数据
6. MCU 能 ReadBack 并通过校验
7. FlashBlockDevice 能提供统一 read / program / erase 接口
8. StorageManager 能识别并管理分区
9. StorageApp 能注册并响应 Storage 命令
10. PC 可以查询 Storage 基础信息
11. PC 可以触发 Storage Self-Test
12. LittleFS 能 mount / format
13. PC 可以 list 文件
14. PC 可以 read / write / delete 文件
15. PC Storage Explorer 可以显示文件表
16. PC Storage Explorer 可以显示容量信息
17. 文件上传下载支持分块传输
18. Storage 错误和耗时可统计
19. Storage 状态可通过 McuInfoApp 暴露给 PC
20. 后续 Bootloader 可以复用 Firmware Staging 分区
```

---

## 15. First Implementation Target

下一步先不要写 LittleFS，也不要写 PC 文件管理器。

第一步目标：

```text
先把 W25Q64JV 读 ID 跑通。
```

需要先完成 CubeMX QSPI 配置，然后实现：

```text
platform_qspi.h/.c
bsp_w25q64jv.h/.c
```

再在 `app_main.c` 里加测试宏：

```c
#define ENABLE_W25Q64_TEST 1
```

先打印：

```text
JEDEC ID
Unique ID
Status Register
Erase / Program / ReadBack result
```

只要这个跑通，再进入 StorageApp 和 StorageManager。

---

## 16. Final Target

最终希望 PC 上出现一个独立 Tab：

```text
Storage Explorer
```

里面可以像操作文件一样操作 MCU 外部 Flash：

```text
/params/config.bin
/logs/fault_0001.log
/imu/record_0001.csv
/ota/app_v1.2.3.bin
```

支持：

```text
上传
下载
删除
刷新
格式化
查看容量
查看文件大小
查看操作日志
查看错误状态
```

这会让系统真正具备“产品级”的持久化能力。
