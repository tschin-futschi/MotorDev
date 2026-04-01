#include "mainwindow.h"

#include <QApplication>
#include <QString>

int main(int argc, char *argv[]) {
    qSetMessagePattern(QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{message}"));

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
