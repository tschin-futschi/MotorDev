#include "mainwindow.h"

#include "tabs/configtab.h"
#include "tabs/fwflashtab.h"
#include "tabs/oscilloscoptab.h"
#include "tabs/registerrwtab.h"
#include "ui/style_constants.h"
#include "widgets/activitybar.h"
#include "widgets/topbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace MotorDev;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("MotorDev"));
    resize(Style::Size::WindowWidth, Style::Size::WindowHeight);
    setMinimumSize(Style::Size::MinWindowWidth, Style::Size::MinWindowHeight);

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
    m_contentStack->addWidget(new ConfigTab(m_contentStack));
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

    m_statusLabel = new QLabel(tr("115200 8N1 | 未连接 | RX: 0 | TX: 0 | 就绪"), m_statusBarWidget);
    m_statusLabel->setStyleSheet(QStringLiteral("color:%1; font-size:10px;")
                                     .arg(Style::Color::StatusBarText.name()));
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();

    m_statusBarWidget->setStyleSheet(QStringLiteral("background:%1;")
                                         .arg(Style::Color::PrimaryGreen.name()));

    rootLayout->addWidget(m_topBar);
    rootLayout->addWidget(middleWidget, 1);
    rootLayout->addWidget(m_statusBarWidget);

    central->setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::WindowBackground.name()));
    setCentralWidget(central);

    connect(m_activityBar, &ActivityBar::pageSelected, m_contentStack, &QStackedWidget::setCurrentIndex);
}
