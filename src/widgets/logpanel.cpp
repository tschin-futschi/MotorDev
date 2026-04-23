#include "widgets/logpanel.h"

#include "ui/style_constants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

using namespace MotorDev;

LogPanel *LogPanel::s_instance = nullptr;

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent) {
    s_instance = this;
    setupUi();

    m_textEdit->document()->setMaximumBlockCount(500);
    connect(m_clearButton, &QPushButton::clicked, m_textEdit, &QTextEdit::clear);
}

LogPanel::~LogPanel() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void LogPanel::appendMessage(QtMsgType type, const QString &category, const QString &msg) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            "appendMessage",
            Qt::QueuedConnection,
            Q_ARG(QtMsgType, type),
            Q_ARG(QString, category),
            Q_ARG(QString, msg));
        return;
    }

    QString prefix;
    QString color;
    switch (type) {
    case QtInfoMsg:
        prefix = QStringLiteral("[INFO] ");
        color = Style::Color::LogInfo.name();
        break;
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

    const QString categoryTag = QStringLiteral("[%1] ").arg(category.isEmpty() ? QStringLiteral("default") : category);
    const QString html = QStringLiteral("<span style=\"color:%1;\">%2%3%4</span>")
                             .arg(color)
                             .arg(prefix.toHtmlEscaped())
                             .arg(categoryTag.toHtmlEscaped())
                             .arg(msg.toHtmlEscaped());
    m_textEdit->append(html);
    m_textEdit->moveCursor(QTextCursor::End);
}

void LogPanel::setupUi() {
    setObjectName(QStringLiteral("LogPanel"));
    setMinimumSize(QSize(0, Style::Size::LogPanelHeight));
    setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::LogPanelHeight));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(4);
    rootLayout->setContentsMargins(8, 4, 8, 4);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setObjectName(QStringLiteral("headerLayout"));
    headerLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addLayout(headerLayout);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setText(tr("输出"));
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_clearButton = new QPushButton(this);
    m_clearButton->setObjectName(QStringLiteral("clearButton"));
    m_clearButton->setMinimumSize(QSize(0, 20));
    m_clearButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 20));
    m_clearButton->setText(tr("清空"));
    headerLayout->addWidget(m_clearButton);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("textEdit"));
    m_textEdit->setReadOnly(true);
    rootLayout->addWidget(m_textEdit);
}
