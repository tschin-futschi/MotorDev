// =============================================================================
// @file    fwflashlogpanel.cpp
// @brief   烧录操作日志面板实现
// =============================================================================
#include "widgets/fwflashlogpanel.h"

#include "ui/style_constants.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

using namespace MotorDev;

FwFlashLogPanel::FwFlashLogPanel(QWidget *parent)
    : QGroupBox(parent) {
    setTitle(tr("操作日志"));
    setObjectName(QStringLiteral("FwFlashLogPanel"));
    setupUi();
}

FwFlashLogPanel::~FwFlashLogPanel() = default;

void FwFlashLogPanel::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(Style::Size::ContentSpacing,
                             Style::Size::ContentSpacing + Style::Size::GroupBoxTopMargin,
                             Style::Size::ContentSpacing,
                             Style::Size::ContentSpacing);
    root->setSpacing(6);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("fwFlashLogText"));
    m_textEdit->setReadOnly(true);
    m_textEdit->setMaximumBlockCount(2000);
    QFont f(QString::fromLatin1(Style::Font::Monospace));
    f.setPixelSize(Style::Size::FwFlashInfoFontPx);
    m_textEdit->setFont(f);
    root->addWidget(m_textEdit, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(0);
    btnRow->addStretch();
    m_clearBtn = new QPushButton(tr("清空"), this);
    m_clearBtn->setObjectName(QStringLiteral("fwFlashLogClearBtn"));
    btnRow->addWidget(m_clearBtn);
    root->addLayout(btnRow);

    connect(m_clearBtn, &QPushButton::clicked, this, &FwFlashLogPanel::clearAll);
}

void FwFlashLogPanel::clearAll() {
    m_textEdit->clear();
}

void FwFlashLogPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QGroupBox::changeEvent(event);
}

void FwFlashLogPanel::retranslateUi() {
    setTitle(tr("操作日志"));
    if (m_clearBtn != nullptr) {
        m_clearBtn->setText(tr("清空"));
    }
}

void FwFlashLogPanel::appendColored(const QString &color,
                                    const QString &level,
                                    const QString &message) {
    auto *bar = m_textEdit->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 2;

    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString html = QStringLiteral(
                             "<span style=\"color:%1;\">[%2] [%3] %4</span>")
                             .arg(color)
                             .arg(ts)
                             .arg(level)
                             .arg(message.toHtmlEscaped());
    m_textEdit->appendHtml(html);

    if (atBottom) {
        m_textEdit->moveCursor(QTextCursor::End);
        bar->setValue(bar->maximum());
    }
}

void FwFlashLogPanel::appendInfo(const QString &message) {
    appendColored(Style::Color::FwFlashLogInfoFg.name(), QStringLiteral("INFO "), message);
}

void FwFlashLogPanel::appendWarn(const QString &message) {
    appendColored(Style::Color::FwFlashLogWarnFg.name(), QStringLiteral("WARN "), message);
}

void FwFlashLogPanel::appendError(const QString &message) {
    appendColored(Style::Color::FwFlashLogErrorFg.name(), QStringLiteral("ERROR"), message);
}

void FwFlashLogPanel::appendOk(const QString &message) {
    appendColored(Style::Color::FwFlashLogOkFg.name(), QStringLiteral("OK   "), message);
}
