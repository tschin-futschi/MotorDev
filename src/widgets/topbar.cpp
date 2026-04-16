#include "widgets/topbar.h"

#include "ui/style_constants.h"
#include "ui_topbar.h"

#include <QStyle>
#include <utility>

using namespace MotorDev;

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

TopBar::TopBar(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::TopBar>()) {
    ui->setupUi(this);
    ui->logo->load(Style::Text::LogoResource);
}

TopBar::~TopBar() = default;

void TopBar::onSerialConnected(const QString &port, qint32 baudRate) {
    ui->portValueLabel->setText(QStringLiteral("%1 / %2").arg(port).arg(baudRate));
    ui->connectionLabel->setText(tr("已连接"));
    ui->connectionIndicator->setProperty("connected", true);
    repolish(ui->connectionIndicator);
}

void TopBar::onSerialDisconnected() {
    ui->portValueLabel->setText(QStringLiteral("– / –"));
    ui->connectionLabel->setText(tr("未连接"));
    ui->connectionIndicator->setProperty("connected", false);
    repolish(ui->connectionIndicator);
}
