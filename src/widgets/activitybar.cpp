#include "widgets/activitybar.h"

#include "ui_activitybar.h"

#include <QPushButton>
#include <QStyle>
#include <utility>

namespace {
void repolish(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
}

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ActivityBar>()) {
    ui->setupUi(this);

    connect(ui->configButton, &QPushButton::clicked, this, [this] {
        setActivePage(ConfigPage);
        emit pageSelected(ConfigPage);
    });
    connect(ui->registerButton, &QPushButton::clicked, this, [this] {
        setActivePage(RegisterPage);
        emit pageSelected(RegisterPage);
    });
    connect(ui->flashButton, &QPushButton::clicked, this, [this] {
        setActivePage(FlashPage);
        emit pageSelected(FlashPage);
    });
    connect(ui->scopeButton, &QPushButton::clicked, this, [this] {
        setActivePage(ScopePage);
        emit pageSelected(ScopePage);
    });
    connect(ui->debugButton, &QPushButton::clicked, this, [this] {
        setActivePage(DebugPage);
        emit pageSelected(DebugPage);
    });

    setActivePage(ConfigPage);
}

ActivityBar::~ActivityBar() = default;

void ActivityBar::setPageEnabled(int page, bool enabled) {
    switch (page) {
    case RegisterPage:
        ui->registerButton->setEnabled(enabled);
        break;
    case FlashPage:
        ui->flashButton->setEnabled(enabled);
        break;
    case ScopePage:
        ui->scopeButton->setEnabled(enabled);
        break;
    case DebugPage:
        break;
    default:
        break;
    }
}

void ActivityBar::setActivePage(int index) {
    const QList<QPushButton *> buttons = {
        ui->configButton,
        ui->registerButton,
        ui->flashButton,
        ui->scopeButton,
        ui->debugButton,
        ui->settingsButton,
    };

    for (auto *button : buttons) {
        const bool active = (button == ui->configButton && index == ConfigPage)
            || (button == ui->registerButton && index == RegisterPage)
            || (button == ui->flashButton && index == FlashPage)
            || (button == ui->scopeButton && index == ScopePage)
            || (button == ui->debugButton && index == DebugPage);
        button->setProperty("active", active);
        repolish(button);
    }
}
