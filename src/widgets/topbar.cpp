#include "widgets/topbar.h"

#include "ui/style_constants.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QSvgWidget>

using namespace MotorDev;

TopBar::TopBar(QWidget *parent)
    : QWidget(parent) {
    setFixedHeight(Style::Size::TopBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(
        Style::Size::HeaderHorizontalPadding,
        Style::Size::OuterMargin,
        Style::Size::HeaderHorizontalPadding,
        Style::Size::OuterMargin);
    layout->setSpacing(Style::Size::ActivityBarSpacing);

    auto *logo = new QSvgWidget(Style::Text::LogoResource, this);
    logo->setFixedSize(Style::Size::LogoSize, Style::Size::LogoSize);

    auto *titleLabel = new QLabel(tr("MotorDev"), this);
    auto *separator = new QFrame(this);
    auto *portLabel = new QLabel(tr("串口"), this);
    m_portValueLabel = new QLabel(tr("COM1 / 115200"), this);
    m_connectionIndicator = new QLabel(this);
    auto *connectionLabel = new QLabel(tr("未连接"), this);
    auto *spacer = new QWidget(this);
    m_languageCombo = new QComboBox(this);

    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_connectionIndicator->setFixedSize(
        Style::Size::IndicatorSize,
        Style::Size::IndicatorSize);
    m_languageCombo->setFixedWidth(Style::Size::LanguageComboWidth);
    m_languageCombo->addItems({tr("中文"), tr("English")});

    titleLabel->setStyleSheet(QStringLiteral("color:%1; font-size:13px; font-weight:500;")
                                  .arg(Style::Color::AppText.name()));
    separator->setStyleSheet(QStringLiteral("color:%1;").arg(Style::Color::TableRowBorder.name()));
    portLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                 .arg(Style::Color::TopBarLabelText.name()));
    m_portValueLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                        .arg(Style::Color::TopBarValueText.name()));
    m_connectionIndicator->setStyleSheet(QStringLiteral(
        "background:%1; border-radius:%2px;")
                                             .arg(Style::Color::DisconnectedIndicator.name())
                                             .arg(Style::Size::IndicatorSize / 2 + 1));
    connectionLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                       .arg(Style::Color::TopBarValueText.name()));
    m_languageCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:%2px solid %3; padding:2px 6px; color:%4; }")
                                        .arg(Style::Color::White.name())
                                        .arg(Style::Size::BorderThin)
                                        .arg(Style::Color::InputBorder.name())
                                        .arg(Style::Color::TopBarValueText.name()));

    setStyleSheet(QStringLiteral("background:%1; border-bottom:%2px solid %3;")
                      .arg(Style::Color::TopBarBackground.name())
                      .arg(Style::Size::BorderThin)
                      .arg(Style::Color::TableRowBorder.name()));

    layout->addWidget(logo);
    layout->addWidget(titleLabel);
    layout->addWidget(separator);
    layout->addWidget(portLabel);
    layout->addWidget(m_portValueLabel);
    layout->addWidget(m_connectionIndicator);
    layout->addWidget(connectionLabel);
    layout->addWidget(spacer);
    layout->addWidget(m_languageCombo);
}
