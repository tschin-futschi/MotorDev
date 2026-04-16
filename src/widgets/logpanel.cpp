#include "widgets/logpanel.h"

#include "ui/style_constants.h"
#include "ui_logpanel.h"

#include <QMetaObject>
#include <QPushButton>
#include <QTextCursor>
#include <QThread>
#include <utility>

using namespace MotorDev;

LogPanel *LogPanel::s_instance = nullptr;

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::LogPanel>()) {
    s_instance = this;
    ui->setupUi(this);

    ui->textEdit->document()->setMaximumBlockCount(500);
    connect(ui->clearButton, &QPushButton::clicked, ui->textEdit, &QTextEdit::clear);
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
        color = Style::Color::LogWarning.name();
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        prefix = QStringLiteral("[ERR]  ");
        color = Style::Color::LogError.name();
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
    ui->textEdit->append(html);
    ui->textEdit->moveCursor(QTextCursor::End);
}
