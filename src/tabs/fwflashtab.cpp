#include "tabs/fwflashtab.h"

#include "ui_fwflashtab.h"
#include "widgets/sidebar.h"

#include <QHBoxLayout>
#include <utility>

FwFlashTab::FwFlashTab(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::FwFlashTab>()) {
    ui->setupUi(this);

    auto *topLayout = qobject_cast<QHBoxLayout *>(layout());
    if (topLayout != nullptr) {
        topLayout->removeWidget(ui->sidebarContent);
        auto *sidebar = new Sidebar(tr("烧录"), ui->sidebarContent, this);
        topLayout->insertWidget(0, sidebar);
    }

    connectSignals();
}

FwFlashTab::~FwFlashTab() = default;

void FwFlashTab::connectSignals() {
}
