# Plan

Stage: register_rw_logging
Status: active
Priority: low
Source: Claude Code
Date: 2026-04-02 21:50:00
Depends-On: none
Supersedes: none

## Goal

在 `registerrwtab.cpp` 的所有读写命令发送和响应处理路径中加入 qDebug 日志，
方便调试时在 IDE 控制台追踪操作过程。

## Scope

1. 修改 `src/tabs/registerrwtab.cpp`：在以下位置各添加一条 qDebug

## Non-Goals

- 修改任何现有逻辑
- 日志面板输出（已有单独实现，不在本次范围）
- 超时日志（在 plan_register_rw_feedback 中随超时功能一起加）

## Files-In-Scope

- `src/tabs/registerrwtab.cpp`

## Files-Frozen

- 所有其他文件

## Constraints

### 日志位置与格式

| 位置 | 触发时机 | 日志内容示例 |
|------|---------|-------------|
| `onReadRowRequested` 发送命令前 | 单行读发出 | `[RW] Read  row=3 addr=0x0020` |
| `onWriteRowRequested` 发送命令前 | 单行写发出 | `[RW] Write row=3 addr=0x0020 val=0x1234` |
| `onFrameReceived` CmdReadRegister 分支，解码成功后 | 读回值 | `[RW] Read  OK   row=3 val=0x1A2B` |
| `onFrameReceived` CmdReadRegister 分支，解码失败后 | 读解码失败 | `[RW] Read  FAIL row=3 (decode error)` |
| `onFrameReceived` CmdWriteRegister 分支 | 写 ACK 收到 | `[RW] Write ACK  row=3` |
| `onFrameReceived` CmdErrorResponse 分支 | 收到错误响应 | `[RW] Error row=3 code=0x01` |
| `processNextInQueue` 队列耗尽时 | 批量操作结束 | `[RW] Queue done (read/write)` |

### 格式约束

- 统一使用 `qDebug().noquote()` 输出 `QStringLiteral`
- 地址和值均以 4 位大写十六进制格式输出（`%1` + `.arg(x, 4, 16, QChar('0')).toUpper()`）
- 错误码以 2 位大写十六进制输出
- 所有日志行以 `[RW]` 开头，便于过滤

## Acceptance

- 点击 R，IDE 控制台输出发送日志和响应日志各一条
- 点击 W，IDE 控制台输出发送日志和响应日志各一条
- 收到 CmdErrorResponse 时输出错误日志
- 全部读取完成后输出 `Queue done` 日志
- 编译通过，无新警告

## Notes

- 超时日志将在 `plan_register_rw_feedback_20260402_210000.md` 实现时一并加入
