#include "widgets/scopegeneratorpanel.h"

#include "ui_scopegeneratorpanel.h"

#include <utility>

ScopeGeneratorPanel::ScopeGeneratorPanel(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ScopeGeneratorPanel>()) {
    ui->setupUi(this);
}

ScopeGeneratorPanel::~ScopeGeneratorPanel() = default;
