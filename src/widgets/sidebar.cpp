#include "widgets/sidebar.h"

#include "ui/style_constants.h"

#include <QComboBox>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QLabel *makeSectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                             .arg(Style::Color::SidebarLabelText.name()));
    return label;
}

QComboBox *makeCombo(const QStringList &items, QWidget *parent) {
    auto *combo = new QComboBox(parent);
    combo->addItems(items);
    combo->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; }")
                             .arg(Style::Color::White.name())
                             .arg(Style::Size::BorderThin)
                             .arg(Style::Color::InputBorder.name())
                             .arg(Style::Color::TopBarValueText.name()));
    return combo;
}
}

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent) {
    setMinimumWidth(Style::Size::SidebarCollapsedWidth);
    setMaximumWidth(Style::Size::SidebarWidth);

    m_animation = new QPropertyAnimation(this, "maximumWidth", this);
    m_animation->setDuration(Style::Size::SidebarAnimationMs);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(
        Style::Size::SidebarContentHorizontalPadding,
        Style::Size::SidebarContentVerticalPadding,
        Style::Size::SidebarContentHorizontalPadding,
        Style::Size::SidebarContentVerticalPadding);
    contentLayout->setSpacing(Style::Size::SidebarSectionSpacing);

    auto *headerLabel = new QLabel(tr("配置"), m_contentWidget);
    headerLabel->setFixedHeight(Style::Size::SidebarHeaderHeight);
    headerLabel->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:500;")
                                   .arg(Style::Color::MutedText.name()));

    contentLayout->addWidget(headerLabel);
    contentLayout->addWidget(makeSectionLabel(tr("端口"), m_contentWidget));
    contentLayout->addWidget(makeCombo({tr("COM1"), tr("COM2"), tr("COM3")}, m_contentWidget));
    contentLayout->addWidget(makeSectionLabel(tr("波特率"), m_contentWidget));
    auto *baudRateCombo = makeCombo(
        {tr("9600"), tr("19200"), tr("38400"), tr("57600"), tr("115200"), tr("230400"), tr("460800"), tr("921600")},
        m_contentWidget);
    baudRateCombo->setCurrentIndex(4);
    contentLayout->addWidget(baudRateCombo);
    contentLayout->addWidget(makeSectionLabel(tr("数据位"), m_contentWidget));
    contentLayout->addWidget(makeCombo({tr("8")}, m_contentWidget));
    contentLayout->addWidget(makeSectionLabel(tr("停止位"), m_contentWidget));
    contentLayout->addWidget(makeCombo({tr("1")}, m_contentWidget));
    contentLayout->addWidget(makeSectionLabel(tr("校验位"), m_contentWidget));
    contentLayout->addWidget(makeCombo({tr("None")}, m_contentWidget));
    contentLayout->addStretch();

    m_connectButton = new QPushButton(tr("连接串口"), m_contentWidget);
    m_connectButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_connectButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:11px; }")
                                       .arg(Style::Color::LightGreen.name())
                                       .arg(Style::Size::BorderThin)
                                       .arg(Style::Color::PrimaryGreen.name())
                                       .arg(Style::Color::PrimaryGreen.name()));
    contentLayout->addWidget(m_connectButton);

    rootLayout->addWidget(m_contentWidget);

    setStyleSheet(QStringLiteral("background:%1; border-right:%2px solid %3;")
                      .arg(Style::Color::SidebarBackground.name())
                      .arg(Style::Size::BorderThin)
                      .arg(Style::Color::DefaultBorder.name()));
}

bool Sidebar::isCollapsed() const {
    return m_collapsed;
}

void Sidebar::toggleCollapsed() {
    const int startWidth = maximumWidth();
    const int targetWidth = m_collapsed ? Style::Size::SidebarWidth : Style::Size::SidebarCollapsedWidth;

    m_animation->stop();
    m_animation->setStartValue(startWidth);
    m_animation->setEndValue(targetWidth);
    m_animation->start();

    m_collapsed = !m_collapsed;
    emit collapseStateChanged(m_collapsed);
}
