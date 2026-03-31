#include "tabs/fwflashtab.h"

#include "ui/style_constants.h"

#include <QLabel>
#include <QVBoxLayout>

using namespace MotorDev;

FwFlashTab::FwFlashTab(QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("固件烧录页面"), this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color:%1; font-size:18px;")
                             .arg(Style::Color::TopBarValueText.name()));
    layout->addWidget(label);
}
