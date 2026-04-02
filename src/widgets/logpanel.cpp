#include "widgets/logpanel.h"

#include "ui/style_constants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QTextEdit>
#include <QTextCursor>
#include <QThread>
#include <QVBoxLayout>

using namespace MotorDev;

LogPanel *LogPanel::s_instance = nullptr;

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent) {
    s_instance = this;

    setFixedHeight(200);
    setStyleSheet(QStringLiteral("background:%1; border-top:%2px solid %3;")
                      .arg(Style::Color::PanelBackground.name())
                      .arg(Style::Size::BorderThin)
                      .arg(Style::Color::DefaultBorder.name()));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 4, 8, 4);
    rootLayout->setSpacing(4);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(tr("输出"), this);
    titleLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:500;")
                                  .arg(Style::Color::AppText.name()));

    auto *clearButton = new QPushButton(tr("清空"), this);
    clearButton->setFixedHeight(20);
    clearButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; border:none; color:%1; font-size:11px; }"
        "QPushButton:hover { color:%2; }")
                                   .arg(Style::Color::MutedText.name())
                                   .arg(Style::Color::AppText.name()));

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(clearButton);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->document()->setMaximumBlockCount(500);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QTextEdit { background:%1; border:none; color:%2; font-family:Consolas,monospace; font-size:11px; }")
                                   .arg(Style::Color::PanelBackground.name())
                                   .arg(Style::Color::AppText.name()));

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(m_textEdit);

    connect(clearButton, &QPushButton::clicked, m_textEdit, &QTextEdit::clear);
}

LogPanel::~LogPanel() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void LogPanel::appendMessage(QtMsgType type, const QString &msg) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            "appendMessage",
            Qt::QueuedConnection,
            Q_ARG(QtMsgType, type),
            Q_ARG(QString, msg));
        return;
    }

    QString prefix;
    QString color;
    switch (type) {
    case QtWarningMsg:
        prefix = QStringLiteral("[WARN] ");
        color = QStringLiteral("#B85C00");
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        prefix = QStringLiteral("[ERR]  ");
        color = QStringLiteral("#C0392B");
        break;
    default:
        prefix = QStringLiteral("[DBG]  ");
        color = Style::Color::MutedText.name();
        break;
    }

    const QString html = QStringLiteral("<span style=\"color:%1;\">%2%3</span>")
                             .arg(color)
                             .arg(prefix.toHtmlEscaped())
                             .arg(msg.toHtmlEscaped());
    m_textEdit->append(html);
    m_textEdit->moveCursor(QTextCursor::End);
}
