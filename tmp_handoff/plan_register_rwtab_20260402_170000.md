# Plan

Stage: register_rwtab
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 17:00:00
Depends-On: review_statusbar_logpanel_20260402_160000.md (pass)
Supersedes: none

## Goal

实现寄存器读写页（RegisterRwTab）的完整功能：4 组 × 4 行固定表格，支持单行读 / 写 / 全部读 / 全部写，配置持久化到本地 JSON，串口未连接时禁用所有非配置页面。

## Scope

- 新建 `src/widgets/registertable.h` / `src/widgets/registertable.cpp`
- 修改 `src/tabs/registerrwtab.h` / `src/tabs/registerrwtab.cpp`（替换占位实现）
- 修改 `src/mainwindow.h` / `src/mainwindow.cpp`（传入 SerialManager，连接 state 信号）
- 修改 `src/ui/style_constants.h`（新增寄存器相关颜色常量，不得修改已有常量）
- 修改 `CMakeLists.txt`（新增 registertable 源文件）

## Non-Goals

- 不实现行的添加 / 删除
- 不实现批量读命令（0x22），全部读取使用逐行 0x20
- 不实现导入 / 导出文件对话框
- 不实现示波器 / 烧录页功能
- 不修改 SerialManager / FrameParser / DeviceContext

## Files-In-Scope

- `src/widgets/registertable.h`（新建）
- `src/widgets/registertable.cpp`（新建）
- `src/tabs/registerrwtab.h`（修改）
- `src/tabs/registerrwtab.cpp`（修改）
- `src/mainwindow.h`（修改）
- `src/mainwindow.cpp`（修改）
- `src/ui/style_constants.h`（仅允许新增常量）
- `CMakeLists.txt`（修改）

## Files-Frozen

- `src/serialmanager.h` / `src/serialmanager.cpp`
- `src/frameparser.h` / `src/frameparser.cpp`
- `src/devicecontext.h` / `src/devicecontext.cpp`
- `src/widgets/topbar.h` / `src/widgets/topbar.cpp`
- `src/widgets/activitybar.h` / `src/widgets/activitybar.cpp`
- `src/widgets/logpanel.h` / `src/widgets/logpanel.cpp`
- `src/widgets/sidebar.h` / `src/widgets/sidebar.cpp`
- `src/tabs/configtab.h` / `src/tabs/configtab.cpp`
- `src/tabs/fwflashtab.h` / `src/tabs/fwflashtab.cpp`
- `src/tabs/oscilloscoptab.h` / `src/tabs/oscilloscoptab.cpp`
- `src/main.cpp`

## Constraints

- SerialManager 单命令等待机制：一次只能有一条挂起命令（100ms 超时，最多重发 2 次）。全部读 / 全部写必须顺序发出，收到响应后再发下一条，不得并发。
- UI / 逻辑严格分离：RegisterTable 只做 UI，不直接引用 SerialManager；RegisterRwTab 负责业务逻辑。
- 所有颜色 / 尺寸必须使用 style_constants.h 中的命名常量，不得在业务代码中硬编码颜色值。
- 持久化文件路径使用 `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/registers.json"`。
- RegisterTable 内的按钮必须作为成员变量或通过 `setCellWidget` 管理，不得存在于构造函数局部作用域中（Review 必查项）。
- globalRow = group × 4 + row（group 0~3，row 0~3），取值范围 0~15。

---

## 详细设计

### 1. style_constants.h — 新增常量

在 `namespace Color` 中追加：

```cpp
inline const QColor ReadButtonBackground("#E6F1FB");
inline const QColor ReadButtonForeground("#185FA5");
inline const QColor WriteButtonBackground("#FAEEDA");
inline const QColor WriteButtonForeground("#854F0B");
inline const QColor RegisterValueText("#185FA5");
inline const QColor RegisterErrorText("#E24B4A");
inline const QColor TableEvenRowBackground("#fafaf8");
inline const QColor TableHoverRowBackground("#f0f7e8");
inline const QColor GroupDivider("#aaaaaa");
inline const QColor TableHeaderBackground("#f0ede8");
inline const QColor TableHeaderText("#555555");
```

在 `namespace Size` 中追加：

```cpp
inline constexpr int TableRowHeight    = 19;
inline constexpr int TableHeaderHeight = 24;
inline constexpr int TableGroupCount   = 4;
inline constexpr int TableRowCount     = 4;
inline constexpr int TableGroupDividerWidth = 2;
// 列宽（每组）
inline constexpr int ColDescWidth  = 110;
inline constexpr int ColAddrWidth  = 52;
inline constexpr int ColValueWidth = 56;
inline constexpr int ColReadWidth  = 26;
inline constexpr int ColWriteWidth = 23;
```

---

### 2. src/widgets/registertable.h

```cpp
#pragma once

#include <QWidget>
#include <cstdint>

class QTableWidget;

class RegisterTable : public QWidget {
    Q_OBJECT

public:
    explicit RegisterTable(QWidget *parent = nullptr);

    // 连接状态：控制按钮和单元格可编辑性
    void setConnected(bool connected);

    // 由 RegisterRwTab 调用：更新值列显示
    void updateRowValue(int globalRow, quint16 value);
    void markRowError(int globalRow);   // 显示 "--"
    void markRowPending(int globalRow); // 可选：显示 "..."

    // 查询行数据（供 RegisterRwTab 读取）
    bool        rowHasAddress(int globalRow) const;
    bool        rowHasValue(int globalRow) const;
    quint16     rowAddress(int globalRow) const;   // 解析地址列十六进制字符串
    quint16     rowValue(int globalRow) const;     // 解析值列十六进制字符串

    // 持久化
    void saveConfig(const QString &path) const;
    void loadConfig(const QString &path);

signals:
    void readRowRequested(int globalRow, quint16 addr);
    void writeRowRequested(int globalRow, quint16 addr, quint16 value);
    // 任何描述/地址单元格内容变更后触发（用于自动保存）
    void configChanged();

private:
    void setupUi();
    void connectSignals();
    void applyColumnWidths();

    // 内部辅助：列索引
    static int descCol(int group)  { return group * 5 + 0; }
    static int addrCol(int group)  { return group * 5 + 1; }
    static int valueCol(int group) { return group * 5 + 2; }
    static int readCol(int group)  { return group * 5 + 3; }
    static int writeCol(int group) { return group * 5 + 4; }

    QTableWidget *m_table = nullptr;
    // 按钮数组：m_readButtons[globalRow], m_writeButtons[globalRow]
    QPushButton *m_readButtons[16]  = {};
    QPushButton *m_writeButtons[16] = {};
};
```

---

### 3. src/widgets/registertable.cpp — 关键实现要点

#### 3.1 表格结构

```cpp
// 4 行，20 列（每组 5 列 × 4 组）
m_table = new QTableWidget(TableRowCount, TableGroupCount * 5, this);
m_table->verticalHeader()->setVisible(false);
m_table->horizontalHeader()->setVisible(true);
m_table->setShowGrid(true);
m_table->setSelectionMode(QAbstractItemView::NoSelection);
m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
```

#### 3.2 表头标签

重复 4 组，每组列头依次为：`"描述"` / `"地址"` / `"值"` / `"读"` / `"写"`

```cpp
const QStringList groupHeaders = { tr("描述"), tr("地址"), tr("值"), tr("读"), tr("写") };
for (int g = 0; g < 4; ++g) {
    for (int c = 0; c < 5; ++c) {
        m_table->setHorizontalHeaderItem(g * 5 + c,
            new QTableWidgetItem(groupHeaders[c]));
    }
}
```

#### 3.3 行高 / 列宽

```cpp
m_table->verticalHeader()->setDefaultSectionSize(Style::Size::TableRowHeight);
m_table->horizontalHeader()->setFixedHeight(Style::Size::TableHeaderHeight);
// 列宽按 ColDescWidth / ColAddrWidth 等常量设置
```

#### 3.4 单元格初始化（每行每组）

```cpp
for (int row = 0; row < 4; ++row) {
    for (int g = 0; g < 4; ++g) {
        int globalRow = g * 4 + row;

        // 描述列：可编辑文字
        auto *descItem = new QTableWidgetItem(QString{});
        descItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_table->setItem(row, descCol(g), descItem);

        // 地址列：可编辑，monospace
        auto *addrItem = new QTableWidgetItem(QString{});
        addrItem->setTextAlignment(Qt::AlignCenter);
        addrItem->setFont(QFont("Consolas,monospace", 11));
        m_table->setItem(row, addrCol(g), addrItem);

        // 值列：可编辑（写入用），初始为空
        auto *valueItem = new QTableWidgetItem(QString{});
        valueItem->setTextAlignment(Qt::AlignCenter);
        valueItem->setFont(QFont("Consolas,monospace", 11));
        valueItem->setForeground(QBrush(Style::Color::RegisterValueText));
        m_table->setItem(row, valueCol(g), valueItem);

        // 读按钮
        auto *readBtn = new QPushButton(tr("读"), m_table);
        readBtn->setFixedHeight(Style::Size::TableRowHeight - 2);
        readBtn->setStyleSheet(/* 蓝色系，见下方样式 */);
        m_table->setCellWidget(row, readCol(g), readBtn);
        m_readButtons[globalRow] = readBtn;

        // 写按钮
        auto *writeBtn = new QPushButton(tr("写"), m_table);
        writeBtn->setFixedHeight(Style::Size::TableRowHeight - 2);
        writeBtn->setStyleSheet(/* 橙色系，见下方样式 */);
        m_table->setCellWidget(row, writeCol(g), writeBtn);
        m_writeButtons[globalRow] = writeBtn;
    }
}
```

#### 3.5 按钮样式

```cpp
// 读按钮
QStringLiteral(
    "QPushButton { background:%1; border:1px solid %2; color:%2; font-size:10px; border-radius:2px; }"
    "QPushButton:hover { background:%2; color:#fff; }"
    "QPushButton:disabled { background:#e8e8e8; border:1px solid #ccc; color:#aaa; }")
    .arg(Style::Color::ReadButtonBackground.name())
    .arg(Style::Color::ReadButtonForeground.name())

// 写按钮
QStringLiteral(
    "QPushButton { background:%1; border:1px solid %2; color:%2; font-size:10px; border-radius:2px; }"
    "QPushButton:hover { background:%2; color:#fff; }"
    "QPushButton:disabled { background:#e8e8e8; border:1px solid #ccc; color:#aaa; }")
    .arg(Style::Color::WriteButtonBackground.name())
    .arg(Style::Color::WriteButtonForeground.name())
```

#### 3.6 组分隔线（自定义 ItemDelegate）

新建内部类 `GroupDividerDelegate : public QStyledItemDelegate`，在 `paint()` 中对 column % 5 == 0 且 column > 0 的列绘制左侧 2px 竖线：

```cpp
void paint(QPainter *painter, const QStyleOptionViewItem &option,
           const QModelIndex &index) const override {
    QStyledItemDelegate::paint(painter, option, index);
    if (index.column() > 0 && index.column() % 5 == 0) {
        painter->save();
        painter->setPen(QPen(Style::Color::GroupDivider, Style::Size::TableGroupDividerWidth));
        painter->drawLine(option.rect.topLeft(), option.rect.bottomLeft());
        painter->restore();
    }
}
```

在 `setupUi()` 末尾：
```cpp
m_table->setItemDelegate(new GroupDividerDelegate(m_table));
```

> `GroupDividerDelegate` 定义在 `registertable.cpp` 中（匿名命名空间或文件内类），不需要单独头文件。

#### 3.7 信号连接（connectSignals）

```cpp
// 读按钮点击 → 发出 readRowRequested
for (int globalRow = 0; globalRow < 16; ++globalRow) {
    connect(m_readButtons[globalRow], &QPushButton::clicked, this, [this, globalRow]() {
        if (rowHasAddress(globalRow))
            emit readRowRequested(globalRow, rowAddress(globalRow));
    });
    connect(m_writeButtons[globalRow], &QPushButton::clicked, this, [this, globalRow]() {
        if (rowHasAddress(globalRow) && rowHasValue(globalRow))
            emit writeRowRequested(globalRow, rowAddress(globalRow), rowValue(globalRow));
    });
}

// 单元格内容变更 → 触发 configChanged（用于自动保存）
connect(m_table, &QTableWidget::itemChanged, this, &RegisterTable::configChanged);
```

#### 3.8 updateRowValue / markRowError

```cpp
void RegisterTable::updateRowValue(int globalRow, quint16 value) {
    int group = globalRow / 4;
    int row   = globalRow % 4;
    auto *item = m_table->item(row, valueCol(group));
    if (!item) return;
    item->setText(QString("%1").arg(value, 4, 16, QChar('0')).toUpper());
    item->setForeground(QBrush(Style::Color::RegisterValueText));
}

void RegisterTable::markRowError(int globalRow) {
    int group = globalRow / 4;
    int row   = globalRow % 4;
    auto *item = m_table->item(row, valueCol(group));
    if (!item) return;
    item->setText(QStringLiteral("--"));
    item->setForeground(QBrush(Style::Color::RegisterErrorText));
}
```

#### 3.9 rowAddress / rowValue 解析

```cpp
quint16 RegisterTable::rowAddress(int globalRow) const {
    int group = globalRow / 4;
    int row   = globalRow % 4;
    auto *item = m_table->item(row, addrCol(group));
    if (!item || item->text().isEmpty()) return 0;
    bool ok = false;
    quint16 addr = item->text().toUShort(&ok, 16);
    return ok ? addr : 0;
}

bool RegisterTable::rowHasAddress(int globalRow) const {
    int group = globalRow / 4;
    int row   = globalRow % 4;
    auto *item = m_table->item(row, addrCol(group));
    return item && !item->text().trimmed().isEmpty();
}
```

#### 3.10 setConnected

```cpp
void RegisterTable::setConnected(bool connected) {
    for (int i = 0; i < 16; ++i) {
        if (m_readButtons[i])  m_readButtons[i]->setEnabled(connected);
        if (m_writeButtons[i]) m_writeButtons[i]->setEnabled(connected);
    }
    // 地址和描述列保持可编辑（允许用户提前配置）
}
```

#### 3.11 持久化（saveConfig / loadConfig）

```cpp
void RegisterTable::saveConfig(const QString &path) const {
    QJsonArray arr;
    for (int globalRow = 0; globalRow < 16; ++globalRow) {
        int group = globalRow / 4;
        int row   = globalRow % 4;
        QJsonObject entry;
        auto *descItem  = m_table->item(row, descCol(group));
        auto *addrItem  = m_table->item(row, addrCol(group));
        entry["desc"] = descItem  ? descItem->text()  : QString{};
        entry["addr"] = addrItem  ? addrItem->text()  : QString{};
        arr.append(entry);
    }
    QJsonObject root;
    root["version"]   = 1;
    root["registers"] = arr;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void RegisterTable::loadConfig(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    QJsonArray arr = doc.object()["registers"].toArray();
    // 临时阻断 itemChanged 信号，避免 loadConfig 期间触发多次保存
    m_table->blockSignals(true);
    for (int globalRow = 0; globalRow < qMin(16, arr.size()); ++globalRow) {
        int group = globalRow / 4;
        int row   = globalRow % 4;
        QJsonObject entry = arr[globalRow].toObject();
        if (auto *item = m_table->item(row, descCol(group)))
            item->setText(entry["desc"].toString());
        if (auto *item = m_table->item(row, addrCol(group)))
            item->setText(entry["addr"].toString());
    }
    m_table->blockSignals(false);
}
```

---

### 4. src/tabs/registerrwtab.h

```cpp
#pragma once

#include <QQueue>
#include <QWidget>
#include <cstdint>

class RegisterTable;
class SerialManager;
class QPushButton;

class RegisterRwTab : public QWidget {
    Q_OBJECT

public:
    explicit RegisterRwTab(SerialManager *serialManager, QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);

private slots:
    void onReadRowRequested(int globalRow, quint16 addr);
    void onWriteRowRequested(int globalRow, quint16 addr, quint16 value);
    void onReadAllClicked();
    void onWriteAllClicked();
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void onSerialError(const QString &message);
    void onConfigChanged();

private:
    void setupUi();
    void connectSignals();
    void processNextInQueue();
    QString configFilePath() const;

    SerialManager  *m_serialManager  = nullptr;
    RegisterTable  *m_registerTable  = nullptr;
    QPushButton    *m_readAllButton  = nullptr;
    QPushButton    *m_writeAllButton = nullptr;

    QQueue<int> m_pendingQueue;     // 待处理的 globalRow 列表
    int         m_pendingRow = -1;  // 当前正在处理的行（-1 表示无）
    bool        m_isWriteOp = false; // true = 写操作批次，false = 读操作批次
};
```

---

### 5. src/tabs/registerrwtab.cpp — 关键逻辑

#### 5.1 构造

```cpp
RegisterRwTab::RegisterRwTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent), m_serialManager(serialManager) {
    setupUi();
    connectSignals();
    // 启动时加载配置
    m_registerTable->loadConfig(configFilePath());
}
```

#### 5.2 setupUi

Sidebar（使用已有 Sidebar 组件）的 sidebarContent 中放两个按钮：

```cpp
// 全部读取按钮（绿色系，与连接按钮同风格）
m_readAllButton = new QPushButton(tr("全部读取"), sidebarContent);
m_readAllButton->setFixedHeight(Style::Size::SidebarButtonHeight);
m_readAllButton->setStyleSheet(/* 与 ConfigTab 连接按钮相同的绿色样式 */);

// 全部写入按钮（橙色系）
m_writeAllButton = new QPushButton(tr("全部写入"), sidebarContent);
m_writeAllButton->setFixedHeight(Style::Size::SidebarButtonHeight);
m_writeAllButton->setStyleSheet(QStringLiteral(
    "QPushButton { background:%1; border:0.5px solid %2; color:%2; font-size:11px; border-radius:5px; }"
    "QPushButton:hover { background:%2; color:#fff; }"
    "QPushButton:disabled { background:#e8e8e8; border:0.5px solid #ccc; color:#aaa; }")
    .arg(Style::Color::WriteButtonBackground.name())
    .arg(Style::Color::WriteButtonForeground.name()));

sidebarLayout->addWidget(m_readAllButton);
sidebarLayout->addWidget(m_writeAllButton);
sidebarLayout->addStretch();
```

右侧主区域放 `m_registerTable`（QVBoxLayout，无内边距）。

#### 5.3 connectSignals

```cpp
void RegisterRwTab::connectSignals() {
    connect(m_registerTable, &RegisterTable::readRowRequested,
            this, &RegisterRwTab::onReadRowRequested);
    connect(m_registerTable, &RegisterTable::writeRowRequested,
            this, &RegisterRwTab::onWriteRowRequested);
    connect(m_registerTable, &RegisterTable::configChanged,
            this, &RegisterRwTab::onConfigChanged);
    connect(m_readAllButton,  &QPushButton::clicked, this, &RegisterRwTab::onReadAllClicked);
    connect(m_writeAllButton, &QPushButton::clicked, this, &RegisterRwTab::onWriteAllClicked);
    connect(m_serialManager, &SerialManager::frameReceived,
            this, &RegisterRwTab::onFrameReceived);
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &RegisterRwTab::onSerialError);
}
```

#### 5.4 单行读 / 写

```cpp
void RegisterRwTab::onReadRowRequested(int globalRow, quint16 addr) {
    m_pendingQueue.clear();
    m_pendingRow    = globalRow;
    m_isWriteOp     = false;
    QByteArray data;
    data.append(quint8(addr >> 8));
    data.append(quint8(addr & 0xFF));
    m_serialManager->sendCommand(0x20, data);
}

void RegisterRwTab::onWriteRowRequested(int globalRow, quint16 addr, quint16 value) {
    m_pendingQueue.clear();
    m_pendingRow = globalRow;
    m_isWriteOp  = true;
    QByteArray data;
    data.append(quint8(addr  >> 8));
    data.append(quint8(addr  & 0xFF));
    data.append(quint8(value >> 8));
    data.append(quint8(value & 0xFF));
    m_serialManager->sendCommand(0x21, data);
}
```

#### 5.5 全部读 / 全部写

```cpp
void RegisterRwTab::onReadAllClicked() {
    m_pendingQueue.clear();
    m_isWriteOp = false;
    for (int i = 0; i < 16; ++i) {
        if (m_registerTable->rowHasAddress(i))
            m_pendingQueue.enqueue(i);
    }
    processNextInQueue();
}

void RegisterRwTab::onWriteAllClicked() {
    m_pendingQueue.clear();
    m_isWriteOp = true;
    for (int i = 0; i < 16; ++i) {
        if (m_registerTable->rowHasAddress(i) && m_registerTable->rowHasValue(i))
            m_pendingQueue.enqueue(i);
    }
    processNextInQueue();
}

void RegisterRwTab::processNextInQueue() {
    if (m_pendingQueue.isEmpty()) {
        m_pendingRow = -1;
        return;
    }
    m_pendingRow = m_pendingQueue.dequeue();
    quint16 addr = m_registerTable->rowAddress(m_pendingRow);
    QByteArray data;
    data.append(quint8(addr >> 8));
    data.append(quint8(addr & 0xFF));
    if (m_isWriteOp) {
        quint16 val = m_registerTable->rowValue(m_pendingRow);
        data.append(quint8(val >> 8));
        data.append(quint8(val & 0xFF));
        m_serialManager->sendCommand(0x21, data);
    } else {
        m_serialManager->sendCommand(0x20, data);
    }
}
```

#### 5.6 响应处理

```cpp
void RegisterRwTab::onFrameReceived(uint8_t cmd, uint8_t /*seq*/, const QByteArray &data) {
    if (m_pendingRow < 0) return;

    if (cmd == 0x20 && !m_isWriteOp) {
        // 读响应：2字节值
        if (data.size() >= 2) {
            quint16 value = (quint8(data[0]) << 8) | quint8(data[1]);
            m_registerTable->updateRowValue(m_pendingRow, value);
        } else {
            m_registerTable->markRowError(m_pendingRow);
        }
        processNextInQueue();
    } else if (cmd == 0x21 && m_isWriteOp) {
        // 写成功响应：LEN=0，直接继续
        processNextInQueue();
    } else if (cmd == 0x01) {
        // 错误响应（来自 STM32）
        m_registerTable->markRowError(m_pendingRow);
        processNextInQueue();
    }
}

void RegisterRwTab::onSerialError(const QString &/*message*/) {
    if (m_pendingRow >= 0) {
        m_registerTable->markRowError(m_pendingRow);
    }
    m_pendingRow = -1;
    m_pendingQueue.clear();
}
```

#### 5.7 setConnected

```cpp
void RegisterRwTab::setConnected(bool connected) {
    m_readAllButton->setEnabled(connected);
    m_writeAllButton->setEnabled(connected);
    m_registerTable->setConnected(connected);
    if (!connected) {
        m_pendingRow = -1;
        m_pendingQueue.clear();
    }
}
```

#### 5.8 持久化路径 / 自动保存

```cpp
QString RegisterRwTab::configFilePath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/registers.json";
}

void RegisterRwTab::onConfigChanged() {
    m_registerTable->saveConfig(configFilePath());
}
```

---

### 6. src/mainwindow.h — 新增成员

```cpp
#include <QWidget>  // 已有

class RegisterRwTab;  // 前向声明（新增）

private:
    RegisterRwTab *m_registerTab = nullptr;  // 新增
```

---

### 7. src/mainwindow.cpp — 修改

#### 7.1 创建 RegisterRwTab 时传入 SerialManager

```cpp
// 旧：
m_contentStack->addWidget(new RegisterRwTab(m_contentStack));

// 新：
m_registerTab = new RegisterRwTab(m_serialManager, m_contentStack);
m_contentStack->addWidget(m_registerTab);
```

加入头文件：`#include "tabs/registerrwtab.h"`（已有，确认即可）

#### 7.2 连接 tab 禁用信号

在构造函数信号槽区域（现有连接后面）追加：

```cpp
// 非配置页：未连接时禁用
connect(m_configTab, &ConfigTab::serialConnected, this, [this](const QString &, qint32) {
    m_registerTab->setConnected(true);
    m_contentStack->widget(ActivityBar::FwFlashPage)->setEnabled(true);
    m_contentStack->widget(ActivityBar::ScopePage)->setEnabled(true);
});
connect(m_configTab, &ConfigTab::serialDisconnected, this, [this]() {
    m_registerTab->setConnected(false);
    m_contentStack->widget(ActivityBar::FwFlashPage)->setEnabled(false);
    m_contentStack->widget(ActivityBar::ScopePage)->setEnabled(false);
});
```

> 注意：`ActivityBar::FwFlashPage` 和 `ActivityBar::ScopePage` 是 ActivityBar 中定义的枚举值，若名称不同请自行查找对应索引。

#### 7.3 初始状态

MainWindow 构造末尾（信号槽之后）：

```cpp
// 初始未连接：禁用非配置页
m_registerTab->setConnected(false);
m_contentStack->widget(ActivityBar::FwFlashPage)->setEnabled(false);
m_contentStack->widget(ActivityBar::ScopePage)->setEnabled(false);
```

---

### 8. CMakeLists.txt

在 `qt_add_executable` 列表中新增：

```cmake
src/widgets/registertable.cpp
src/widgets/registertable.h
```

所需 Qt 模块已有（Core、Widgets）；`QStandardPaths` 在 Qt6::Core 中，无需额外添加。

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 无错误
2. **表格结构** — 4 行 × 20 列，分 4 组，每组表头显示"描述/地址/值/读/写"
3. **组分隔线** — 第 5、10、15 列左侧有 2px #aaa 竖线
4. **行高 / 表头高** — 行高 19px，表头高 24px
5. **未连接状态** — 启动后读 / 写 / 全部读取 / 全部写入按钮均 disabled；烧录、示波器页面 disabled
6. **已连接状态** — 串口连接后上述按钮恢复 enabled
7. **单行读** — 点击某行"读"按钮，发送 0x20 命令，收到响应后值列显示大写十六进制（如 "0064"），颜色蓝色
8. **单行写** — 在值列输入数值后点击"写"，发送 0x21 命令；成功后值列内容不变
9. **读取失败** — 命令超时或收到错误响应时，对应行值列显示"--"，颜色红色
10. **全部读取** — 点击"全部读取"，对所有有地址的行逐行发出 0x20，按顺序更新值列
11. **全部写入** — 点击"全部写入"，对所有有地址+值的行逐行发出 0x21
12. **持久化加载** — 关闭并重启程序后，之前填写的描述和地址自动恢复
13. **持久化保存** — 修改描述或地址后立即写入 JSON 文件（不需要手动保存）
14. **冻结文件未修改** — SerialManager / FrameParser 等冻结文件无改动

## Notes

- `GroupDividerDelegate` 定义在 `registertable.cpp` 内部，不需要单独头文件
- `rowHasValue()` 判断：值列文字非空且可解析为有效十六进制数
- 地址列输入格式：接受 "0010" 或 "0x0010"（`toUShort(&ok, 16)` 对两者均可正确解析）
- `QTableWidget::blockSignals(true/false)` 在 loadConfig 期间必须使用，防止触发多次 configChanged → saveConfig 循环
- ActivityBar 的页面枚举名称（`FwFlashPage` / `ScopePage`）请工程师确认实际名称后使用正确的索引
- `QStandardPaths` 需要 `#include <QStandardPaths>` 和 `#include <QDir>`
