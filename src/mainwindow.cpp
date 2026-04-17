#include "mainwindow.h"

#include "devicecontext.h"
#include "serialmanager.h"
#include "tabs/configtab.h"
#include "tabs/fwflashtab.h"
#include "tabs/oscilloscoptab.h"
#include "tabs/registerrwtab.h"
#include "tabs/serialdebugtab.h"
#include "ui/style_constants.h"
#include "ui_mainwindow.h"
#include "widgets/activitybar.h"
#include "widgets/logpanel.h"
#include "widgets/topbar.h"

#include <QPushButton>
#include <utility>

using namespace MotorDev;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>()) {
    setWindowTitle(tr("MotorDev"));
    resize(Style::Size::WindowWidth, Style::Size::WindowHeight);
    setMinimumSize(Style::Size::MinWindowWidth, Style::Size::MinWindowHeight);

    m_serialManager = new SerialManager(this);
    m_deviceContext = new DeviceContext(this);

    setupUi();
    connectSignals();

    m_registerTab->setConnected(false);
    m_registerTab->setEnabled(false);
    ui->activityBar->setPageEnabled(ActivityBar::RegisterPage, false);
    ui->activityBar->setPageEnabled(ActivityBar::FlashPage, false);
    ui->activityBar->setPageEnabled(ActivityBar::ScopePage, true);
    ui->contentStack->widget(ActivityBar::FlashPage)->setEnabled(false);
    m_scopeTab->setEnabled(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto *centralWidget = new QWidget(this);
    ui->setupUi(centralWidget);
    setCentralWidget(centralWidget);

    m_configTab = new ConfigTab(m_serialManager, m_deviceContext, ui->contentStack);
    ui->contentStack->addWidget(m_configTab);

    m_registerTab = new RegisterRwTab(m_serialManager, ui->contentStack);
    ui->contentStack->addWidget(m_registerTab);

    ui->contentStack->addWidget(new FwFlashTab(ui->contentStack));

    m_scopeTab = new OscilloscopTab(m_serialManager, ui->contentStack);
    ui->contentStack->addWidget(m_scopeTab);

    m_debugTab = new SerialDebugTab(this);
    ui->contentStack->setCurrentIndex(ActivityBar::ConfigPage);
    ui->topBar->setScopeControlsVisible(false);
    ui->topBar->setViewMode(0);
    ui->logPanel->setVisible(false);
}

void MainWindow::connectSignals() {
    connect(ui->activityBar, &ActivityBar::pageSelected, this, [this](int index) {
        ui->topBar->setScopeControlsVisible(index == ActivityBar::ScopePage);
        if (index == ActivityBar::DebugPage) {
            m_debugTab->show();
            m_debugTab->raise();
            m_debugTab->activateWindow();
            return;
        }
        ui->contentStack->setCurrentIndex(index);
    });

    connect(m_configTab, &ConfigTab::serialConnected, ui->topBar, &TopBar::onSerialConnected);
    connect(m_configTab, &ConfigTab::serialDisconnected, ui->topBar, &TopBar::onSerialDisconnected);
    connect(ui->topBar, &TopBar::viewModeChanged, m_scopeTab, &OscilloscopTab::onViewModeChangeRequested);
    connect(ui->topBar, &TopBar::styleToggleRequested, m_scopeTab, &OscilloscopTab::onStyleToggleRequested);
    connect(m_scopeTab, &OscilloscopTab::viewModeChanged, ui->topBar, &TopBar::setViewMode);
    connect(m_debugTab, &SerialDebugTab::debugStreamGenerated,
            m_scopeTab, &OscilloscopTab::ingestDebugStream);
    connect(m_debugTab, &SerialDebugTab::debugStreamActiveChanged,
            m_scopeTab, &OscilloscopTab::setDebugStreamActive);

    connect(m_configTab, &ConfigTab::serialConnected, this, [this](const QString &, qint32) {
        m_registerTab->setConnected(true);
        m_registerTab->setEnabled(true);
        ui->activityBar->setPageEnabled(ActivityBar::RegisterPage, true);
        ui->activityBar->setPageEnabled(ActivityBar::FlashPage, true);
        ui->activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        ui->contentStack->widget(ActivityBar::FlashPage)->setEnabled(true);
        m_scopeTab->setEnabled(true);
    });

    connect(m_configTab, &ConfigTab::serialDisconnected, this, [this]() {
        m_registerTab->setConnected(false);
        m_registerTab->setEnabled(false);
        ui->activityBar->setPageEnabled(ActivityBar::RegisterPage, false);
        ui->activityBar->setPageEnabled(ActivityBar::FlashPage, false);
        ui->activityBar->setPageEnabled(ActivityBar::ScopePage, true);
        ui->contentStack->widget(ActivityBar::FlashPage)->setEnabled(false);
        m_scopeTab->setEnabled(true);
    });

    connect(ui->logToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !ui->logPanel->isVisible();
        ui->logPanel->setVisible(visible);
        ui->logToggleButton->setText(visible ? QStringLiteral("▲ 输出")
                                             : QStringLiteral("▼ 输出"));
    });
}