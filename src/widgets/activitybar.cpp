#include "widgets/activitybar.h"

#include "ui/style_constants.h"

#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString activityButtonStyle(bool active) {
    const auto background = active ? Style::Color::White.name() : Style::Color::Transparent.name();
    const auto border = active ? Style::Color::BorderGreen.name() : Style::Color::Transparent.name();
    const auto text = active ? Style::Color::PrimaryGreen.name() : Style::Color::MutedText.name();

    return QStringLiteral(
               "QPushButton {"
               "background:%1;"
               "border:%2px solid %3;"
               "border-radius:%4px;"
               "color:%5;"
               "font-size:9px;"
               "padding:%6px;"
               "}"
               "QPushButton:hover { background:%7; }")
        .arg(background)
        .arg(Style::Size::BorderThin)
        .arg(border)
        .arg(Style::Size::ActivityButtonRadius)
        .arg(text)
        .arg(Style::Size::ActivityButtonPadding)
        .arg(Style::Color::TopBarBackground.name());
}
}

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent) {
    setFixedWidth(Style::Size::ActivityBarWidth);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        Style::Size::ActivityButtonPadding,
        Style::Size::ActivityBarTopPadding,
        Style::Size::ActivityButtonPadding,
        Style::Size::ActivityBarBottomPadding);
    layout->setSpacing(Style::Size::ActivityBarSpacing);

    m_registerButton = new QPushButton(tr("读写"), this);
    m_flashButton = new QPushButton(tr("烧录"), this);
    m_scopeButton = new QPushButton(tr("示波"), this);
    m_settingsButton = new QPushButton(tr("设置"), this);

    const QList<QPushButton *> buttons = {
        m_registerButton, m_flashButton, m_scopeButton, m_settingsButton};
    for (auto *button : buttons) {
        button->setFixedSize(
            Style::Size::ActivityButtonSize,
            Style::Size::ActivityButtonSize);
        button->setStyleSheet(activityButtonStyle(false));
        layout->addWidget(button, 0, Qt::AlignHCenter);
    }

    layout->addStretch();

    setStyleSheet(QStringLiteral("background:%1; border-right:%2px solid %3;")
                      .arg(Style::Color::ActivityBarBackground.name())
                      .arg(Style::Size::BorderThin)
                      .arg(Style::Color::DefaultBorder.name()));

    connect(m_registerButton, &QPushButton::clicked, this, [this] {
        setActivePage(RegisterPage);
        emit pageSelected(RegisterPage);
    });
    connect(m_flashButton, &QPushButton::clicked, this, [this] {
        setActivePage(FlashPage);
        emit pageSelected(FlashPage);
    });
    connect(m_scopeButton, &QPushButton::clicked, this, [this] {
        setActivePage(ScopePage);
        emit pageSelected(ScopePage);
    });

    setActivePage(RegisterPage);
}

void ActivityBar::setActivePage(int index) {
    m_registerButton->setStyleSheet(activityButtonStyle(index == RegisterPage));
    m_flashButton->setStyleSheet(activityButtonStyle(index == FlashPage));
    m_scopeButton->setStyleSheet(activityButtonStyle(index == ScopePage));
    m_settingsButton->setStyleSheet(activityButtonStyle(false));
}
