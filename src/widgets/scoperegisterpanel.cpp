#include "widgets/scoperegisterpanel.h"

#include "ui_scoperegisterpanel.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <utility>

ScopeRegisterPanel::ScopeRegisterPanel(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ScopeRegisterPanel>()) {
    ui->setupUi(this);
    m_descEdits[0] = ui->descEdit0; m_addrEdits[0] = ui->addrEdit0; m_valueEdits[0] = ui->valueEdit0; m_readButtons[0] = ui->readButton0; m_writeButtons[0] = ui->writeButton0;
    m_descEdits[1] = ui->descEdit1; m_addrEdits[1] = ui->addrEdit1; m_valueEdits[1] = ui->valueEdit1; m_readButtons[1] = ui->readButton1; m_writeButtons[1] = ui->writeButton1;
    m_descEdits[2] = ui->descEdit2; m_addrEdits[2] = ui->addrEdit2; m_valueEdits[2] = ui->valueEdit2; m_readButtons[2] = ui->readButton2; m_writeButtons[2] = ui->writeButton2;
    m_descEdits[3] = ui->descEdit3; m_addrEdits[3] = ui->addrEdit3; m_valueEdits[3] = ui->valueEdit3; m_readButtons[3] = ui->readButton3; m_writeButtons[3] = ui->writeButton3;
    m_descEdits[4] = ui->descEdit4; m_addrEdits[4] = ui->addrEdit4; m_valueEdits[4] = ui->valueEdit4; m_readButtons[4] = ui->readButton4; m_writeButtons[4] = ui->writeButton4;
    m_descEdits[5] = ui->descEdit5; m_addrEdits[5] = ui->addrEdit5; m_valueEdits[5] = ui->valueEdit5; m_readButtons[5] = ui->readButton5; m_writeButtons[5] = ui->writeButton5;
    m_descEdits[6] = ui->descEdit6; m_addrEdits[6] = ui->addrEdit6; m_valueEdits[6] = ui->valueEdit6; m_readButtons[6] = ui->readButton6; m_writeButtons[6] = ui->writeButton6;
    m_descEdits[7] = ui->descEdit7; m_addrEdits[7] = ui->addrEdit7; m_valueEdits[7] = ui->valueEdit7; m_readButtons[7] = ui->readButton7; m_writeButtons[7] = ui->writeButton7;
    for (int row = 0; row < RowCount; ++row) {
        auto *rowLayout = qobject_cast<QHBoxLayout *>(ui->rootLayout->itemAt(row)->layout());
        if (rowLayout != nullptr) {
            rowLayout->setStretch(0, 3);
            rowLayout->setStretch(1, 2);
            rowLayout->setStretch(2, 2);
        }
    }
    m_intervalEdit = ui->intervalEdit;
    m_startButton = ui->startButton;
    m_stopButton = ui->stopButton;
    m_clearButton = ui->clearButton;
    m_loadButton = ui->loadButton;
    connectSignals();
}

ScopeRegisterPanel::~ScopeRegisterPanel() = default;

void ScopeRegisterPanel::connectSignals() {
    for (int row = 0; row < RowCount; ++row) {
        connect(m_readButtons[row], &QPushButton::clicked, this, [this, row]() { emit readRequested(row); });
        connect(m_writeButtons[row], &QPushButton::clicked, this, [this, row]() { emit writeRequested(row); });
    }

    connect(m_startButton, &QPushButton::clicked, this, &ScopeRegisterPanel::startRequested);
    connect(m_stopButton, &QPushButton::clicked, this, &ScopeRegisterPanel::stopRequested);
    connect(m_clearButton, &QPushButton::clicked, this, &ScopeRegisterPanel::clearPanelRequested);
    connect(m_loadButton, &QPushButton::clicked, this, &ScopeRegisterPanel::loadParamsRequested);
}
