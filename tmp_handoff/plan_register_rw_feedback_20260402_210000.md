# Plan

Stage: register_rw_feedback
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 21:00:00
Depends-On: plan_register_rwtab_ui_20260402_190000.md
Supersedes: none

## Goal

为寄存器读写页的 R/W 按钮补充超时保护和操作反馈：
读操作超时显示 `--`；写操作成功闪烁、失败变暗；全部读取/写入逐行超时后继续队列。

## Scope

1. 修改 `src/tabs/registerrwtab.h`：新增超时定时器成员变量
2. 修改 `src/tabs/registerrwtab.cpp`：
   - 新增 500ms `QTimer`，每次发出命令后启动，收到响应后停止
   - 超时时：读操作调用 `markRowError`，写操作调用 `markWriteButtonFeedback(row, false)`，然后继续队列
   - 收到写成功响应时：调用 `markWriteButtonFeedback(row, true)`
   - 收到错误响应（CmdErrorResponse）时：读操作调用 `markRowError`，写操作调用 `markWriteButtonFeedback(row, false)`
3. 修改 `src/widgets/registertable.h`：新增 `markWriteButtonFeedback(int globalRow, bool success)` 公共方法声明
4. 修改 `src/widgets/registertable.cpp`：实现 `markWriteButtonFeedback`

## Non-Goals

- 改变任何现有的读写命令发送逻辑
- 改变队列管理逻辑
- 超时时间可配置（固定 500ms）
- 状态栏日志（本次不做）

## Files-In-Scope

- `src/tabs/registerrwtab.h`
- `src/tabs/registerrwtab.cpp`
- `src/widgets/registertable.h`
- `src/widgets/registertable.cpp`

## Files-Frozen

- `src/protocol/motor_protocol.h / .cpp`
- `src/ui/style_constants.h`
- `src/tabs/configtab.h / .cpp`
- `src/mainwindow.h / .cpp`
- `CMakeLists.txt`

## Constraints

### 超时定时器约束

- 使用单个 `QTimer m_timeoutTimer`（singleShot 模式），成员变量
- 每次调用 `sendCommand`（通过 `invokeMethod`）之前或之后立即启动定时器，间隔 500ms
- 收到有效响应（任意 cmd 匹配当前操作）时，立即 `m_timeoutTimer.stop()`
- 超时槽函数 `onTimeout()`：
  - 若 `m_isWriteOp`：调用 `m_registerTable->markWriteButtonFeedback(m_pendingRow, false)`
  - 若非写操作：调用 `m_registerTable->markRowError(m_pendingRow)`
  - 之后调用 `processNextInQueue()`

### 写按钮反馈约束

`RegisterTable::markWriteButtonFeedback(int globalRow, bool success)` 实现规则：

- 找到对应行的 W 按钮（`m_writeButtons[globalRow]`）
- **成功（success = true）**：
  - 按钮短暂切换为高亮样式（使用 `Style::Color::PrimaryGreen` 作为背景）
  - 200ms 后恢复原始 `makeWriteButtonStyle()`
  - 使用单次 `QTimer::singleShot` 实现，不阻塞
- **失败（success = false）**：
  - 按钮短暂切换为暗色样式（使用 `Style::Color::MutedText` 作为背景，白色文字）
  - 200ms 后恢复原始 `makeWriteButtonStyle()`
  - 使用单次 `QTimer::singleShot` 实现，不阻塞

### onFrameReceived 修改约束

现有逻辑不变，在以下位置插入 `m_timeoutTimer.stop()`：
- `cmd == CmdReadRegister && !m_isWriteOp` 分支：stop 后处理响应
- `cmd == CmdWriteRegister && m_isWriteOp` 分支：stop 后调用 `markWriteButtonFeedback(m_pendingRow, true)`，再调用 `processNextInQueue()`
- `cmd == CmdErrorResponse` 分支：stop 后按 m_isWriteOp 分别调用 markWriteButtonFeedback(false) 或 markRowError，再调用 processNextInQueue()

### processNextInQueue 修改约束

在每次调用 `invokeMethod sendCommand` 之后，紧接着启动定时器：
```
m_timeoutTimer.start(500);
```

## Acceptance

- 点击 R，设备正常回应 → 值列显示读回值 ✓
- 点击 R，无回应 500ms → 值列显示 `--`，可重新点击 ✓
- 点击 W，设备正常回应 → W 按钮短暂高亮绿色后恢复 ✓
- 点击 W，设备回错误响应或无回应 500ms → W 按钮短暂变暗后恢复 ✓
- 全部读取：单行超时 → 该行 `--` → 继续下一行，不卡队列 ✓
- 全部写入：单行超时 → 该行 W 按钮变暗恢复 → 继续下一行，不卡队列 ✓
- 编译通过，无新警告 ✓

## Notes

- `makeWriteButtonStyle` 函数在 `registertable.cpp` 匿名 namespace 中，`markWriteButtonFeedback` 需要在同文件中调用，无需提取到头文件
- 成功高亮颜色使用 `Style::Color::PrimaryGreen`，失败暗色使用 `Style::Color::MutedText`，不得硬编码色值
- 反馈动画时长固定 200ms，不可配置
