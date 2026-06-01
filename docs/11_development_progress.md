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
| Stage 0  | Repository and Documentation       | In Progress | 建立仓库、目录、README、总纲文档           |
| Stage 1  | Board Bring-up                     | Not Started | 点亮板子，验证时钟、串口、定时器、复位、HardFault |
| Stage 2  | UART Reliable Protocol             | Not Started | 建立 PC 与 MCU 的可靠串口通信           |
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
| 添加 README.md         | In Progress        | 初稿已准备，需提交                                         |
| 添加 Git 首次推送问题复盘文档    | Done / Need Commit | 建议保存为 `docs/10_git_first_push_issue_summary.md`   |
| 添加项目总纲文档             | In Progress        | `docs/00_project_overview.md`                     |
| 添加进度管理文档             | In Progress        | 当前文档                                              |
| 添加板卡信息文档             | Not Started        | `docs/01_board_notes.md`                          |
| 添加芯片能力文档             | Not Started        | `docs/02_chip_notes.md`                           |
| 添加软件架构文档             | Not Started        | `docs/03_software_architecture.md`                |

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

验证 DM-MC02 / STM32H723 的基础运行能力。

该阶段不追求复杂架构，目标是确认：

```text
板子能启动
时钟配置正确
串口能输出
GPIO 能控制
定时器周期准确
复位原因能读取
HardFault 能捕获
工程能稳定编译、烧录、调试
```

### 6.2 Planned Tasks

| 任务                         | 状态          | 说明                         |
| -------------------------- | ----------- | -------------------------- |
| CubeMX 创建 STM32H723VGT6 工程 | Not Started | 建立基础 app 工程                |
| 配置基础时钟树                    | Not Started | 先使用稳定频率，不急于最高主频            |
| LED / GPIO 测试              | Not Started | 验证 GPIO 输出                 |
|  UART printf               | Done          | USART1 printf redirection verified 
| SysTick 1ms                | Not Started | 建立系统 tick                  |
| Timer GPIO toggle          | Not Started | 用示波器/逻辑分析仪验证定时周期           |
| Reset Reason 读取            | Not Started | 读取复位标志                     |
| HardFault Handler          | Not Started | 主动触发并捕获异常                  |
| Git 提交基础工程                 | Not Started | 提交 `feature/board-bringup` |

### 6.3 Acceptance Criteria

```text
1. 固件可以正常编译和下载
2. 板子上电后稳定运行
3. 串口可以输出 boot log
4. LED 可以周期翻转
5. Timer 输出频率与配置一致
6. Reset reason 可以通过串口打印
7. HardFault 能进入自定义 Handler
8. 代码提交到 GitHub
```

---

## 7. Stage 2: UART Reliable Protocol

### 7.1 Stage Goal

建立 PC 与 MCU 的可靠通信通道。

### 7.2 Planned Tasks

| 任务                 | 状态          | 说明                   |
| ------------------ | ----------- | -------------------- |
| UART DMA + IDLE 接收 | Not Started | H7 需要特别关注 Cache      |
| RX RingBuffer      | Not Started | 字节流缓冲                |
| Frame Parser       | Not Started | 帧头、长度、CRC            |
| CRC16              | Not Started | 基础通信校验               |
| PING 命令            | Not Started | 最小通信闭环               |
| GET_VERSION 命令     | Not Started | 查询固件信息               |
| GET_STATUS 命令      | Not Started | 查询系统状态               |
| Python PC Tool     | Not Started | `h7tool ping/status` |
| 协议压力测试             | Not Started | 半包、粘包、CRC 错误         |

### 7.3 Acceptance Criteria

```text
1. PC 能稳定 ping MCU
2. 连续 ping 10000 次无死机
3. 错误 CRC 帧被拒绝
4. 半包超时后能恢复
5. 粘包能连续解析
6. 超长帧不会导致 buffer 越界
7. 通信错误有计数器
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
