#include "tabs/oscilloscoptab.h"

#include "ui/style_constants.h"
#include "widgets/sidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

using namespace MotorDev;

OscilloscopTab::OscilloscopTab(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

void OscilloscopTab::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *sidebarContent = new QWidget;
    auto *sidebarLayout = new QVBoxLayout(sidebarContent);
    sidebarLayout->setContentsMargins(
        Style::Size::SidebarContentHorizontalPadding,
        Style::Size::SidebarContentVerticalPadding,
        Style::Size::SidebarContentHorizontalPadding,
        Style::Size::SidebarContentVerticalPadding);
    sidebarLayout->setSpacing(Style::Size::SidebarSectionSpacing);
    auto *sidebarLabel = new QLabel(tr("示波侧边栏"), sidebarContent);
    sidebarLabel->setWordWrap(true);
    sidebarLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                    .arg(Style::Color::SidebarLabelText.name()));
    sidebarLayout->addWidget(sidebarLabel);
    sidebarLayout->addStretch();

    layout->addWidget(new Sidebar(tr("示波"), sidebarContent, this));

    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    auto *label = new QLabel(tr("实时示波页面"), contentWidget);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color:%1; font-size:18px;")
                             .arg(Style::Color::TopBarValueText.name()));
    contentLayout->addWidget(label, 1);
    layout->addWidget(contentWidget, 1);
}

void OscilloscopTab::connectSignals() {
}
