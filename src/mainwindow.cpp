// =============================================================================
// @file    mainwindow.cpp
// @brief   主窗口实现
//
// 构建应用程序主界面布局，初始化核心服务，建立页面切换、串口连接状态
// 和示波器控件的信号槽连接。
// =============================================================================

#include "mainwindow.h"

#include "devicecontext.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "services/commanddispatcher.h"
#include "services/cyclicwriteservice.h"
#include "services/generatorservice.h"
#include "services/scopeservice.h"
#include "tabs/configtab.h"
#include "tabs/fwflashtab.h"
#include "tabs/oscilloscoptab.h"
#include "tabs/registerrwtab.h"
#include "tabs/serialdebugtab.h"
#include "ui/style_constants.h"
#include "widgets/activitybar.h"
#include "widgets/logpanel.h"
#include "widgets/topbar.h"

#include <QPointer>

#include <QDebug>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

using namespace MotorDev;

Q_LOGGING_CATEGORY(lcMainWindow, "motordev.main")

// -----------------------------------------------------------------------------
// 构造与析构
// -----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("MotorDev"));

    // 自适应屏幕尺寸：默认窗口大小与最小窗口大小都不允许超过当前屏幕可用区域
    // （减去 40 像素留出任务栏 / 边框余量）。在小屏笔记本上启动时窗口仍能完整显示，
    // 不会因为默认 1280×800 / minimum 1024×600 大于屏幕物理像素而溢出屏幕。
    QScreen *screen = QGuiApplication::primaryScreen();
    const QSize available = screen != nullptr
                                ? screen->availableGeometry().size()
                                : QSize(Style::Size::WindowWidth, Style::Size::WindowHeight);
    const int safeW = std::max(0, available.width() - 40);
    const int safeH = std::max(0, available.height() - 40);

    const int prefW = std::min(Style::Size::WindowWidth, safeW);
    const int prefH = std::min(Style::Size::WindowHeight, safeH);
    const int minW = std::min(Style::Size::MinWindowWidth, prefW);
    const int minH = std::min(Style::Size::MinWindowHeight, prefH);

    resize(prefW, prefH);
    setMinimumSize(minW, minH);

    // 初始化核心服务
    m_serialManager = new SerialManager(this);
    m_dispatcher = new CommandDispatcher(m_serialManager, this);
    m_deviceContext = new DeviceContext(this);

    setupUi();
    connectSignals();

    // 临时：解除所有页面的串口未连接锁定，便于 UI 浏览/调试
    m_registerTab->setConnected(false);
    m_registerTab->setEnabled(true);
    m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
    m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
    m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
    m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
    m_scopeTab->setEnabled(true);
}

MainWindow::~MainWindow() {
    // SerialManager 在独立线程运行，需在主窗口销毁前手动删除
    delete m_serialManager;
    m_serialManager = nullptr;
}

// -----------------------------------------------------------------------------
// UI 构建
// -----------------------------------------------------------------------------

/// @brief 构建主窗口的完整 UI 布局
///
/// 布局结构（从上到下）：
/// ┌─────────────────────────────────┐
/// │           TopBar                │  顶栏
/// ├────┬────────────────────────────┤
/// │    │                            │
/// │ AB │      ContentStack          │  活动栏 + 内容堆叠区
/// │    │                            │
/// ├────┴────────────────────────────┤
/// │           LogPanel              │  日志面板（默认隐藏）
/// ├─────────────────────────────────┤
/// │           StatusBar             │  状态栏
/// └─────────────────────────────────┘
void MainWindow::setupUi() {
    // --- 根容器 ---
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setObjectName(QStringLiteral("centralWidget"));
    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // --- 顶栏 ---
    m_topBar = new TopBar(centralWidget);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    rootLayout->addWidget(m_topBar);

    // --- 中间区域：活动栏 + 内容堆叠区 ---
    auto *middleWidget = new QWidget(centralWidget);
    middleWidget->setObjectName(QStringLiteral("middleWidget"));
    auto *middleLayout = new QHBoxLayout(middleWidget);
    middleLayout->setObjectName(QStringLiteral("middleLayout"));
    middleLayout->setSpacing(0);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(middleWidget);

    m_activityBar = new ActivityBar(middleWidget);
    m_activityBar->setObjectName(QStringLiteral("activityBar"));
    middleLayout->addWidget(m_activityBar);

    m_contentStack = new QStackedWidget(middleWidget);
    m_contentStack->setObjectName(QStringLiteral("contentStack"));
    m_contentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    middleLayout->addWidget(m_contentStack);

    // --- 日志面板 ---
    m_logPanel = new LogPanel(centralWidget);
    m_logPanel->setObjectName(QStringLiteral("logPanel"));
    rootLayout->addWidget(m_logPanel);

    // --- 状态栏 ---
    auto *statusBarWidget = new QWidget(centralWidget);
    statusBarWidget->setObjectName(QStringLiteral("statusBarWidget"));
    statusBarWidget->setMinimumSize(QSize(0, 22));
    statusBarWidget->setMaximumSize(QSize(QWIDGETSIZE_MAX, 22));
    auto *statusBarLayout = new QHBoxLayout(statusBarWidget);
    statusBarLayout->setObjectName(QStringLiteral("statusBarLayout"));
    statusBarLayout->setSpacing(10);
    statusBarLayout->setContentsMargins(10, 0, 10, 0);
    rootLayout->addWidget(statusBarWidget);

    // 软件信息标签（左侧）
    auto *softwareLabel = new QLabel(statusBarWidget);
    softwareLabel->setObjectName(QStringLiteral("softwareLabel"));
    softwareLabel->setText(QStringLiteral("MotorDev · MIT License"));
    statusBarLayout->addWidget(softwareLabel);
    statusBarLayout->addStretch();

    // 固件版本标签（居中）
    auto *firmwareLabel = new QLabel(statusBarWidget);
    firmwareLabel->setObjectName(QStringLiteral("firmwareLabel"));
    firmwareLabel->setText(tr("固件 v0.0.0 · 编译日期 2026-01-01"));
    firmwareLabel->setAlignment(Qt::AlignCenter);
    statusBarLayout->addWidget(firmwareLabel);
    statusBarLayout->addStretch();

    // 日志面板展开/收起按钮（右侧）
    m_logToggleButton = new QPushButton(statusBarWidget);
    m_logToggleButton->setObjectName(QStringLiteral("logToggleButton"));
    m_logToggleButton->setMinimumSize(QSize(0, 18));
    m_logToggleButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 18));
    m_logToggleButton->setText(QStringLiteral("▼ 输出"));
    m_logToggleButton->setFlat(true);
    statusBarLayout->addWidget(m_logToggleButton);

    // --- 各 Tab 页面注册到 ContentStack ---
    m_configTab = new ConfigTab(m_serialManager, m_dispatcher, m_deviceContext, m_contentStack);
    m_contentStack->addWidget(m_configTab);   // index 0: 配置页

    m_registerTab = new RegisterRwTab(m_dispatcher, m_contentStack);
    m_contentStack->addWidget(m_registerTab); // index 1: 寄存器读写页

    // 烧录链路需要把 configtab 配置的 IC 7-bit 地址同步给真实 AW SDK DLL。
    // 用 QPointer + lambda 实时取 DeviceContext::slaveId()，避免地址被缓存而漂移。
    QPointer<DeviceContext> devCtxPtr(m_deviceContext);
    auto awAddrProvider = [devCtxPtr]() -> uint8_t {
        return devCtxPtr ? devCtxPtr->slaveId() : uint8_t(0);
    };
    m_fwFlashTab = new FwFlashTab(m_serialManager, awAddrProvider, m_contentStack);
    m_contentStack->addWidget(m_fwFlashTab);  // index 2: 固件烧录页

    m_scopeTab = new OscilloscopTab(m_serialManager, m_dispatcher, m_contentStack);
    m_contentStack->addWidget(m_scopeTab);    // index 3: 示波器页

    // 烧录前置序列：从 OscilloscopTab 持有的 service 实例查找并注入回调
    // findChild 依赖各 service 在对应 tab 构造时以 this 作为 QObject parent，
    // 取不到时仅写日志、跳过该步骤（fire-and-forget 语义）
    // PMIC 不在前置序列中关闭：烧录期间 IC 必须保持正常供电
    if (auto *scope = m_scopeTab->findChild<ScopeService *>()) {
        QPointer<ScopeService> p(scope);
        m_fwFlashTab->setStopScopeCallback([p]() { if (p) p->requestStop(); });
    }
    if (auto *gen = m_scopeTab->findChild<GeneratorService *>()) {
        QPointer<GeneratorService> p(gen);
        m_fwFlashTab->setStopGeneratorCallback([p]() { if (p) p->stop(); });
    }
    if (auto *cyclic = m_scopeTab->findChild<CyclicWriteService *>()) {
        QPointer<CyclicWriteService> p(cyclic);
        m_fwFlashTab->setStopCyclicWriteCallback([p]() { if (p) p->stop(); });
    }

    // 串口调试模拟器为浮动窗口，不占 ContentStack
    m_debugTab = new SerialDebugTab(this);
    m_contentStack->setCurrentIndex(ActivityBar::ConfigPage);  // 默认显示配置页
    m_topBar->setScopeControlsVisible(false);   // 非示波器页面时隐藏示波器控件
    m_topBar->setViewMode(0);
    m_logPanel->setVisible(false);              // 日志面板默认隐藏
}

// -----------------------------------------------------------------------------
// 信号槽连接
// -----------------------------------------------------------------------------

/// @brief 连接所有信号槽
///
/// 主要连接关系：
/// - ActivityBar 页面切换 → ContentStack 切换 + TopBar 示波器控件显隐
/// - ConfigTab 串口连接/断开 → TopBar 状态更新 + 各页面启用/禁用
/// - TopBar 示波器控件 ↔ OscilloscopTab 双向同步
/// - SerialDebugTab 调试数据流 → OscilloscopTab 注入
/// - LogToggleButton → LogPanel 显隐切换
void MainWindow::connectSignals() {
    // --- 页面切换 ---
    connect(m_activityBar, &ActivityBar::pageSelected, this, [this](int index) {
        // 切换到示波器页面时显示示波器专属控件
        m_topBar->setScopeControlsVisible(index == ActivityBar::ScopePage);

        // 调试页面为浮动窗口，不切换 ContentStack
        if (index == ActivityBar::DebugPage) {
            m_debugTab->show();
            m_debugTab->raise();
            m_debugTab->activateWindow();
            return;
        }
        m_contentStack->setCurrentIndex(index);
    });

    // --- 串口连接状态 → TopBar 指示灯更新 ---
    connect(m_configTab, &ConfigTab::serialConnected, m_topBar, &TopBar::onSerialConnected);
    connect(m_configTab, &ConfigTab::serialDisconnected, m_topBar, &TopBar::onSerialDisconnected);

    // --- 设备主动上报的调试信息 ---
    connect(m_dispatcher, &CommandDispatcher::unsolicitedFrameReceived, this,
            [](uint8_t cmd, uint8_t seq, const QByteArray &data) {
                Q_UNUSED(seq);
                if (cmd == MotorProtocol::CmdDebugInfo) {
                    qCWarning(lcMainWindow).noquote()
                        << QStringLiteral("Device: %1").arg(QString::fromUtf8(data));
                }
            });

    // --- 示波器控件双向同步：TopBar ↔ OscilloscopTab ---
    connect(m_topBar, &TopBar::viewModeChanged, m_scopeTab, &OscilloscopTab::onViewModeChangeRequested);
    connect(m_topBar, &TopBar::styleToggleRequested, m_scopeTab, &OscilloscopTab::onStyleToggleRequested);
    connect(m_topBar, &TopBar::crosshairToggleRequested, m_scopeTab, &OscilloscopTab::onCrosshairToggleRequested);
    connect(m_scopeTab, &OscilloscopTab::viewModeChanged, m_topBar, &TopBar::setViewMode);

    // --- 调试模拟器数据流 → 示波器 ---
    connect(m_debugTab, &SerialDebugTab::debugStreamGenerated,
            m_scopeTab, &OscilloscopTab::ingestDebugStream);
    connect(m_debugTab, &SerialDebugTab::debugStreamActiveChanged,
            m_scopeTab, &OscilloscopTab::setDebugStreamActive);

    // --- 串口连接成功：启用各受限页面 ---
    connect(m_configTab, &ConfigTab::serialConnected, this, [this](const QString &, qint32) {
        m_registerTab->setConnected(true);
        m_registerTab->setEnabled(true);
        m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
        m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
        m_scopeTab->setEnabled(true);
    });

    // --- 串口断开：临时解除所有页面锁定 ---
    connect(m_configTab, &ConfigTab::serialDisconnected, this, [this]() {
        m_registerTab->setConnected(false);
        m_registerTab->setEnabled(true);
        m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
        m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
        m_scopeTab->setEnabled(true);
    });

    // --- 日志面板展开/收起 ---
    connect(m_logToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_logPanel->isVisible();
        m_logPanel->setVisible(visible);
        m_logToggleButton->setText(visible ? QStringLiteral("▲ 输出")
                                             : QStringLiteral("▼ 输出"));
    });
}
