#include "widgets/topbar.h"

#include "ui/repolish.h"
#include "ui/style_constants.h"
#include "ui_topbar.h"

#include <utility>

using namespace MotorDev;

TopBar::TopBar(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::TopBar>()) {
    ui->setupUi(this);
    ui->logo->load(Style::Text::LogoResource);
    connectSignals();
    setScopeControlsVisible(false);
    setRunning(false);
    setViewMode(0);
}

TopBar::~TopBar() = default;

void TopBar::connectSignals() {
    ui->viewModeButton->setToolTip(tr("Toggle view mode"));
    ui->styleButton->setToolTip(tr("Toggle channel style panel"));

    connect(ui->viewModeButton, &QToolButton::clicked, this, [this]() {
        const int nextMode = m_viewMode == 0 ? 1 : 0;
        setViewMode(nextMode);
        emit viewModeChanged(m_viewMode);
    });
    connect(ui->samplingButton, &QPushButton::clicked, this, [this]() {
        emit samplingToggleRequested(!m_running);
    });
    connect(ui->styleButton, &QToolButton::clicked, this, [this]() {
        emit styleToggleRequested();
    });
}

void TopBar::onSerialConnected(const QString &port, qint32 baudRate) {
    ui->portValueLabel->setText(QStringLiteral("%1 / %2").arg(port).arg(baudRate));
    ui->connectionLabel->setText(tr("已连接"));
    ui->connectionIndicator->setProperty("connected", true);
    UiUtil::repolish(ui->connectionIndicator);
}

void TopBar::onSerialDisconnected() {
    ui->portValueLabel->setText(QStringLiteral("– / –"));
    ui->connectionLabel->setText(tr("未连接"));
    ui->connectionIndicator->setProperty("connected", false);
    UiUtil::repolish(ui->connectionIndicator);
}

void TopBar::setScopeControlsVisible(bool visible) {
    ui->viewModeButton->setVisible(visible);
    ui->styleButton->setVisible(visible);
    ui->samplingButton->setVisible(visible);
}

void TopBar::setRunning(bool running) {
    m_running = running;
    ui->samplingButton->setText(m_running ? tr("停止采样") : tr("开始采样"));
    ui->samplingButton->setProperty("running", m_running);
    UiUtil::repolish(ui->samplingButton);
}

void TopBar::setViewMode(int mode) {
    m_viewMode = mode == 1 ? 1 : 0;
    ui->viewModeButton->setText(m_viewMode == 0
            ? QStringLiteral("Overlay")
            : QStringLiteral("Stacked"));
}