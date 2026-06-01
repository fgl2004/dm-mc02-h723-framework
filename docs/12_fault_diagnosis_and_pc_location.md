# Fault Diagnosis and PC Location

## 1. Document Purpose

本文档记录 STM32H723 项目中的 HardFault / BusFault / MemManage / UsageFault 诊断方法，以及如何通过故障现场中的 PC 地址定位到具体代码位置。

该文档用于支撑后续：

```text
HardFault Handler
Fault Manager
Blackbox Manager
Diagnostic Manager
Watchdog Reset Analysis
Bootloader Failure Analysis
Field Issue Debugging
```

目标不是简单知道“程序死机了”，而是能够回答：

```text
1. 程序为什么死机？
2. 死机发生在什么地址？
3. 这个地址对应哪一段代码？
4. 是访问非法地址、未对齐访问、除零、非法指令，还是总线错误？
5. 是否可以把故障现场保存下来，复位后继续分析？
```

---

## 2. Cortex-M Exception Stack Frame

在 Cortex-M 内核进入异常时，硬件会自动把部分寄存器压入当前栈。

自动压栈内容包括：

```text
R0
R1
R2
R3
R12
LR
PC
xPSR
```

因此，在 HardFault Handler 中拿到异常栈指针后，可以解析出：

```text
stack_frame[0] = R0
stack_frame[1] = R1
stack_frame[2] = R2
stack_frame[3] = R3
stack_frame[4] = R12
stack_frame[5] = LR
stack_frame[6] = PC
stack_frame[7] = xPSR
```

其中最关键的是：

```text
PC = 发生异常时 CPU 正在执行的指令地址
LR = 异常发生前的返回地址或调用链相关地址
xPSR = 程序状态寄存器
```

---

## 3. Why PC Is Important

故障现场中的 PC 地址是定位 HardFault 的核心。

例如串口输出：

```text
PC = 0x08004090
LR = 0x08000CE9
```

其中：

```text
0x08004090
```

表示 CPU 发生异常时正在执行的指令地址。

如果可以把该地址映射回源代码，就可以知道：

```text
哪个函数触发了异常
大概是哪一行 C 代码触发了异常
是非法指针、栈溢出、数组越界，还是未对齐访问
```

---

## 4. Fault Status Registers

Cortex-M 中常见的 Fault 状态寄存器包括：

```text
SCB->CFSR
SCB->HFSR
SCB->DFSR
SCB->AFSR
SCB->MMFAR
SCB->BFAR
```

### 4.1 CFSR

CFSR 是 Configurable Fault Status Register。

它由三部分组成：

```text
CFSR[7:0]    = MMFSR，MemManage Fault Status Register
CFSR[15:8]   = BFSR，BusFault Status Register
CFSR[31:16]  = UFSR，UsageFault Status Register
```

常见 UsageFault 位：

```text
UNDEFINSTR: 执行了未定义指令
INVSTATE  : EPSR/T-bit 状态错误
INVPC     : 非法 PC 或异常返回值
NOCP      : 使用了不可用协处理器
UNALIGNED : 非对齐访问
DIVBYZERO : 除零
```

常见 BusFault 位：

```text
IBUSERR    : 指令总线错误
PRECISERR  : 精确数据总线错误
IMPRECISERR: 非精确数据总线错误
UNSTKERR   : 异常返回出栈时发生总线错误
STKERR     : 异常进入压栈时发生总线错误
BFARVALID  : BFAR 中保存有效故障地址
```

常见 MemManage Fault 位：

```text
IACCVIOL : 指令访问违规
DACCVIOL : 数据访问违规
MSTKERR  : 异常进入压栈时发生 MemManage 错误
MUNSTKERR: 异常返回出栈时发生 MemManage 错误
MMARVALID: MMFAR 中保存有效故障地址
```

---

## 5. HFSR

HFSR 是 HardFault Status Register。

常见位：

```text
VECTTBL : 读取异常向量表时发生总线错误
FORCED  : 可配置 Fault 被升级为 HardFault
DEBUGEVT: Debug event 触发
```

如果看到：

```text
HFSR = 0x40000000
```

通常表示：

```text
FORCED = 1
```

也就是某个 MemManage / BusFault / UsageFault 没有被单独处理，最终升级成 HardFault。

---

## 6. Example: Unaligned Access Fault

测试代码：

```c
volatile uint32_t *bad_addr = (uint32_t *)0xFFFFFFFFU;
*bad_addr = 0x12345678U;
```

故障输出：

```text
PC   = 0x08004090
CFSR = 0x01000000
HFSR = 0x40000000
```

分析：

```text
0xFFFFFFFF 不是 4 字节对齐地址。
代码尝试通过 uint32_t* 进行 32 位写入。
CPU 检测到非对齐访问。
CFSR bit24 被置位，即 UFSR.UNALIGNED。
HFSR.FORCED 置位，说明 UsageFault 最终升级为 HardFault。
```

因此可以判断：

```text
Primary Fault Type: UsageFault
Fault Detail: Unaligned memory access
```

---

## 7. How to Locate PC Address in Keil MDK

### 7.1 Method 1: Use Disassembly Window

当串口输出：

```text
PC = 0x08004090
```

可以在 Keil Debug 模式下：

```text
1. 进入 Debug
2. 打开 Disassembly 窗口
3. 跳转到地址 0x08004090
4. 查看该地址对应的汇编指令
5. 结合附近源码定位触发位置
```

如果当前工程有调试信息，Keil 通常可以显示对应 C 源码位置。

---

### 7.2 Method 2: Use Map File

Keil 编译后会生成 `.map` 文件。

通常位于：

```text
firmware/app/MDK-ARM/<ProjectName>/<ProjectName>.map
```

或者 Keil 输出目录下。

通过 map 文件可以查：

```text
0x08004090 属于哪个函数范围
```

例如 map 文件中可能有：

```text
main.o(i.main)
    0x08003F80   main
```

如果 `main` 的地址范围覆盖 `0x08004090`，说明故障发生在 `main()` 附近。

---

### 7.3 Method 3: Use Debug Symbol and Breakpoint

如果 PC 地址已知，可以在 Keil 中：

```text
1. 打开 Debug
2. 在 Disassembly 中定位 PC 地址
3. 设置断点
4. 重新运行程序
5. 观察触发前的变量和调用关系
```

这种方法适合复现稳定的 fault。

---

## 8. How to Interpret LR

LR 在异常现场中也很重要。

它可能表示：

```text
异常发生前的函数返回地址
异常返回码 EXC_RETURN
调用链附近地址
```

如果 PC 指向具体崩溃指令，LR 可以帮助判断：

```text
是谁调用到了这里
异常发生前从哪里进入
是否是函数返回时栈损坏导致
```

如果出现：

```text
PC 地址异常，例如 0xFFFFFFF9 或 0x00000000
LR 地址异常
```

可能意味着：

```text
函数指针错误
栈损坏
非法跳转
中断返回异常
```

---

## 9. Common Fault Types and Possible Causes

### 9.1 UNALIGNED

可能原因：

```text
未对齐地址访问 uint32_t / uint16_t
强制类型转换错误
协议解析时直接把字节流转成结构体指针
DMA buffer 地址不对齐
```

典型修复：

```text
使用 memcpy 解析非对齐数据
保证结构体和 buffer 对齐
避免直接把 uint8_t* 强转为 uint32_t*
```

---

### 9.2 DIVBYZERO

可能原因：

```text
除数为 0
算法参数未初始化
传感器采样周期为 0
速度估计 dt = 0
```

典型修复：

```text
除法前检查分母
为算法参数设置默认值
采样时间戳异常时拒绝计算
```

---

### 9.3 PRECISERR

可能原因：

```text
访问非法外设地址
访问未映射内存
Flash 擦写时访问错误区域
指针损坏
```

典型处理：

```text
查看 BFAR 是否有效
如果 BFARVALID = 1，BFAR 中保存故障地址
```

---

### 9.4 IMPRECISERR

可能原因：

```text
写缓冲导致的延迟总线错误
DMA 或总线访问异常
Cache / Memory ordering 问题
```

这种错误比 PRECISERR 难定位，因为 PC 不一定指向真正出错指令。

---

### 9.5 INVPC / INVSTATE

可能原因：

```text
函数指针错误
栈被破坏
中断返回值错误
跳转地址没有 Thumb bit
Bootloader 跳转 App 时 VTOR/MSP/Reset_Handler 设置错误
```

Bootloader 阶段尤其要注意这类错误。

---

## 10. Fault Diagnosis Workflow

推荐排查流程：

```text
1. 记录 PC / LR / xPSR
2. 记录 CFSR / HFSR / BFAR / MMFAR
3. 解码 CFSR
4. 判断属于 UsageFault / BusFault / MemManage
5. 如果 BFARVALID / MMARVALID 置位，读取故障地址
6. 用 PC 地址定位代码
7. 用 LR 辅助分析调用关系
8. 判断是否和指针、栈、对齐、除零、非法跳转有关
9. 修复代码
10. 增加断言、参数检查或边界保护
```

---

## 11. Current Project Status

当前项目已经实现：

```text
HardFault_Handler 汇编入口
HardFault_Handler_C 栈帧解析
R0/R1/R2/R3/R12/LR/PC/xPSR 打印
CFSR/HFSR/DFSR/AFSR/MMFAR/BFAR 打印
CFSR/HFSR 自动解码，计划加入
```

当前测试结果：

```text
Fault Trigger: write uint32_t to 0xFFFFFFFF
Observed CFSR: 0x01000000
Observed HFSR: 0x40000000
Analysis: UsageFault UNALIGNED escalated to HardFault
```

---

## 12. Future Improvements

后续计划：

```text
1. 将 HardFault 处理从 main.c 抽离到 Fault Manager
2. 故障现场保存到 Backup SRAM 或 Flash Blackbox
3. 复位后通过 Boot Log 打印上一次 Fault 信息
4. PC Tool 增加 fault decode 命令
5. 将 PC 地址和 map 文件结合，自动辅助定位函数
6. 增加 BusFault / MemManage / UsageFault 独立 Handler
7. 增加栈溢出检测
8. 增加 assert_failed 统一处理
```

---

## 13. Development Rule

后续任何可能导致 HardFault 的测试代码必须使用宏控制：

```c
#define ENABLE_HARDFAULT_TEST 0
```

默认关闭，只有验证时打开。

故障测试完成后必须关闭测试宏，避免正常运行时反复进入 HardFault。
