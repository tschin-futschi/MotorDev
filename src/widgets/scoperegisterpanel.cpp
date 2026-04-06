#include "widgets/scoperegisterpanel.h"

#include "ui/style_constants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString makePanelStyle() {
    return QStringLiteral("background:%1; border:1px solid %2; border-radius:6px;")
        .arg(Style::Color::ScopePanelBackground.name(), Style::Color::ScopePanelBorder.name());
}

QString makeTextStyle() {
    return QStringLiteral("QLineEdit { background:%1; color:%2; border:1px solid %3; "
                          "border-radius:4px; padding:3px 6px; font-size:11px; min-height:24px; }")
        .arg(Style::Color::White.name(), Style::Color::ScopeText.name(), Style::Color::ScopeInputBorder.name());
}

QString makeActionStyle(const QColor &bg, const QColor &border) {
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:4px; "
               "padding:0; font-size:11px; min-width:28px; min-height:24px; }"
               "QPushButton:hover { background:%3; }")
        .arg(bg.name(), Style::Color::ScopeText.name(), border.name());
}
} // namespace

ScopeRegisterPanel::ScopeRegisterPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

void ScopeRegisterPanel::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(4);
    rootLayout->setSizeConstraint(QLayout::SetMinimumSize);

    for (int row = 0; row < RowCount; ++row) {
        auto *rowLayout = new QHBoxLayout;
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        m_descEdits[row] = new QLineEdit(this);
        m_addrEdits[row] = new QLineEdit(this);
        m_valueEdits[row] = new QLineEdit(this);
        m_readButtons[row] = new QPushButton(QStringLiteral("R"), this);
        m_writeButtons[row] = new QPushButton(QStringLiteral("W"), this);

        m_descEdits[row]->setText(QStringLiteral("REG%1").arg(row + 1));
        m_addrEdits[row]->setText(QStringLiteral("0x%1").arg(0x0020 + row * 2, 4, 16, QLatin1Char('0')).toUpper());
        m_valueEdits[row]->setText(QStringLiteral("0x0000"));
        m_descEdits[row]->setMinimumWidth(124);
        m_addrEdits[row]->setMinimumWidth(92);
        m_valueEdits[row]->setMinimumWidth(92);
        m_descEdits[row]->setFixedHeight(24);
        m_addrEdits[row]->setFixedHeight(24);
        m_valueEdits[row]->setFixedHeight(24);
        m_readButtons[row]->setFixedSize(28, 24);
        m_writeButtons[row]->setFixedSize(28, 24);

        m_descEdits[row]->setStyleSheet(makeTextStyle());
        m_addrEdits[row]->setStyleSheet(makeTextStyle());
        m_valueEdits[row]->setStyleSheet(makeTextStyle());
        m_readButtons[row]->setStyleSheet(
            makeActionStyle(Style::Color::ScopeRegReadBackground, Style::Color::ScopeRegReadBorder));
        m_writeButtons[row]->setStyleSheet(
            makeActionStyle(Style::Color::ScopeRegWriteBackground, Style::Color::ScopeRegWriteBorder));

        rowLayout->addWidget(m_descEdits[row], 3, Qt::AlignVCenter);
        rowLayout->addWidget(m_addrEdits[row], 2, Qt::AlignVCenter);
        rowLayout->addWidget(m_valueEdits[row], 2, Qt::AlignVCenter);
        rowLayout->addWidget(m_readButtons[row], 0, Qt::AlignVCenter);
        rowLayout->addWidget(m_writeButtons[row], 0, Qt::AlignVCenter);

        rootLayout->addLayout(rowLayout);
    }

    auto *intervalRow = new QHBoxLayout;
    intervalRow->setContentsMargins(0, 10, 0, 0);
    intervalRow->setSpacing(6);

    auto *intervalLabel = new QLabel(tr("下发时间间隔"), this);
    intervalLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                     .arg(Style::Color::ScopeTextSecondary.name()));

    m_intervalEdit = new QLineEdit(this);
    m_intervalEdit->setPlaceholderText(QStringLiteral("100"));
    m_intervalEdit->setFixedWidth(68);
    m_intervalEdit->setStyleSheet(makeTextStyle());

    m_startButton = new QPushButton(tr("启动"), this);
    m_stopButton = new QPushButton(tr("停止"), this);
    m_clearButton = new QPushButton(tr("清除面板"), this);
    m_loadButton = new QPushButton(tr("录入参数"), this);
    m_startButton->setStyleSheet(makeActionStyle(Style::Color::ScopeRegReadBackground, Style::Color::ScopeRegReadBorder));
    m_stopButton->setStyleSheet(makeActionStyle(Style::Color::ScopeRegWriteBackground, Style::Color::ScopeRegWriteBorder));
    m_clearButton->setStyleSheet(makeActionStyle(Style::Color::ScopeRegClearBackground, Style::Color::ScopeRegClearBorder));
    m_loadButton->setStyleSheet(makeActionStyle(Style::Color::ScopeRegLoadBackground, Style::Color::ScopeRegLoadBorder));

    intervalRow->addWidget(intervalLabel);
    intervalRow->addWidget(m_intervalEdit);
    intervalRow->addWidget(m_startButton);
    intervalRow->addWidget(m_stopButton);
    intervalRow->addStretch();
    intervalRow->addWidget(m_clearButton);
    intervalRow->addWidget(m_loadButton);

    rootLayout->addLayout(intervalRow);
    setMinimumSize(430, 292);
    setStyleSheet(makePanelStyle());
}

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
