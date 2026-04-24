#include "mainwindow.h"

#include "devicecontext.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "services/commanddispatcher.h"
#include "tabs/configtab.h"
#include "tabs/fwflashtab.h"
#include "tabs/oscilloscoptab.h"
#include "tabs/registerrwtab.h"
#include "tabs/serialdebugtab.h"
#include "ui/style_constants.h"
#include "widgets/activitybar.h"
#include "widgets/logpanel.h"
#include "widgets/topbar.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace MotorDev;

Q_LOGGING_CATEGORY(lcMainWindow, "motordev.main")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("MotorDev"));
    resize(Style::Size::WindowWidth, Style::Size::WindowHeight);
    setMinimumSize(Style::Size::MinWindowWidth, Style::Size::MinWindowHeight);

    m_serialManager = new SerialManager(this);
    m_dispatcher = new CommandDispatcher(m_serialManager, this);
    m_deviceContext = new DeviceContext(this);

    setupUi();
    connectSignals();

    m_registerTab->setConnected(false);
    m_registerTab->setEnabled(false);
    m_activityBar->setPageEnabled(ActivityBar::RegisterPage, false);
    m_activityBar->setPageEnabled(ActivityBar::FlashPage, false);
    m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
    m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(false);
    m_scopeTab->setEnabled(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setObjectName(QStringLiteral("centralWidget"));
    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_topBar = new TopBar(centralWidget);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    rootLayout->addWidget(m_topBar);

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

    m_logPanel = new LogPanel(centralWidget);
    m_logPanel->setObjectName(QStringLiteral("logPanel"));
    rootLayout->addWidget(m_logPanel);

    auto *statusBarWidget = new QWidget(centralWidget);
    statusBarWidget->setObjectName(QStringLiteral("statusBarWidget"));
    statusBarWidget->setMinimumSize(QSize(0, 22));
    statusBarWidget->setMaximumSize(QSize(QWIDGETSIZE_MAX, 22));
    auto *statusBarLayout = new QHBoxLayout(statusBarWidget);
    statusBarLayout->setObjectName(QStringLiteral("statusBarLayout"));
    statusBarLayout->setSpacing(10);
    statusBarLayout->setContentsMargins(10, 0, 10, 0);
    rootLayout->addWidget(statusBarWidget);

    auto *softwareLabel = new QLabel(statusBarWidget);
    softwareLabel->setObjectName(QStringLiteral("softwareLabel"));
    softwareLabel->setText(QStringLiteral("MotorDev · MIT License"));
    statusBarLayout->addWidget(softwareLabel);
    statusBarLayout->addStretch();

    auto *firmwareLabel = new QLabel(statusBarWidget);
    firmwareLabel->setObjectName(QStringLiteral("firmwareLabel"));
    firmwareLabel->setText(tr("固件 v0.0.0 · 编译日期 2026-01-01"));
    firmwareLabel->setAlignment(Qt::AlignCenter);
    statusBarLayout->addWidget(firmwareLabel);
    statusBarLayout->addStretch();

    m_logToggleButton = new QPushButton(statusBarWidget);
    m_logToggleButton->setObjectName(QStringLiteral("logToggleButton"));
    m_logToggleButton->setMinimumSize(QSize(0, 18));
    m_logToggleButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 18));
    m_logToggleButton->setText(QStringLiteral("▼ 输出"));
    m_logToggleButton->setFlat(true);
    statusBarLayout->addWidget(m_logToggleButton);

    m_configTab = new ConfigTab(m_serialManager, m_dispatcher, m_deviceContext, m_contentStack);
    m_contentStack->addWidget(m_configTab);

    m_registerTab = new RegisterRwTab(m_dispatcher, m_contentStack);
    m_contentStack->addWidget(m_registerTab);

    m_contentStack->addWidget(new FwFlashTab(m_contentStack));

    m_scopeTab = new OscilloscopTab(m_serialManager, m_dispatcher, m_contentStack);
    m_contentStack->addWidget(m_scopeTab);

    m_debugTab = new SerialDebugTab(this);
    m_contentStack->setCurrentIndex(ActivityBar::ConfigPage);
    m_topBar->setScopeControlsVisible(false);
    m_topBar->setViewMode(0);
    m_logPanel->setVisible(false);
}

void MainWindow::connectSignals() {
    connect(m_activityBar, &ActivityBar::pageSelected, this, [this](int index) {
        m_topBar->setScopeControlsVisible(index == ActivityBar::ScopePage);
        if (index == ActivityBar::DebugPage) {
            m_debugTab->show();
            m_debugTab->raise();
            m_debugTab->activateWindow();
            return;
        }
        m_contentStack->setCurrentIndex(index);
    });

    connect(m_configTab, &ConfigTab::serialConnected, m_topBar, &TopBar::onSerialConnected);
    connect(m_configTab, &ConfigTab::serialDisconnected, m_topBar, &TopBar::onSerialDisconnected);
    connect(m_dispatcher, &CommandDispatcher::unsolicitedFrameReceived, this,
            [](uint8_t cmd, uint8_t seq, const QByteArray &data) {
                Q_UNUSED(seq);
                if (cmd == MotorProtocol::CmdDebugInfo) {
                    qCWarning(lcMainWindow).noquote()
                        << QStringLiteral("Device: %1").arg(QString::fromUtf8(data));
                }
            });
    connect(m_topBar, &TopBar::viewModeChanged, m_scopeTab, &OscilloscopTab::onViewModeChangeRequested);
    connect(m_topBar, &TopBar::styleToggleRequested, m_scopeTab, &OscilloscopTab::onStyleToggleRequested);
    connect(m_scopeTab, &OscilloscopTab::viewModeChanged, m_topBar, &TopBar::setViewMode);
    connect(m_debugTab, &SerialDebugTab::debugStreamGenerated,
            m_scopeTab, &OscilloscopTab::ingestDebugStream);
    connect(m_debugTab, &SerialDebugTab::debugStreamActiveChanged,
            m_scopeTab, &OscilloscopTab::setDebugStreamActive);

    connect(m_configTab, &ConfigTab::serialConnected, this, [this](const QString &, qint32) {
        m_registerTab->setConnected(true);
        m_registerTab->setEnabled(true);
        m_activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
        m_activityBar->setPageEnabled(ActivityBar::FlashPage, true);
        m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
        m_scopeTab->setEnabled(true);
    });

    connect(m_configTab, &ConfigTab::serialDisconnected, this, [this]() {
        m_registerTab->setConnected(false);
        m_registerTab->setEnabled(false);
        m_activityBar->setPageEnabled(ActivityBar::RegisterPage, false);
        m_activityBar->setPageEnabled(ActivityBar::FlashPage, false);
        m_activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        m_contentStack->widget(ActivityBar::FlashPage)->setEnabled(false);
        m_scopeTab->setEnabled(true);
    });

    connect(m_logToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_logPanel->isVisible();
        m_logPanel->setVisible(visible);
        m_logToggleButton->setText(visible ? QStringLiteral("▲ 输出")
                                             : QStringLiteral("▼ 输出"));
    });
}
