#include "mainwindow.h"

#include "devicecontext.h"
#include "serialmanager.h"
#include "tabs/configtab.h"
#include "tabs/fwflashtab.h"
#include "tabs/oscilloscoptab.h"
#include "tabs/registerrwtab.h"
#include "ui/style_constants.h"
#include "widgets/activitybar.h"
#include "widgets/logpanel.h"
#include "widgets/topbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace MotorDev;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("MotorDev"));
    resize(Style::Size::WindowWidth, Style::Size::WindowHeight);
    setMinimumSize(Style::Size::MinWindowWidth, Style::Size::MinWindowHeight);

    m_serialManager = new SerialManager(this);
    m_deviceContext = new DeviceContext(this);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(
        Style::Size::OuterMargin,
        Style::Size::OuterMargin,
        Style::Size::OuterMargin,
        Style::Size::OuterMargin);
    rootLayout->setSpacing(Style::Size::MainSpacing);

    m_topBar = new TopBar(central);

    auto *middleWidget = new QWidget(central);
    auto *middleLayout = new QHBoxLayout(middleWidget);
    middleLayout->setContentsMargins(
        Style::Size::OuterMargin,
        Style::Size::OuterMargin,
        Style::Size::OuterMargin,
        Style::Size::OuterMargin);
    middleLayout->setSpacing(Style::Size::MainSpacing);

    m_activityBar = new ActivityBar(middleWidget);
    m_contentStack = new QStackedWidget(middleWidget);
    m_configTab = new ConfigTab(m_serialManager, m_deviceContext, m_contentStack);
    m_contentStack->addWidget(m_configTab);
    m_contentStack->addWidget(new RegisterRwTab(m_contentStack));
    m_contentStack->addWidget(new FwFlashTab(m_contentStack));
    m_contentStack->addWidget(new OscilloscopTab(m_contentStack));
    m_contentStack->setCurrentIndex(ActivityBar::ConfigPage);
    m_contentStack->setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::WindowBackground.name()));

    middleLayout->addWidget(m_activityBar);
    middleLayout->addWidget(m_contentStack, 1);

    m_statusBarWidget = new QWidget(central);
    m_statusBarWidget->setFixedHeight(Style::Size::StatusBarHeight);
    auto *statusLayout = new QHBoxLayout(m_statusBarWidget);
    statusLayout->setContentsMargins(
        Style::Size::StatusBarHorizontalPadding,
        Style::Size::OuterMargin,
        Style::Size::StatusBarHorizontalPadding,
        Style::Size::OuterMargin);
    statusLayout->setSpacing(Style::Size::StatusBarSpacing);

    auto *softwareLabel = new QLabel(QStringLiteral("MotorDev · MIT License"), m_statusBarWidget);
    softwareLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                     .arg(Style::Color::StatusBarText.name()));

    auto *firmwareLabel = new QLabel(QStringLiteral("固件 v0.0.0 · 编译日期 2026-01-01"), m_statusBarWidget);
    firmwareLabel->setAlignment(Qt::AlignCenter);
    firmwareLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                     .arg(Style::Color::StatusBarText.name()));

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

    m_statusBarWidget->setStyleSheet(QStringLiteral("background:%1;")
                                         .arg(Style::Color::PrimaryGreen.name()));

    m_logPanel = new LogPanel(central);
    m_logPanel->setVisible(false);

    rootLayout->addWidget(m_topBar);
    rootLayout->addWidget(middleWidget, 1);
    rootLayout->addWidget(m_logPanel);
    rootLayout->addWidget(m_statusBarWidget);

    central->setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::WindowBackground.name()));
    setCentralWidget(central);

    connect(m_activityBar, &ActivityBar::pageSelected, m_contentStack, &QStackedWidget::setCurrentIndex);
    connect(m_configTab, &ConfigTab::serialConnected, m_topBar, &TopBar::onSerialConnected);
    connect(m_configTab, &ConfigTab::serialDisconnected, m_topBar, &TopBar::onSerialDisconnected);
    connect(m_logToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_logPanel->isVisible();
        m_logPanel->setVisible(visible);
        m_logToggleButton->setText(visible ? QStringLiteral("▲ 输出")
                                           : QStringLiteral("▼ 输出"));
    });
}
