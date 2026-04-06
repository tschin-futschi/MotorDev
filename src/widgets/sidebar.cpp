#include "widgets/sidebar.h"

#include "ui/style_constants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

Sidebar::Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent)
    : QWidget(parent) {
    m_bodyWidth = Style::Size::SidebarWidth;
    setMinimumWidth(Style::Size::SidebarTotalWidth);
    setMaximumWidth(Style::Size::SidebarTotalWidth);

    m_animation = new QPropertyAnimation(this, "maximumWidth", this);
    m_animation->setDuration(Style::Size::SidebarAnimationMs);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_bodyWidget = new QWidget(this);
    m_bodyWidget->setFixedWidth(m_bodyWidth);

    auto *bodyLayout = new QVBoxLayout(m_bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto *headerLabel = new QLabel(title, m_bodyWidget);
    headerLabel->setFixedHeight(Style::Size::SidebarHeaderHeight);
    headerLabel->setContentsMargins(
        Style::Size::SidebarContentHorizontalPadding,
        0,
        Style::Size::SidebarContentHorizontalPadding,
        0);
    headerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:500;")
                                   .arg(Style::Color::MutedText.name()));
    bodyLayout->addWidget(headerLabel);

    m_contentWidget = contentWidget;
    m_contentWidget->setParent(m_bodyWidget);
    bodyLayout->addWidget(m_contentWidget, 1);

    m_toggleButton = new QPushButton(QStringLiteral("<"), this);
    m_toggleButton->setFixedWidth(Style::Size::SidebarHandleWidth);
    m_toggleButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; border-left:none; border-right:%2px solid %3; color:%4; }")
                                      .arg(Style::Color::SidebarBackground.name())
                                      .arg(Style::Size::BorderThin)
                                      .arg(Style::Color::DefaultBorder.name())
                                      .arg(Style::Color::MutedText.name()));

    rootLayout->addWidget(m_bodyWidget);
    rootLayout->addWidget(m_toggleButton);

    setStyleSheet(QStringLiteral("background:%1; border-right:%2px solid %3;")
                      .arg(Style::Color::SidebarBackground.name())
                      .arg(Style::Size::BorderThin)
                      .arg(Style::Color::DefaultBorder.name()));

    connect(m_toggleButton, &QPushButton::clicked, this, &Sidebar::toggleCollapsed);
    connect(m_animation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        const int width = value.toInt();
        setMinimumWidth(width);
    });
}

bool Sidebar::isCollapsed() const {
    return m_collapsed;
}

QWidget *Sidebar::contentWidget() const {
    return m_contentWidget;
}

void Sidebar::setBodyWidth(int width) {
    if (width <= 0 || m_bodyWidth == width) {
        return;
    }

    m_bodyWidth = width;
    m_bodyWidget->setFixedWidth(m_bodyWidth);
    if (!m_collapsed) {
        applyExpandedWidth();
    }
}

void Sidebar::toggleCollapsed() {
    setCollapsed(!m_collapsed);
}

void Sidebar::setCollapsed(bool collapsed) {
    if (m_collapsed == collapsed) {
        return;
    }

    const int startWidth = maximumWidth();
    const int targetWidth = collapsed ? Style::Size::SidebarHandleWidth
                                      : m_bodyWidth + Style::Size::SidebarHandleWidth;

    m_animation->stop();
    m_animation->disconnect(this);
    connect(m_animation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        const int width = value.toInt();
        setMinimumWidth(width);
    });
    connect(m_animation, &QPropertyAnimation::finished, this, [this, collapsed] {
        if (collapsed) {
            m_bodyWidget->setVisible(false);
        }
    });

    if (!collapsed) {
        m_bodyWidget->setVisible(true);
    }

    m_animation->setStartValue(startWidth);
    m_animation->setEndValue(targetWidth);
    m_animation->start();

    m_collapsed = collapsed;
    m_toggleButton->setText(m_collapsed ? QStringLiteral(">") : QStringLiteral("<"));

    emit collapseStateChanged(m_collapsed);
}

void Sidebar::applyExpandedWidth() {
    const int totalWidth = m_bodyWidth + Style::Size::SidebarHandleWidth;
    setMinimumWidth(totalWidth);
    setMaximumWidth(totalWidth);
}
