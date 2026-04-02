# Plan

Stage: statusbar_logpanel
Status: active
Priority: high
Source: Claude Code
Date: 2026-04-02 15:00:00
Depends-On: review_topbar_serial_status_20260402_140000.md (pass)
Supersedes: none

## Goal

重新设计底部状态栏内容，并新增从底部向上展开的日志面板：
1. 状态栏左侧显示软件名称 + License
2. 状态栏中间显示固件版本 + 编译日期（当前硬编码占位）
3. 状态栏右侧显示日志开关按钮
4. 日志面板展开时挤压主内容区向上，浅色背景，显示 qDebug/qWarning 输出

## Scope

- 新建 `src/widgets/logpanel.h` / `src/widgets/logpanel.cpp`
- 修改 `src/mainwindow.h` / `src/mainwindow.cpp`
- 修改 `src/main.cpp`（安装全局消息处理器）
- 修改 `CMakeLists.txt`（新增 logpanel 源文件）

## Non-Goals

- 不实现固件版本查询协议（第2条用硬编码占位，后续单独阶段实现）
- 不实现日志持久化（不写文件）
- 不实现日志过滤 / 搜索
- 不修改 TopBar
- 不修改 SerialManager / FrameParser / DeviceContext

## Files-In-Scope

- `src/widgets/logpanel.h`（新建）
- `src/widgets/logpanel.cpp`（新建）
- `src/mainwindow.h`
- `src/mainwindow.cpp`
- `src/main.cpp`
- `CMakeLists.txt`

## Files-Frozen

- `src/serialmanager.h` / `src/serialmanager.cpp`
- `src/frameparser.h` / `src/frameparser.cpp`
- `src/devicecontext.h` / `src/devicecontext.cpp`
- `src/ui/style_constants.h`
- `src/widgets/topbar.h` / `src/widgets/topbar.cpp`
- `src/widgets/activitybar.h` / `src/widgets/activitybar.cpp`
- `src/tabs/*.h` / `src/tabs/*.cpp`

## Constraints

- `qInstallMessageHandler` 需要一个静态 C 函数，不能直接用 lambda；通过静态指针 `LogPanel::s_instance` 转发到实例
- `appendMessage` 必须线程安全（通过 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 调用）
- 日志面板初始隐藏，展开时在布局中占位（挤压主内容区），不使用浮动叠加
- 日志面板高度固定 200px，不可调整大小（暂不做 splitter）

---

## 详细设计

### 1. CMakeLists.txt

在 `qt_add_executable` 列表中新增：
```cmake
src/widgets/logpanel.cpp
src/widgets/logpanel.h
```

---

### 2. src/widgets/logpanel.h

```cpp
#pragma once

#include <QWidget>
#include <QtGlobal>

class QPlainTextEdit;
class QPushButton;

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);

    static LogPanel *instance() { return s_instance; }

public slots:
    void appendMessage(QtMsgType type, const QString &msg);

private:
    QPlainTextEdit *m_textEdit = nullptr;
    static LogPanel *s_instance;
};
```

---

### 3. src/widgets/logpanel.cpp

```cpp
#include "widgets/logpanel.h"

#include "ui/style_constants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

LogPanel *LogPanel::s_instance = nullptr;

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent) {
    s_instance = this;
    setFixedHeight(200);
    setStyleSheet(QStringLiteral("background:%1; border-top:1px solid %2;")
                      .arg(MotorDev::Style::Color::PanelBackground.name())
                      .arg(MotorDev::Style::Color::DefaultBorder.name()));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 4, 8, 4);
    rootLayout->setSpacing(4);

    // 标题行
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto *titleLabel = new QLabel(tr("输出"), this);
    titleLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:500;")
                                  .arg(MotorDev::Style::Color::AppText.name()));
    auto *clearButton = new QPushButton(tr("清空"), this);
    clearButton->setFixedHeight(20);
    clearButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; border:none; color:%1; font-size:11px; }"
        "QPushButton:hover { color:%2; }")
                                   .arg(MotorDev::Style::Color::MutedText.name())
                                   .arg(MotorDev::Style::Color::AppText.name()));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(clearButton);

    // 文本区
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setMaximumBlockCount(500);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background:%1; border:none; color:%2; font-family:Consolas,monospace; font-size:11px; }")
                                  .arg(MotorDev::Style::Color::PanelBackground.name())
                                  .arg(MotorDev::Style::Color::AppText.name()));

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(m_textEdit);

    connect(clearButton, &QPushButton::clicked, m_textEdit, &QPlainTextEdit::clear);
}

void LogPanel::appendMessage(QtMsgType type, const QString &msg) {
    QString prefix;
    QString color;
    switch (type) {
    case QtWarningMsg:
        prefix = QStringLiteral("[WARN] ");
        color  = QStringLiteral("#B85C00");
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        prefix = QStringLiteral("[ERR]  ");
        color  = QStringLiteral("#C0392B");
        break;
    default:
        prefix = QStringLiteral("[DBG]  ");
        color  = MotorDev::Style::Color::MutedText.name();
        break;
    }

    const QString html = QStringLiteral("<span style=\"color:%1;\">%2%3</span>")
                             .arg(color)
                             .arg(prefix.toHtmlEscaped())
                             .arg(msg.toHtmlEscaped());
    m_textEdit->appendHtml(html);
}
```

---

### 4. src/main.cpp — 安装消息处理器

在 `main()` 中，`MainWindow` 创建之后，安装全局消息处理器：

```cpp
#include "widgets/logpanel.h"
#include <QMessageLogContext>

static void motorDevMessageHandler(QtMsgType type,
                                   const QMessageLogContext &,
                                   const QString &msg) {
    if (LogPanel::instance() != nullptr) {
        QMetaObject::invokeMethod(
            LogPanel::instance(),
            "appendMessage",
            Qt::QueuedConnection,
            Q_ARG(QtMsgType, type),
            Q_ARG(QString, msg));
    }
}
```

在 `main()` 中 `w.show()` 之前调用：
```cpp
qInstallMessageHandler(motorDevMessageHandler);
```

---

### 5. src/mainwindow.h — 新增成员

```cpp
class LogPanel;   // 前向声明

private:
    LogPanel *m_logPanel = nullptr;       // 新增
    QPushButton *m_logToggleButton = nullptr;  // 新增
```

---

### 6. src/mainwindow.cpp — 状态栏重构 + 日志面板

#### 6.1 主布局调整

主布局（rootLayout）结构变为：
```
[TopBar]
[MiddleWidget]  ← stretch(1)
[LogPanel]      ← 初始隐藏
[StatusBar]     ← fixed 22px
```

在 `rootLayout->addWidget(m_statusBarWidget)` 之前插入：
```cpp
m_logPanel = new LogPanel(central);
m_logPanel->setVisible(false);
rootLayout->addWidget(m_logPanel);
```

#### 6.2 状态栏内容重构

替换原有状态栏内容（清空 `statusLayout` 中的内容），改为：

```cpp
// 左：软件名 + License
auto *softwareLabel = new QLabel(
    QStringLiteral("MotorDev · MIT License"), m_statusBarWidget);
softwareLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                 .arg(Style::Color::StatusBarText.name()));

// 中：固件信息（硬编码占位）
auto *firmwareLabel = new QLabel(
    QStringLiteral("固件 v0.0.0 · 编译日期 2026-01-01"), m_statusBarWidget);
firmwareLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                 .arg(Style::Color::StatusBarText.name()));
firmwareLabel->setAlignment(Qt::AlignCenter);

// 右：日志开关按钮
m_logToggleButton = new QPushButton(QStringLiteral("▼ 输出"), m_statusBarWidget);
m_logToggleButton->setFlat(true);
m_logToggleButton->setFixedHeight(18);
m_logToggleButton->setStyleSheet(QStringLiteral(
    "QPushButton { background:transparent; border:none; color:%1; font-size:11px; }"
    "QPushButton:hover { color:%2; }")
                                     .arg(Style::Color::StatusBarText.name())
                                     .arg(Style::Color::LightGreen.name()));

statusLayout->addWidget(softwareLabel);
statusLayout->addStretch();
statusLayout->addWidget(firmwareLabel);
statusLayout->addStretch();
statusLayout->addWidget(m_logToggleButton);
```

#### 6.3 日志面板开关逻辑

在构造函数末尾（信号槽区域）新增：

```cpp
connect(m_logToggleButton, &QPushButton::clicked, this, [this]() {
    const bool visible = !m_logPanel->isVisible();
    m_logPanel->setVisible(visible);
    m_logToggleButton->setText(visible ? QStringLiteral("▲ 输出")
                                       : QStringLiteral("▼ 输出"));
});
```

---

## Acceptance

1. **编译通过** — `mingw32-make -C build_make_qt -j4` 成功
2. **状态栏左侧** — 显示 `"MotorDev · MIT License"`
3. **状态栏中间** — 显示 `"固件 v0.0.0 · 编译日期 2026-01-01"`
4. **状态栏右侧** — 显示 `"▼ 输出"` 按钮
5. **日志面板展开** — 点击按钮后日志面板从底部展开（200px），主内容区被挤压，按钮文字变为 `"▲ 输出"`
6. **日志面板收起** — 再次点击后面板隐藏，主内容区恢复，按钮文字变回 `"▼ 输出"`
7. **日志输出显示** — 程序运行期间的 qDebug 输出（如 "I2C scan started"）出现在日志面板中
8. **qWarning 颜色区分** — 警告信息以橙色显示，普通 debug 以灰色显示
9. **清空按钮** — 点击 "清空" 后日志面板内容清除
10. **冻结文件未修改**

## Notes

- `LogPanel::s_instance` 为静态指针，消息处理器通过它转发；`QMetaObject::invokeMethod` 确保跨线程安全
- 消息处理器安装在 `main.cpp` 的 `w.show()` 之前，避免在 LogPanel 创建前有消息丢失（LogPanel 在 MainWindow 构造时创建，`show()` 之前已就绪）
- 固件版本信息（中间区域）后续需要时通过新协议命令实现，届时将硬编码替换为动态更新
