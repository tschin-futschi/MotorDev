# Plan

Stage: serial_thread_fix
Status: active
Priority: critical
Source: Claude Code
Date: 2026-04-02 01:00:00
Depends-On: review_console_log_20260402_005000.md (pass)
Supersedes: none

## Goal

修复 SerialManager 的跨线程调用问题。ConfigTab（主线程）不得直接调用 SerialManager（工作线程）的方法，必须通过队列连接确保所有操作在工作线程中执行。

## 问题描述

SerialManager 通过 `moveToThread` 运行在工作线程中，其 QSerialPort、QTimer 等对象具有工作线程亲和性。当前 ConfigTab 在主线程直接调用 `m_serialManager->openPort()` / `closePort()`，导致这些方法在主线程执行，触发两个 Qt 警告：

```
QObject: Cannot create children for a parent that is in a different thread.
QObject::startTimer: Timers cannot be started from another thread
```

## Scope

- 修改 `src/tabs/configtab.cpp` — 将 SerialManager 的直接方法调用改为通过 `QMetaObject::invokeMethod` 队列调用

## Non-Goals

- 不修改 SerialManager 的接口或内部实现
- 不修改 UI 布局或其他逻辑
- 不修改 FrameParser、DeviceContext

## Files-In-Scope

- `src/tabs/configtab.cpp` (修改)

## Files-Frozen

- `src/serialmanager.h` / `src/serialmanager.cpp`
- `src/frameparser.h` / `src/frameparser.cpp`
- `src/devicecontext.h` / `src/devicecontext.cpp`
- `src/tabs/configtab.h`
- `src/mainwindow.h` / `src/mainwindow.cpp`
- `CMakeLists.txt`
- 其他所有文件

## Constraints

- 仅修改调用方式，不改变已有的业务逻辑和日志输出
- 使用 `QMetaObject::invokeMethod` + `Qt::QueuedConnection` 确保调用在 SerialManager 所在线程执行

---

## 详细设计

将 ConfigTab 中对 SerialManager 的两处直接调用改为队列调用：

### 1. openPort（connectSignals 中 Connect 按钮 lambda）

当前代码：
```cpp
m_serialManager->openPort(portName, baudRate);
```

修改为：
```cpp
QMetaObject::invokeMethod(m_serialManager, "openPort",
    Qt::QueuedConnection,
    Q_ARG(QString, portName),
    Q_ARG(qint32, baudRate));
```

### 2. closePort（connectSignals 中 Connect 按钮 lambda）

当前代码：
```cpp
m_serialManager->closePort();
```

修改为：
```cpp
QMetaObject::invokeMethod(m_serialManager, "closePort",
    Qt::QueuedConnection);
```

### 3. 需要额外 include

```cpp
#include <QMetaObject>
```

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **警告消除** — 连接 COM 口时控制台不再出现 "Cannot create children for a parent that is in a different thread" 和 "Timers cannot be started from another thread" 警告
3. **功能不变** — Connect / Disconnect 行为与修复前一致（日志输出、UI 状态切换正常）
4. **冻结文件未修改**

## Notes

- 这是一个 Review 遗漏的 bug，应在 config_logic 阶段发现
- 后续所有从主线程调用 SerialManager 方法的地方都必须使用队列调用
