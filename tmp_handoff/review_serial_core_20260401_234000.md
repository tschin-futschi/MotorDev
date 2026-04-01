# Review

Stage: serial_core
Status: pass
Source: Claude Code
Date: 2026-04-01 23:40:00
Related-Report: report_serial_core_20260401_232000.md
Related-Plan: plan_serial_core_20260401_230000.md
Supersedes: none

## Summary

工程师完整实现了 FrameParser 和 SerialManager，代码结构清晰，协议实现与 protocol.md 一致，全部 13 项验收标准通过。

## Findings

None

## 逐项验收

### FrameParser

- [PASS] 控制帧编码：AA 55 帧头 + seq + cmd + len + data + CRC16 大端
- [PASS] CRC16-MODBUS：反射算法 (0xA001)，初始值 0xFFFF，结果大端输出
- [PASS] 控制帧解码状态机：WaitHeader → WaitHeader2 → WaitSeq → WaitCmd → WaitLen → WaitData → WaitCrcHigh → WaitCrcLow，CRC 失败回 WaitHeader
- [PASS] 数据流帧解码状态机：WaitHeader → WaitChannelMask → WaitStreamLen → WaitStreamData → WaitXor
- [PASS] LEN 与 channelMask 一致性校验（countEnabledChannels * 2）
- [PASS] XOR 校验范围正确（channelMask + LEN + data）
- [PASS] int16 大端解析正确
- [PASS] 帧类型识别：WaitHeader 按 0xAA/0xBB 分流
- [PASS] WaitHeader2 处理 AA-AA 重同步（连续 0xAA 时重置控制帧状态）

### SerialManager

- [PASS] 线程模型：构造函数中 moveToThread，QThread::started → init()，QSerialPort 在工作线程创建
- [PASS] 串口参数：Data8 / OneStop / NoParity / NoFlowControl
- [PASS] availablePorts 为静态方法，可任意线程调用
- [PASS] openPort：设置参数 → open → reset parser → 启动心跳 → emit connected
- [PASS] closePortInternal：停止定时器 → 清除 pending → reset parser → close → emit disconnected
- [PASS] sendCommand：编码帧 → 写入串口 → 启动 100ms 重发定时器 → 串行模型（pending 非空则拒绝）
- [PASS] 重发逻辑：onRetryTimeout 检查 retryCount >= MaxRetries(2) → errorOccurred，否则重发
- [PASS] 心跳独立：onHeartbeatTimeout 直接编码写入，不走 sendCommand
- [PASS] 心跳断线判定：每次发送 missed++，收到响应归零，>= 3 则 closePortInternal
- [PASS] handleControlFrame 分流正确：cmd=0x00 心跳 → 归零 / cmd=0x01 错误 → 清 pending + emit / 其他 → seq 匹配后清 pending + emit
- [PASS] 析构函数安全：BlockingQueuedConnection 调用 closePortInternal → quit → wait → delete
- [PASS] qRegisterMetaType<QVector<int16_t>> 用于跨线程信号传递

### 通用检查

- [PASS] 编译通过
- [PASS] 冻结文件未被修改
- [PASS] CMakeLists.txt 仅新增 4 个文件条目

## Test-Result

- [PASS] 全部 13 项验收标准满足

## Decision

- 本轮 Review 通过
- serial_core 阶段完成，通信基础设施就绪
- 可进入下一阶段：ConfigTab 业务逻辑接入（config_logic）
