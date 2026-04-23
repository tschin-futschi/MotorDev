#include "mainwindow.h"
#include "widgets/logpanel.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageLogContext>
#include <QMetaObject>
#include <QStandardPaths>
#include <QString>
#include <QStringConverter>
#include <QTextStream>

#include <cstdio>

namespace {
QFile *g_logFile = nullptr;
QTextStream *g_logStream = nullptr;

QString logCategory(const QMessageLogContext &context) {
    return QString::fromUtf8((context.category != nullptr && context.category[0] != '\0') ? context.category : "default");
}

void initializeLogFile() {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (baseDir.isEmpty()) {
        return;
    }

    QDir dir(baseDir);
    if (!dir.mkpath(QStringLiteral("logs"))) {
        return;
    }

    const QString fileName = QStringLiteral("MotorDev_%1.log")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    g_logFile = new QFile(dir.filePath(QStringLiteral("logs/%1").arg(fileName)));
    if (!g_logFile->open(QIODevice::Append | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
        return;
    }

    g_logStream = new QTextStream(g_logFile);
    g_logStream->setEncoding(QStringConverter::Utf8);
}
} // namespace

static void motorDevMessageHandler(QtMsgType type,
                                   const QMessageLogContext &context,
                                   const QString &msg) {
    if (LogPanel::instance() != nullptr) {
        QMetaObject::invokeMethod(
            LogPanel::instance(),
            "appendMessage",
            Qt::QueuedConnection,
            Q_ARG(QtMsgType, type),
            Q_ARG(QString, logCategory(context)),
            Q_ARG(QString, msg));
    }

    const QString formatted = qFormatLogMessage(type, context, msg);
    if (g_logStream != nullptr) {
        (*g_logStream) << formatted << Qt::endl;
        g_logStream->flush();
    }
    std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
    std::fflush(stderr);
}

int main(int argc, char *argv[]) {
    qSetMessagePattern(QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] [%{category}] %{message}"));
    qRegisterMetaType<QtMsgType>("QtMsgType");

    QApplication app(argc, argv);
    initializeLogFile();
    qInstallMessageHandler(motorDevMessageHandler);
    app.setWindowIcon(QIcon(QStringLiteral(":/motordev_logo.svg")));
    QFile qssFile(QStringLiteral(":/styles/motordev.qss"));
    if (qssFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qssFile.readAll()));
    }

    MainWindow window;
    window.setWindowIcon(QIcon(QStringLiteral(":/motordev_logo.svg")));
    window.show();
    const int exitCode = app.exec();
    delete g_logStream;
    g_logStream = nullptr;
    delete g_logFile;
    g_logFile = nullptr;
    return exitCode;
}
