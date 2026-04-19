#include "widgets/scoperegisterpanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

ScopeRegisterPanel::ScopeRegisterPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
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

void ScopeRegisterPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeRegisterPanel"));
    setMinimumSize(QSize(430, 292));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(4);
    rootLayout->setSizeConstraint(QLayout::SetMinimumSize);
    rootLayout->setContentsMargins(6, 6, 6, 6);

    for (int row = 0; row < RowCount; ++row) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(QStringLiteral("row%1Layout").arg(row));
        rowLayout->setSpacing(8);
        rowLayout->setStretch(0, 3);
        rowLayout->setStretch(1, 2);
        rowLayout->setStretch(2, 2);
        rootLayout->addLayout(rowLayout);

        m_descEdits[row] = new QLineEdit(this);
        m_descEdits[row]->setObjectName(QStringLiteral("descEdit%1").arg(row));
        m_descEdits[row]->setMinimumSize(QSize(124, 24));
        m_descEdits[row]->setMaximumSize(QSize(QWIDGETSIZE_MAX, 24));
        m_descEdits[row]->setText(QStringLiteral("REG%1").arg(row + 1));
        m_descEdits[row]->setProperty("inputRole", QStringLiteral("scope-register"));
        rowLayout->addWidget(m_descEdits[row]);

        m_addrEdits[row] = new QLineEdit(this);
        m_addrEdits[row]->setObjectName(QStringLiteral("addrEdit%1").arg(row));
        m_addrEdits[row]->setMinimumSize(QSize(92, 24));
        m_addrEdits[row]->setMaximumSize(QSize(QWIDGETSIZE_MAX, 24));
        m_addrEdits[row]->setText(QStringLiteral("0x%1").arg(0x0020 + row * 2, 4, 16, QLatin1Char('0')).toUpper());
        m_addrEdits[row]->setProperty("inputRole", QStringLiteral("scope-register"));
        rowLayout->addWidget(m_addrEdits[row]);

        m_valueEdits[row] = new QLineEdit(this);
        m_valueEdits[row]->setObjectName(QStringLiteral("valueEdit%1").arg(row));
        m_valueEdits[row]->setMinimumSize(QSize(92, 24));
        m_valueEdits[row]->setMaximumSize(QSize(QWIDGETSIZE_MAX, 24));
        m_valueEdits[row]->setText(QStringLiteral("0x0000"));
        m_valueEdits[row]->setProperty("inputRole", QStringLiteral("scope-register"));
        rowLayout->addWidget(m_valueEdits[row]);

        m_readButtons[row] = new QPushButton(this);
        m_readButtons[row]->setObjectName(QStringLiteral("readButton%1").arg(row));
        m_readButtons[row]->setMinimumSize(QSize(28, 24));
        m_readButtons[row]->setMaximumSize(QSize(28, 24));
        m_readButtons[row]->setProperty("buttonRole", QStringLiteral("read"));
        m_readButtons[row]->setText(QStringLiteral("R"));
        rowLayout->addWidget(m_readButtons[row]);

        m_writeButtons[row] = new QPushButton(this);
        m_writeButtons[row]->setObjectName(QStringLiteral("writeButton%1").arg(row));
        m_writeButtons[row]->setMinimumSize(QSize(28, 24));
        m_writeButtons[row]->setMaximumSize(QSize(28, 24));
        m_writeButtons[row]->setProperty("buttonRole", QStringLiteral("write"));
        m_writeButtons[row]->setText(QStringLiteral("W"));
        rowLayout->addWidget(m_writeButtons[row]);
    }

    auto *intervalRow = new QHBoxLayout();
    intervalRow->setObjectName(QStringLiteral("intervalRow"));
    intervalRow->setSpacing(6);
    intervalRow->setContentsMargins(0, 10, 0, 0);
    rootLayout->addLayout(intervalRow);

    auto *intervalLabel = new QLabel(this);
    intervalLabel->setObjectName(QStringLiteral("intervalLabel"));
    intervalLabel->setText(tr("下发时间间隔"));
    intervalRow->addWidget(intervalLabel);

    m_intervalEdit = new QLineEdit(this);
    m_intervalEdit->setObjectName(QStringLiteral("intervalEdit"));
    m_intervalEdit->setMinimumSize(QSize(68, 0));
    m_intervalEdit->setMaximumSize(QSize(68, QWIDGETSIZE_MAX));
    m_intervalEdit->setPlaceholderText(QStringLiteral("100"));
    m_intervalEdit->setProperty("inputRole", QStringLiteral("scope-register"));
    intervalRow->addWidget(m_intervalEdit);

    m_startButton = new QPushButton(this);
    m_startButton->setObjectName(QStringLiteral("startButton"));
    m_startButton->setProperty("buttonRole", QStringLiteral("action-start"));
    m_startButton->setText(tr("启动"));
    intervalRow->addWidget(m_startButton);

    m_stopButton = new QPushButton(this);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_stopButton->setProperty("buttonRole", QStringLiteral("action-stop"));
    m_stopButton->setText(tr("停止"));
    intervalRow->addWidget(m_stopButton);

    intervalRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_clearButton = new QPushButton(this);
    m_clearButton->setObjectName(QStringLiteral("clearButton"));
    m_clearButton->setProperty("buttonRole", QStringLiteral("action-clear"));
    m_clearButton->setText(tr("清除面板"));
    intervalRow->addWidget(m_clearButton);

    m_loadButton = new QPushButton(this);
    m_loadButton->setObjectName(QStringLiteral("loadButton"));
    m_loadButton->setProperty("buttonRole", QStringLiteral("action-load"));
    m_loadButton->setText(tr("录入参数"));
    intervalRow->addWidget(m_loadButton);
}
