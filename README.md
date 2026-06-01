\# DM-MC02 H723 Embedded Framework



本项目基于达妙 DM-MC02 开发板与 STM32H723 MCU，构建一个面向嵌入式系统训练的可靠通信、诊断、安全、升级与算法框架。



项目目标不是做一个简单外设 Demo，而是以一块真实开发板为平台，逐步搭建接近企业级 MCU 项目的系统骨架，覆盖底层驱动、通信协议、状态机、缓冲区、诊断、Bootloader、低功耗、安全认证、算法运行与自动化测试等内容。



\---



\## 1. Target Board



\* Board: Damiao DM-MC02

\* MCU: STM32H723VGT6

\* Core: ARM Cortex-M7

\* Main Interfaces:



&#x20; \* UART

&#x20; \* FDCAN

&#x20; \* SPI / I2C

&#x20; \* IMU

&#x20; \* LCD

&#x20; \* Flash

&#x20; \* Timer / PWM

&#x20; \* ADC

&#x20; \* GPIO



\---



\## 2. Project Goals



本项目主要用于系统性训练以下能力：



1\. 探索 STM32H723 Cortex-M7 底层能力

2\. 构建 UART / FDCAN 可靠通信框架

3\. 实现嵌入式分层架构与 Manager 化模块设计

4\. 实现状态机、缓冲区、非阻塞调度与诊断系统

5\. 实现参数管理、Flash 双备份和掉电保护

6\. 实现 Bootloader、固件升级和固件签名验证

7\. 实现低功耗管理、复位原因分析和 HardFault 定位

8\. 实现 IMU 数据采集、滤波、状态估计与算法测试

9\. 实现 Chaos 测试、自动化测试、产测与自检机制

10\. 通过 Platform Abstraction Layer 提升系统移植性



\---



\## 3. Architecture



项目采用分层架构：



```text

Application Layer

&#x20;   ↓

Service / Manager Layer

&#x20;   ↓

Middleware Layer

&#x20;   ↓

Platform Abstraction Layer

&#x20;   ↓

BSP / HAL Layer

&#x20;   ↓

Hardware

```



\### Application Layer



负责设备业务流程和应用逻辑，例如：



\* 设备运行流程

\* 产测流程

\* 算法演示流程

\* 升级流程入口



\### Service / Manager Layer



负责系统级能力和策略收口，例如：



\* Device Manager

\* Command Manager

\* Protocol Manager

\* Parameter Manager

\* Diagnostic Manager

\* Buffer Manager

\* Health Manager

\* Upgrade Manager

\* Power Manager

\* Security Manager

\* Algorithm Manager

\* Chaos Manager



\### Middleware Layer



负责通用机制，例如：



\* RingBuffer

\* MessageQueue

\* CRC16 / CRC32

\* Software Timer

\* Event Bus

\* Protocol Frame Parser

\* File Transfer

\* Log / Trace Buffer



\### Platform Abstraction Layer



负责屏蔽具体芯片和板级差异，例如：



\* platform\_uart

\* platform\_fdcan

\* platform\_flash

\* platform\_time

\* platform\_gpio

\* platform\_power

\* platform\_watchdog

\* platform\_rng



\### BSP / HAL Layer



负责具体外设、引脚、时钟、DMA、Cache、Flash、LCD、IMU 等底层硬件适配。



\---



\## 4. Main Modules



计划逐步实现以下模块：



```text

Protocol Manager       可靠通信协议、ACK/NACK、重传、去重

Command Manager        命令注册、权限检查、命令分发

Device Manager         设备状态机与状态切换

Parameter Manager      参数管理、Flash 双备份、CRC/HMAC

Diagnostic Manager     错误码、计数器、系统状态查询

Buffer Manager         RX/TX/Log/Trace Buffer 统计与溢出检测

Health Manager         任务健康监控与软件看门狗

Upgrade Manager        Bootloader、固件升级、App confirmed

Power Manager          Sleep/Stop/Standby、唤醒恢复

Security Manager       HMAC、固件签名、权限认证、防重放

Algorithm Manager      滤波、PID、故障检测、算法统计与回放

Chaos Manager          异常注入、鲁棒性测试、压力测试

Factory Manager        产测、自检、SN、Factory Lock

```



\---



\## 5. Development Roadmap



\### Stage 0: Project Initialization



\* 创建 GitHub 仓库

\* 建立项目目录结构

\* 添加 README、docs、firmware、pc\_tool 等基础目录

\* 配置 `.gitignore`

\* 完成首次提交



\### Stage 1: Board Bring-up



\* LED / GPIO 测试

\* UART printf

\* SysTick

\* Timer GPIO toggle

\* Reset reason

\* HardFault test

\* Clock tree verification



\### Stage 2: UART Reliable Protocol



\* UART DMA + IDLE

\* Cache / DMA 处理

\* RX RingBuffer

\* Frame parser

\* CRC16

\* PING / GET\_VERSION / GET\_STATUS

\* Python PC Tool



\### Stage 3: Diagnostic Framework



\* Device Manager

\* Command Manager

\* Diagnostic Manager

\* Buffer Manager

\* Log / Trace

\* Health Manager



\### Stage 4: IMU Algorithm Loop



\* BMI088 driver

\* IMU sample timestamp

\* Low-pass filter

\* Median filter

\* Static detection

\* Bias estimation

\* Algorithm statistics



\### Stage 5: FDCAN Communication



\* FDCAN initialization

\* CAN RX/TX queue

\* FDCAN diagnostic counters

\* CAN stress test



\### Stage 6: Parameter and Flash



\* Parameter Manager

\* Flash A/B backup

\* CRC32

\* Version and sequence

\* Default parameter recovery



\### Stage 7: Bootloader and Upgrade



\* Bootloader project

\* App jump

\* Vector table relocation

\* Firmware header

\* CRC/SHA check

\* App valid / App confirmed



\### Stage 8: Security



\* HMAC dangerous command authentication

\* Parameter HMAC

\* Firmware signature verification

\* Replay protection

\* Security log



\### Stage 9: Chaos and Automated Test



\* Protocol fuzz test

\* Buffer overflow test

\* Upgrade interruption test

\* Fault injection

\* Watchdog test

\* Security test report



\---



\## 6. Repository Structure



```text

dm-mc02-h723-framework/

├── docs/

├── firmware/

│   ├── app/

│   └── bootloader/

├── pc\_tool/

├── tools/

├── scripts/

├── tests/

├── release/

├── README.md

├── LICENSE

└── .gitignore

```



\---



\## 7. Current Status



Current stage:



```text

Stage 0: Project Initialization

```



Completed:



\* Project directory created

\* Git repository initialized

\* GitHub remote configured

\* SSH push configured

\* Initial repository structure prepared



Next step:



```text

Add board notes, chip notes, and software architecture documentation.

```



\---



\## 8. Development Notes



This project focuses on three main capability lines:



```text

1\. Software Architecture

&#x20;  Layering, Manager design, state machine, protocol, buffer, diagnostics.



2\. Hardware and Physical Thinking

&#x20;  Power, clock, signal, DMA, Cache, timing, reset, low power, measurement.



3\. Algorithm Thinking

&#x20;  Filtering, estimation, control, fault detection, logic algorithms, replay test.

```



The long-term goal is to build a reusable embedded framework that is reliable, observable, testable, upgradable, secure, and portable.



