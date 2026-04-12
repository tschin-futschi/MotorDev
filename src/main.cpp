#include "mainwindow.h"
#include "widgets/logpanel.h"

#include <QApplication>
#include <QIcon>
#include <QMessageLogContext>
#include <QMetaObject>
#include <QString>

#include <cstdio>

static void motorDevMessageHandler(QtMsgType type,
                                   const QMessageLogContext &context,
                                   const QString &msg) {
    if (LogPanel::instance() != nullptr) {
        QMetaObject::invokeMethod(
            LogPanel::instance(),
            "appendMessage",
            Qt::QueuedConnection,
            Q_ARG(QtMsgType, type),
            Q_ARG(QString, msg));
    }

    const QString formatted = qFormatLogMessage(type, context, msg);
    std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
    std::fflush(stderr);
}

int main(int argc, char *argv[]) {
    qSetMessagePattern(QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{message}"));
    qRegisterMetaType<QtMsgType>("QtMsgType");

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/motordev_logo.svg")));

    MainWindow window;
    window.setWindowIcon(QIcon(QStringLiteral(":/motordev_logo.svg")));
    qInstallMessageHandler(motorDevMessageHandler);
    window.show();
    return app.exec();
}
