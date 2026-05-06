// =============================================================================
// @file    topbar.cpp
// @brief   顶部栏实现 — UI 构建、串口状态显示、视图模式切换
// =============================================================================
#include "widgets/topbar.h"

#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QToolButton>
#include <QtSvgWidgets/QSvgWidget>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

TopBar::TopBar(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    m_logo->load(Style::Text::LogoResource);
    connectSignals();
    setScopeControlsVisible(false);
    setViewMode(0);
    setCrosshairEnabled(false);
}

TopBar::~TopBar() = default;

// =============================================================================
// 信号槽连接
// =============================================================================

void TopBar::connectSignals() {
    m_viewModeButton->setToolTip(tr("Toggle view mode"));
    m_styleButton->setToolTip(tr("Toggle channel style panel"));
    m_crosshairButton->setToolTip(tr("Toggle crosshair cursor"));

    // Overlay ↔ Stacked 切换
    connect(m_viewModeButton, &QToolButton::clicked, this, [this]() {
        const int nextMode = m_viewMode == 0 ? 1 : 0;
        setViewMode(nextMode);
        emit viewModeChanged(m_viewMode);
    });

    // 样式面板开关
    connect(m_styleButton, &QToolButton::clicked, this, [this]() {
        emit styleToggleRequested();
    });

    // 十字光标开关
    connect(m_crosshairButton, &QToolButton::toggled, this, [this](bool checked) {
        setCrosshairEnabled(checked);
        emit crosshairToggleRequested(checked);
    });
}

// =============================================================================
// 公共槽 — 串口状态同步
// =============================================================================

void TopBar::onSerialConnected(const QString &port, qint32 baudRate) {
    m_portValueLabel->setText(QStringLiteral("%1 / %2").arg(port).arg(baudRate));
    m_connectionLabel->setText(tr("已连接"));
    m_connectionIndicator->setProperty("connected", true);
    UiUtil::repolish(m_connectionIndicator);
}

void TopBar::onSerialDisconnected() {
    m_portValueLabel->setText(QStringLiteral("– / –"));
    m_connectionLabel->setText(tr("未连接"));
    m_connectionIndicator->setProperty("connected", false);
    UiUtil::repolish(m_connectionIndicator);
}

// =============================================================================
// 公共槽 — 示波器控件
// =============================================================================

void TopBar::setScopeControlsVisible(bool visible) {
    m_viewModeButton->setVisible(visible);
    m_styleButton->setVisible(visible);
    m_crosshairButton->setVisible(visible);
}

void TopBar::setViewMode(int mode) {
    m_viewMode = mode == 1 ? 1 : 0;
    m_viewModeButton->setText(m_viewMode == 0
            ? QStringLiteral("Overlay")
            : QStringLiteral("Stacked"));
}

void TopBar::setCrosshairEnabled(bool enabled) {
    const QSignalBlocker blocker(m_crosshairButton);
    m_crosshairButton->setChecked(enabled);
    m_crosshairButton->setText(enabled
            ? tr("使用十字光标：开启")
            : tr("使用十字光标：关闭"));
}

// =============================================================================
// UI 构建
// =============================================================================

void TopBar::setupUi() {
    setObjectName(QStringLiteral("TopBar"));
    resize(900, 36);
    setMinimumSize(QSize(0, 36));
    setMaximumSize(QSize(QWIDGETSIZE_MAX, 36));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *topBarLayout = new QHBoxLayout(this);
    topBarLayout->setObjectName(QStringLiteral("topBarLayout"));
    topBarLayout->setSpacing(6);
    topBarLayout->setContentsMargins(12, 0, 12, 0);

    // Logo 图标
    m_logo = new QSvgWidget(this);
    m_logo->setObjectName(QStringLiteral("logo"));
    m_logo->setMinimumSize(QSize(22, 22));
    m_logo->setMaximumSize(QSize(22, 22));
    topBarLayout->addWidget(m_logo);

    // 应用标题
    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setText(QStringLiteral("MotorDev"));
    topBarLayout->addWidget(titleLabel);

    // 分隔线
    auto *separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("separator"));
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    topBarLayout->addWidget(separator);

    // 串口状态区：标签 + 端口值 + 指示灯 + 状态文字
    auto *portLabel = new QLabel(this);
    portLabel->setObjectName(QStringLiteral("portLabel"));
    portLabel->setText(tr("串口"));
    topBarLayout->addWidget(portLabel);

    m_portValueLabel = new QLabel(this);
    m_portValueLabel->setObjectName(QStringLiteral("portValueLabel"));
    m_portValueLabel->setText(QStringLiteral("– / –"));
    topBarLayout->addWidget(m_portValueLabel);

    m_connectionIndicator = new QLabel(this);
    m_connectionIndicator->setObjectName(QStringLiteral("connectionIndicator"));
    m_connectionIndicator->setMinimumSize(QSize(7, 7));
    m_connectionIndicator->setMaximumSize(QSize(7, 7));
    m_connectionIndicator->setText(QString());
    m_connectionIndicator->setProperty("connected", false);
    topBarLayout->addWidget(m_connectionIndicator);

    m_connectionLabel = new QLabel(this);
    m_connectionLabel->setObjectName(QStringLiteral("connectionLabel"));
    m_connectionLabel->setText(tr("未连接"));
    topBarLayout->addWidget(m_connectionLabel);

    // 示波器控件：视图模式 + 样式面板开关
    m_viewModeButton = new QToolButton(this);
    m_viewModeButton->setObjectName(QStringLiteral("viewModeButton"));
    m_viewModeButton->setMinimumSize(QSize(68, 22));
    m_viewModeButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 24));
    m_viewModeButton->setText(QStringLiteral("Overlay"));
    topBarLayout->addWidget(m_viewModeButton);

    m_styleButton = new QToolButton(this);
    m_styleButton->setObjectName(QStringLiteral("styleButton"));
    m_styleButton->setMinimumSize(QSize(52, 22));
    m_styleButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 24));
    m_styleButton->setCheckable(true);
    m_styleButton->setText(QStringLiteral("Style"));
    topBarLayout->addWidget(m_styleButton);

    m_crosshairButton = new QToolButton(this);
    m_crosshairButton->setObjectName(QStringLiteral("crosshairButton"));
    m_crosshairButton->setMinimumSize(QSize(132, 22));
    m_crosshairButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 24));
    m_crosshairButton->setCheckable(true);
    m_crosshairButton->setText(tr("使用十字光标：关闭"));
    topBarLayout->addWidget(m_crosshairButton);

    // 弹性间距
    topBarLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // 语言选择
    m_languageCombo = new QComboBox(this);
    m_languageCombo->setObjectName(QStringLiteral("languageCombo"));
    m_languageCombo->setMinimumSize(QSize(96, 0));
    m_languageCombo->setMaximumSize(QSize(96, QWIDGETSIZE_MAX));
    m_languageCombo->addItem(tr("中文"));
    m_languageCombo->addItem(QStringLiteral("English"));
    topBarLayout->addWidget(m_languageCombo);
}
