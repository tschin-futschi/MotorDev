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
#include "services/dw9786oisresetservice.h"
#include "services/fwflashservice.h"
#include "services/generatorservice.h"
#include "services/scopeservice.h"
#include "tabs/configtab.h"
#include "tabs/flashstoragetab.h"
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
#include <QMessageBox>
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
    m_oisResetService = new Dw9786OisResetService(m_serialManager, 0x24, this);

    setupUi();
    connectSignals();

    // 产品决策（2026-05-21）：连接状态不再门控页面可用性，所有 Tab 默认全部启用，
    // 便于 UI 浏览、离线调试和 stub 功能开发。具体业务命令在串口未连接时由
    // CommandDispatcher 自动落到本地错误回调（参见 CommandDispatcher::onSerialError）。
    // design_spec.md 已同步修订对应条目。
    m_registerTab->setConnected(false);
    m_registerTab->setEnabled(true);
    m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
    m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
    m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
    m_activityBar->setPageEnabled(ActivityBar::FlashStoragePage, true);
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

    // 烧录改用 STM32 本地 ISP（协议 0x32~0x37），IC 7-bit 地址由 STM32 端
    // ISP 驱动内部约定，不再由上位机传入。
    m_fwFlashTab = new FwFlashTab(m_serialManager, m_deviceContext, m_contentStack);
    m_contentStack->addWidget(m_fwFlashTab);  // index 2: 固件烧录页

    m_scopeTab = new OscilloscopTab(m_serialManager, m_dispatcher, m_contentStack);
    m_contentStack->addWidget(m_scopeTab);    // index 3: 示波器页

    // STM32 自身 FLASH 文件存储页（协议 v2.10 / 0x39~0x3E）
    m_flashStorageTab = new FlashStorageTab(m_serialManager, m_contentStack);
    m_contentStack->addWidget(m_flashStorageTab); // index 4: FLASH 文件存储页

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

    // --- 设备主动上报的非配对帧（0x06 调试信息 / 0x0B 启动状态报告 / 0x38 EXEC 进度）---
    // SEQ 固定 0xFF，由 STM32 主动发送，PC 无需响应。
    connect(m_dispatcher, &CommandDispatcher::unsolicitedFrameReceived, this,
            [this](uint8_t cmd, uint8_t seq, const QByteArray &data) {
                Q_UNUSED(seq);
                if (cmd == MotorProtocol::CmdDebugInfo) {
                    qCWarning(lcMainWindow).noquote()
                        << QStringLiteral("Device: %1").arg(QString::fromUtf8(data));
                    return;
                }
                if (cmd == MotorProtocol::CmdFlashExecProgress) {
                    // 0x38 EXEC 进度事件帧：STM32 在 EXEC 期间于 ISP 写入循环中主动上报。
                    // FwFlashService 为 FwFlashTab 的子对象（QObject parent），与既有
                    // ScopeService 等的查找模式一致：findChild 拿到后通过 QueuedConnection
                    // 入队调 onFlashExecProgress slot。
                    uint8_t phase = 0xFF;
                    quint32 done = 0;
                    quint32 total = 0;
                    if (!MotorProtocol::decodeFlashExecProgress(data, &phase, &done, &total)) {
                        qCWarning(lcMainWindow).noquote()
                            << QStringLiteral("FLASH_EXEC_PROGRESS payload truncated (size=%1)").arg(data.size());
                        return;
                    }
                    if (auto *svc = m_fwFlashTab ? m_fwFlashTab->findChild<FwFlashService *>() : nullptr) {
                        QMetaObject::invokeMethod(svc,
                                                   "onFlashExecProgress",
                                                   Qt::QueuedConnection,
                                                   Q_ARG(quint8, phase),
                                                   Q_ARG(quint32, done),
                                                   Q_ARG(quint32, total));
                    }
                    return;
                }
                if (cmd == MotorProtocol::CmdBootStatus) {
                    uint8_t status = 0xFF;
                    if (!MotorProtocol::decodeBootStatusResponse(data, &status)) {
                        qCWarning(lcMainWindow).noquote()
                            << QStringLiteral("BOOT_STATUS frame payload truncated (size=%1)").arg(data.size());
                        return;
                    }
                    const QString description = QString::fromUtf8(MotorProtocol::bootStatusDescription(status));
                    const QString line = QStringLiteral("BOOT_STATUS %1 (0x%2): %3")
                        .arg(QString::fromLatin1(MotorProtocol::bootStatusName(status)))
                        .arg(status, 2, 16, QLatin1Char('0'))
                        .arg(description);

                    // 1. TopBar 徽章更新
                    if (m_topBar != nullptr) {
                        m_topBar->setMcuBootState(status, description);
                    }

                    if (status == static_cast<uint8_t>(MotorProtocol::BootStatusCode::Ok)) {
                        qCInfo(lcMainWindow).noquote() << line;
                        m_mcuFailureNotified = false;
                        return;
                    }

                    // INIT_FAIL_* / 保留段都视为致命错误：STM32 此后会陷入 LED 快闪死循环，
                    // 不再响应任何控制帧，PC 端应停止下发业务命令并提示用户处理硬件
                    qCCritical(lcMainWindow).noquote() << line;

                    // 2. 同一会话仅弹一次警告框，避免重复打扰
                    if (!m_mcuFailureNotified) {
                        m_mcuFailureNotified = true;
                        QMessageBox::warning(
                            this,
                            tr("STM32 启动失败"),
                            tr("%1\n\nSTM32 已进入 LED 快闪死循环，不再响应任何命令。\n请处理硬件问题后重启 MCU。")
                                .arg(description));
                    }
                    return;
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

    // --- DW9786 上电 OISReset 日志 → 底部日志面板 ---
    connect(m_oisResetService, &Dw9786OisResetService::logMessage, this,
            [this](const QString &line) {
                m_logPanel->appendMessage(QtInfoMsg, QStringLiteral("DW9786"), line);
            });

    // --- 串口连接成功：启用各受限页面 ---
    connect(m_configTab, &ConfigTab::serialConnected, this, [this](const QString &, qint32) {
        m_registerTab->setConnected(true);
        m_registerTab->setEnabled(true);
        m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
        m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashStoragePage, true);
        m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
        m_scopeTab->setEnabled(true);

        // DW9786 上电默认 Sleep（寄存器全读 0xFFFF）：连接成功后自动执行一次
        // OISReset 把 IC 切到工作态。其它 IC 不触发。
        if (m_deviceContext->icType() == MotorIcType::DW9786) {
            m_oisResetService->requestOisReset();
        }
    });

    // --- 串口断开：保持所有页面启用（产品决策：连接状态不门控页面可用性）---
    connect(m_configTab, &ConfigTab::serialDisconnected, this, [this]() {
        m_registerTab->setConnected(false);
        m_registerTab->setEnabled(true);
        m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
        m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashStoragePage, true);
        m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
        m_scopeTab->setEnabled(true);

        // 断开后 MCU 启动状态回到"未知"灰态，并清除已弹警告标志，
        // 让重连后若再次收到 INIT_FAIL_* 能再弹一次
        if (m_topBar != nullptr) {
            m_topBar->resetMcuState();
        }
        m_mcuFailureNotified = false;
    });

    // --- 日志面板展开/收起 ---
    connect(m_logToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_logPanel->isVisible();
        m_logPanel->setVisible(visible);
        m_logToggleButton->setText(visible ? QStringLiteral("▲ 输出")
                                             : QStringLiteral("▼ 输出"));
    });
}
