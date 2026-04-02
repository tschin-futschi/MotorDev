# Plan

Stage: topbar_serial_status
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 13:00:00
Depends-On: review_ic_commands_sscom_test_20260402_120000.md (pass)
Supersedes: none

## Goal

将 TopBar 中的串口信息区域（端口名 / 波特率 / 连接状态指示灯）接入真实的 SerialManager 状态，断开时显示灰色 "–" + 红点，连接后实时更新端口名、波特率并变为绿点。

## Scope

- `topbar.h / .cpp` — 新增 public slot，使 `connectionLabel` 成为成员变量
- `configtab.h / .cpp` — 新增信号，在串口连接/断开时携带端口和波特率发出
- `mainwindow.h / .cpp` — 存储 `ConfigTab*`，连接信号槽

## Non-Goals

- 不修改 SerialManager（信号不携带端口信息，从 ConfigTab 中转）
- 不修改底部状态栏（单独阶段处理）
- 不实现语言切换功能

## Files-In-Scope

- `src/widgets/topbar.h`
- `src/widgets/topbar.cpp`
- `src/tabs/configtab.h`
- `src/tabs/configtab.cpp`
- `src/mainwindow.h`
- `src/mainwindow.cpp`

## Files-Frozen

- `src/serialmanager.h` / `src/serialmanager.cpp`
- `src/frameparser.h` / `src/frameparser.cpp`
- `src/devicecontext.h` / `src/devicecontext.cpp`
- `src/ui/style_constants.h`
- `src/widgets/activitybar.h` / `src/widgets/activitybar.cpp`
- `CMakeLists.txt`

## Constraints

- `SerialManager::connected()` 不携带端口和波特率，由 ConfigTab 在 `setSerialControlsConnected(true)` 时从 combo 读取后通过信号中转
- TopBar 只接收信号，不持有 SerialManager 引用
- 断开状态下端口显示 `"– / –"`，指示灯用 `DisconnectedIndicator`；连接后显示 `"COM11 / 115200"`，指示灯用 `ConnectedIndicator`

---

## 详细设计

### 1. configtab.h — 新增信号

```cpp
signals:
    void serialConnected(const QString &port, qint32 baudRate);
    void serialDisconnected();
```

### 2. configtab.cpp — 发出信号

在 `setSerialControlsConnected(bool connected)` 末尾新增：

```cpp
if (connected) {
    emit serialConnected(m_portCombo->currentText().trimmed(),
                         m_baudRateCombo->currentText().toInt());
} else {
    emit serialDisconnected();
}
```

### 3. topbar.h — 新增成员和 slots

将 `m_connectionLabel` 从局部变量提升为成员：

```cpp
public slots:
    void onSerialConnected(const QString &port, qint32 baudRate);
    void onSerialDisconnected();

private:
    QLabel *m_connectionIndicator = nullptr;
    QLabel *m_portValueLabel = nullptr;
    QLabel *m_connectionLabel = nullptr;   // 新增
    QComboBox *m_languageCombo = nullptr;
```

### 4. topbar.cpp — 修改构造函数 + 实现 slots

#### 4.1 构造函数

将：
```cpp
auto *connectionLabel = new QLabel(tr("未连接"), this);
```
改为：
```cpp
m_connectionLabel = new QLabel(tr("未连接"), this);
```

同时对 `m_connectionLabel` 应用相同的样式（原来对 `connectionLabel` 的 setStyleSheet 改为 `m_connectionLabel`），布局中的 `connectionLabel` 也改为 `m_connectionLabel`。

初始端口值改为占位符：
```cpp
m_portValueLabel = new QLabel(QStringLiteral("– / –"), this);
```

#### 4.2 新增 slots 实现

```cpp
void TopBar::onSerialConnected(const QString &port, qint32 baudRate) {
    m_portValueLabel->setText(QStringLiteral("%1 / %2").arg(port).arg(baudRate));
    m_connectionLabel->setText(tr("已连接"));
    m_connectionIndicator->setStyleSheet(
        QStringLiteral("background:%1; border-radius:%2px;")
            .arg(Style::Color::ConnectedIndicator.name())
            .arg(Style::Size::IndicatorSize / 2 + 1));
}

void TopBar::onSerialDisconnected() {
    m_portValueLabel->setText(QStringLiteral("– / –"));
    m_connectionLabel->setText(tr("未连接"));
    m_connectionIndicator->setStyleSheet(
        QStringLiteral("background:%1; border-radius:%2px;")
            .arg(Style::Color::DisconnectedIndicator.name())
            .arg(Style::Size::IndicatorSize / 2 + 1));
}
```

### 5. mainwindow.h — 新增 ConfigTab 成员

```cpp
#include "tabs/configtab.h"   // 或保持前向声明 + 在 .cpp 中 include

private:
    ConfigTab *m_configTab = nullptr;   // 新增
```

前向声明区新增：
```cpp
class ConfigTab;
```

### 6. mainwindow.cpp — 存储 ConfigTab 并连接信号

将：
```cpp
m_contentStack->addWidget(new ConfigTab(m_serialManager, m_deviceContext, m_contentStack));
```
改为：
```cpp
m_configTab = new ConfigTab(m_serialManager, m_deviceContext, m_contentStack);
m_contentStack->addWidget(m_configTab);
```

在构造函数末尾（`connect(m_activityBar, ...)` 之后）新增：
```cpp
connect(m_configTab, &ConfigTab::serialConnected,
        m_topBar, &TopBar::onSerialConnected);
connect(m_configTab, &ConfigTab::serialDisconnected,
        m_topBar, &TopBar::onSerialDisconnected);
```

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **断开状态默认显示** — 启动后 TopBar 显示 `"– / –"` + 红色指示灯 + `"未连接"`
3. **连接后更新** — 串口连接成功后，TopBar 立即显示实际端口名 + 波特率 + 绿色指示灯 + `"已连接"`
4. **断开后恢复** — 点击 Disconnect 后，TopBar 恢复 `"– / –"` + 红色指示灯 + `"未连接"`
5. **冻结文件未修改**
