# Plan

Stage: heartbeat_defer
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 01:30:00
Depends-On: review_serial_thread_fix_20260402_012000.md (pass)
Supersedes: none

## Goal

连接串口时不自动启动心跳，改为由外部显式调用启用。方便开发调试阶段在没有 STM32 固件回复的情况下保持连接。

## Scope

- 修改 `src/serialmanager.h` — 新增 `startHeartbeat()` / `stopHeartbeat()` public slots
- 修改 `src/serialmanager.cpp` — openPort 中移除心跳自动启动，新增两个 slot 实现

## Non-Goals

- 不修改 FrameParser、DeviceContext、ConfigTab
- 不修改心跳的内部逻辑（间隔、断线判定条件不变）

## Files-In-Scope

- `src/serialmanager.h` (修改)
- `src/serialmanager.cpp` (修改)

## Files-Frozen

- `src/frameparser.h` / `src/frameparser.cpp`
- `src/devicecontext.h` / `src/devicecontext.cpp`
- `src/tabs/*.h` / `src/tabs/*.cpp`
- `src/widgets/*.h` / `src/widgets/*.cpp`
- `src/mainwindow.h` / `src/mainwindow.cpp`
- `src/main.cpp`
- `src/ui/style_constants.h`
- `CMakeLists.txt`

## Constraints

- 心跳逻辑本身不变（1 秒间隔、3 次未响应断线），只改启动时机
- startHeartbeat / stopHeartbeat 为 public slots，便于后续通过队列连接从主线程调用

---

## 详细设计

### 1. serialmanager.h

新增两个 public slots：

```cpp
public slots:
    void startHeartbeat();
    void stopHeartbeat();
```

### 2. serialmanager.cpp

#### openPort 中移除心跳自动启动

当前代码：
```cpp
if (m_heartbeatTimer != nullptr) {
    m_heartbeatTimer->start();
}
```

删除这三行。

#### 新增 startHeartbeat

```cpp
void SerialManager::startHeartbeat() {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        return;
    }
    m_missedHeartbeats = 0;
    if (m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->start();
    }
    qDebug() << "Heartbeat started";
}
```

#### 新增 stopHeartbeat

```cpp
void SerialManager::stopHeartbeat() {
    stopHeartbeatTimer();
    m_missedHeartbeats = 0;
    qDebug() << "Heartbeat stopped";
}
```

#### closePortInternal 保持不变

关闭端口时仍然停止心跳定时器（已有逻辑），无需修改。

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **连接不自动断开** — 连接 COM 口后不再自动启动心跳，连接保持稳定
3. **startHeartbeat / stopHeartbeat 接口存在** — 两个 public slots 可供后续调用
4. **closePort 仍停止心跳** — 断开连接时心跳定时器正确停止
5. **冻结文件未修改**

## Notes

- 后续与 STM32 真实通信时，可在连接成功后调用 startHeartbeat() 启用心跳
