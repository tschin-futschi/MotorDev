#include "widgets/topbar.h"

#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QToolButton>
#include <QtSvgWidgets/QSvgWidget>

using namespace MotorDev;

TopBar::TopBar(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    m_logo->load(Style::Text::LogoResource);
    connectSignals();
    setScopeControlsVisible(false);
    setViewMode(0);
}

TopBar::~TopBar() = default;

void TopBar::connectSignals() {
    m_viewModeButton->setToolTip(tr("Toggle view mode"));
    m_styleButton->setToolTip(tr("Toggle channel style panel"));

    connect(m_viewModeButton, &QToolButton::clicked, this, [this]() {
        const int nextMode = m_viewMode == 0 ? 1 : 0;
        setViewMode(nextMode);
        emit viewModeChanged(m_viewMode);
    });
    connect(m_styleButton, &QToolButton::clicked, this, [this]() {
        emit styleToggleRequested();
    });
}

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

void TopBar::setScopeControlsVisible(bool visible) {
    m_viewModeButton->setVisible(visible);
    m_styleButton->setVisible(visible);
}

void TopBar::setViewMode(int mode) {
    m_viewMode = mode == 1 ? 1 : 0;
    m_viewModeButton->setText(m_viewMode == 0
            ? QStringLiteral("Overlay")
            : QStringLiteral("Stacked"));
}

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

    m_logo = new QSvgWidget(this);
    m_logo->setObjectName(QStringLiteral("logo"));
    m_logo->setMinimumSize(QSize(22, 22));
    m_logo->setMaximumSize(QSize(22, 22));
    topBarLayout->addWidget(m_logo);

    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setText(QStringLiteral("MotorDev"));
    topBarLayout->addWidget(titleLabel);

    auto *separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("separator"));
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    topBarLayout->addWidget(separator);

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

    topBarLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_languageCombo = new QComboBox(this);
    m_languageCombo->setObjectName(QStringLiteral("languageCombo"));
    m_languageCombo->setMinimumSize(QSize(96, 0));
    m_languageCombo->setMaximumSize(QSize(96, QWIDGETSIZE_MAX));
    m_languageCombo->addItem(tr("中文"));
    m_languageCombo->addItem(QStringLiteral("English"));
    topBarLayout->addWidget(m_languageCombo);
}
