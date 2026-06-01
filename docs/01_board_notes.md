# Board Notes: Damiao DM-MC02

## 1. Document Purpose

本文档用于记录达妙 DM-MC02 开发板的硬件资源、接口分布、外设连接、供电路径、调试接口和后续需要验证的问题。

该文档的目标不是一次性写完所有硬件细节，而是作为后续 Board Bring-up 和驱动开发过程中的持续记录入口。

后续每完成一个外设验证，都需要回到本文档更新对应信息。

---

## 2. Board Basic Information

| Item          | Description                                                                        |
| ------------- | ---------------------------------------------------------------------------------- |
| Board Name    | Damiao DM-MC02                                                                     |
| MCU           | STM32H723VGT6                                                                      |
| Core          | ARM Cortex-M7                                                                      |
| Project Usage | Embedded reliability, diagnostics, security, communication and algorithm framework |
| Main Scenario | Robot / motor control / embedded system training platform                          |

DM-MC02 本项目中的定位：

```text
真实嵌入式控制板
机器人控制器雏形
STM32H7 底层能力训练平台
通信、诊断、算法、安全框架验证平台
```

---

## 3. Board-Level Capability Map

需要重点探索的板级资源：

```text
MCU: STM32H723VGT6
Communication: UART / FDCAN / USB
Sensor: BMI088 IMU
Display: LCD
Debug: SWD / UART log
Storage: Internal Flash
Control: Timer / PWM / GPIO
Analog: ADC
Power: Input power, 3.3V rail, protection circuit
```

---

## 4. MCU

目标 MCU：

```text
STM32H723VGT6
```

后续需要确认：

```text
1. 实际封装
2. Flash 大小
3. SRAM 大小
4. 可用 UART 数量
5. 可用 FDCAN 数量
6. 可用 SPI / I2C 数量
7. ADC 通道
8. Timer / PWM 资源
9. 是否引出 USB
10. 是否具备硬件 RNG / HASH / CRYP，需要结合具体型号确认
```

---

## 5. Power Input

待验证内容：

```text
1. 板卡输入电压范围
2. 是否具备防反接保护
3. 是否具备缓启动电路
4. 3.3V 稳压芯片型号
5. MCU、IMU、LCD、CAN 收发器分别由哪一路供电
6. 是否有 VDDA 独立滤波
7. 是否有电源指示灯
8. 是否有可测量电流的位置
```

实验计划：

```text
1. 用万用表测输入电压
2. 用万用表测 3.3V
3. 上电瞬间观察是否稳定
4. LCD 开关前后测电流变化
5. IMU 使能/关闭前后观察电流变化
6. FDCAN 通信时观察电源稳定性
```

---

## 6. Debug Interface

需要确认：

```text
1. SWDIO 引脚
2. SWCLK 引脚
3. NRST 引脚
4. GND
5. 3.3V Reference
6. 是否板载调试器
7. 是否需要外接 ST-Link / J-Link
```

Bring-up 阶段要求：

```text
1. 能成功连接调试器
2. 能下载最小固件
3. 能单步调试
4. 能查看寄存器
5. 能在 HardFault 中断点停住
```

---

## 7. UART Resources

用途规划：

```text
1. Stage 1: UART printf
2. Stage 2: UART reliable protocol
3. Stage 3: Diagnostic command channel
4. Stage 7: Serial firmware upgrade
```

待确认：

```text
1. 使用哪个 UART 实例
2. TX 引脚
3. RX 引脚
4. 是否经过 USB-to-UART 芯片
5. 默认波特率
6. 是否支持 DMA
7. 是否存在硬件流控
```

第一阶段建议：

```text
Baudrate: 115200
Format: 8N1
Mode: TX printf first, then RX DMA + IDLE
```

---

## 8. FDCAN Resources

用途规划：

```text
1. Stage 5: FDCAN communication
2. 后续可扩展为电机/机器人控制通信
3. 可用于诊断、参数配置、固件升级扩展
```

待确认：

```text
1. 使用 FDCAN1 / FDCAN2 / FDCAN3 中哪一路
2. CAN_TX 引脚
3. CAN_RX 引脚
4. CAN 收发器型号
5. 是否板载 120Ω 终端电阻
6. CANH / CANL 接口位置
7. 是否支持 CAN FD
```

第一阶段测试：

```text
1. Loopback 模式测试
2. Normal 模式收发测试
3. USB-CAN 工具联调
4. RX/TX 计数器
5. Bus-off 诊断
```

---

## 9. IMU

板载 IMU：

```text
BMI088
```

用途规划：

```text
1. Stage 4: IMU 数据采集
2. 滤波算法
3. 零偏估计
4. 静止检测
5. 姿态估计扩展
6. 算法回放测试
```

待确认：

```text
1. BMI088 使用 SPI 还是 I2C
2. 片选引脚
3. 中断引脚
4. 加速度计地址/片选
5. 陀螺仪地址/片选
6. 采样频率配置
7. 供电电压
8. 坐标轴方向
```

实验计划：

```text
1. 读取 WHO_AM_I
2. 读取加速度计原始数据
3. 读取陀螺仪原始数据
4. 静止状态记录噪声
5. 估计陀螺仪零偏
6. 加入一阶低通滤波
7. 加入中值滤波
8. 通过串口或 LCD 显示状态
```

---

## 10. LCD

用途规划：

```text
1. 显示系统状态
2. 显示通信计数器
3. 显示 IMU 状态
4. 显示错误码
5. 显示当前阶段信息
```

待确认：

```text
1. LCD 控制器型号
2. 分辨率
3. 接口类型：SPI / FSMC / RGB / 其他
4. 背光控制引脚
5. Reset 引脚
6. DC 引脚
7. 片选引脚
8. 刷屏速度
```

第一阶段显示内容：

```text
DM-MC02 Framework
Stage: Bring-up
Uptime
Last Error
UART Status
```

---

## 11. Buttons and LEDs

待确认：

```text
1. 是否有用户 LED
2. LED 引脚
3. LED 有效电平
4. 是否有用户按键
5. 按键引脚
6. 按键是否上拉/下拉
7. 是否需要软件消抖
```

Bring-up 验收：

```text
1. LED 1Hz 翻转
2. 按键触发状态变化
3. 按键事件有 Trace 记录
```

---

## 12. Boot Configuration

待确认：

```text
1. BOOT0 引脚如何连接
2. 是否可进入系统 Bootloader
3. 是否有 BOOT 按键
4. NRST 电路
5. 上电复位是否稳定
6. 软件复位后启动路径
```

Bootloader 阶段需要验证：

```text
1. Bootloader 固定运行
2. App valid 后跳转 App
3. App 请求进入 Bootloader
4. 向量表重定位
5. 错误 App 不运行
```

---

## 13. Hardware Diagnostic Commands Planned

未来 PC Tool 可支持：

```text
GET_BOARD_INFO
GET_POWER_STATUS
GET_RESET_REASON
GET_CLOCK_INFO
GET_GPIO_STATE
UART_LOOPBACK_TEST
FDCAN_STATUS
IMU_STATUS
LCD_TEST
ADC_TEST
PWM_TEST
```

---

## 14. Hardware Questions To Verify

当前待验证问题：

```text
1. UART printf 使用哪个串口？
2. BMI088 是 SPI 还是 I2C？
3. LCD 控制器型号是什么？
4. LCD 接口是什么？
5. FDCAN 使用哪一路？
6. CAN 收发器型号是什么？
7. 是否板载 120Ω CAN 终端？
8. 输入电压范围是多少？
9. 3.3V 稳压芯片型号是什么？
10. 是否有外部 Flash？
11. BOOT0 / NRST 电路如何设计？
12. 是否有用户 LED / 按键？
13. SWD 接口位置和引脚定义？
14. 是否可以独立测量板卡电流？
```

---

## 15. Bring-up Priority

第一批验证顺序：

```text
1. Debug connection
2. Clock configuration
3. LED / GPIO
4. UART printf
5. Reset reason
6. HardFault handler
7. Timer GPIO toggle
8. IMU WHO_AM_I
9. LCD basic display
10. FDCAN loopback
```

---

## 16. Update Log

| Date | Update                      |
| ---- | --------------------------- |
| TBD  | Initial board notes created |
