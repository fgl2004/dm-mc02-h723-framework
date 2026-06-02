# 07 栈空间问题记录：Protocol Manager 测试引发 Tick 异常

## 1. 问题背景

在 Stage 2 UART Frame Protocol 开发过程中，系统已经完成以下模块：

```text
1. UART DMA + RX RingBuffer
2. Protocol Frame Parser
3. Protocol Manager
4. Command Manager
5. PC 侧协议测试脚本
```

在进行 `proto_ping_test.py` 测试时，PC 发送 PING 命令后，MCU 能够正常返回 PONG，说明协议收发链路本身是通的。

但是测试过程中发现 MCU 的周期性运行日志出现异常。

---

## 2. 异常现象

正常情况下，MCU 周期打印：

```text
[INFO] [RUN] uptime = 1000 ms
[INFO] [RUN] uptime = 2000 ms
[INFO] [RUN] uptime = 3000 ms
...
```

运行 PC 协议测试脚本后，出现异常跳变：

```text
[INFO] [RUN] uptime = 13000 ms
[INFO] [RUN] uptime = 2684359565 ms
[INFO] [RUN] uptime = 2684360565 ms
[INFO] [RUN] uptime = 2684361565 ms
```

该现象说明系统 Tick 相关数据或运行时内存区域可能被破坏。

---

## 3. 初步排查过程

最初怀疑点包括：

```text
1. GET_TIME_INFO 命令中的 snprintf 格式化问题
2. ProtocolManager 内部局部变量过大
3. ProtocolFrame CRC 校验临时缓冲区占用栈
4. BoardLog / printf 与二进制协议发送共用 UART
5. 协议收发过程中存在内存越界
```

进一步测试发现：

```text
python proto_ping_test.py --port COM19 --baud 115200 --cmd ping
```

仅发送 PING 命令也会触发该现象。

因此可以排除 GET_TIME_INFO 单独导致问题的可能性，问题更可能出现在基础协议收发链路引入后的运行时资源压力上。

---

## 4. 最终定位

通过增大工程栈空间后，异常消失。

因此当前阶段判断：

```text
该问题主要由栈空间不足引起。
```

协议链路引入后，以下因素增加了栈消耗：

```text
1. ProtocolFrame_t 局部变量
2. CommandManagerResponse_t 局部变量
3. printf / snprintf / BoardLog 调用链
4. ProtocolFrame CRC 校验临时缓冲区
5. 多层函数调用叠加
```

当栈空间不足时，可能破坏相邻内存区域，导致 Tick 值异常跳变。

---

## 5. 当前处理方式

当前临时处理方式：

```text
增大工程栈空间。
```

增大栈空间后：

```text
1. PING / PONG 正常
2. GET_VERSION 正常
3. GET_STATUS 正常
4. Tick 不再异常跳变
```

该处理方式说明问题方向基本正确。

---

## 6. 后续优化方向

后续可以进一步优化，而不是长期只依赖增大栈空间。

建议优化方向：

```text
1. 将较大的局部结构体移动到模块级上下文中
2. 减少协议热路径中的 printf / BoardLog
3. 将 ProtocolFrame CRC 临时缓冲区移出栈
4. 为 App / Protocol / Command 模块建立栈使用估算表
5. 在 HardFault 诊断中补充 MSP / PSP / 栈边界检查
6. 后续引入 FreeRTOS 时，为不同任务单独分配和监控栈空间
```

---

## 7. 工程经验总结

本问题说明：

```text
当工程从 Board Bring-up 进入协议栈、状态机、命令分发阶段后，
运行时资源问题会逐渐显现。
```

在嵌入式系统中，协议功能“逻辑正确”不代表系统“运行安全”。

需要同时关注：

```text
1. 栈空间
2. 堆空间
3. 全局缓冲区
4. 中断与主循环之间的数据边界
5. 日志系统对实时路径的影响
6. 二进制协议数据与调试文本共用 UART 的风险
```

该问题后续需要作为一次典型的嵌入式资源问题案例继续分析。

---

## 8. 当前结论

```text
问题类型：运行时栈空间不足
触发场景：Protocol Manager / Command Manager 协议测试
表现形式：HAL Tick / uptime 异常跳变
当前处理：增大栈空间后恢复正常
后续计划：继续评估协议链路栈使用，并优化大局部变量和日志路径
```

当前阶段先记录该问题，不阻塞后续协议开发。
