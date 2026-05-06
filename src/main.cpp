// =============================================================================
// @file    main.cpp
// @brief   MotorDev 应用程序入口
//
// 负责初始化 QApplication、日志系统、全局样式表、启动画面，
// 以及创建主窗口并进入事件循环。
//
// 日志架构：
// - 安装全局 Qt 消息处理器 motorDevMessageHandler()
// - 所有 qDebug/qWarning/qCritical 输出同时路由至三个目标：
//   1. LogPanel（底部日志面板，通过 QueuedConnection 跨线程安全投递）
//   2. 磁盘日志文件（AppLocalDataLocation/logs/MotorDev_*.log）
//   3. stderr（控制台）
// =============================================================================

#include "mainwindow.h"
#include "widgets/logpanel.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageLogContext>
#include <QMetaObject>
#include <QPixmap>
#include <QScreen>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QString>
#include <QStringConverter>
#include <QTextStream>

#include <cstdio>

namespace {

/// @brief 全局日志文件句柄，程序退出时手动释放
QFile *g_logFile = nullptr;

/// @brief 全局日志文本流，绑定到 g_logFile
QTextStream *g_logStream = nullptr;

/// @brief 从日志上下文中提取分类名称
/// @param context  Qt 日志上下文，包含 category、file、line 等信息
/// @return 分类名称字符串；若上下文无分类则返回 "default"
QString logCategory(const QMessageLogContext &context) {
    return QString::fromUtf8((context.category != nullptr && context.category[0] != '\0') ? context.category : "default");
}

/// @brief 初始化磁盘日志文件
///
/// 在 AppLocalDataLocation/logs/ 目录下创建以时间戳命名的日志文件。
/// 若目录创建失败或文件打开失败，则静默跳过（不影响程序启动）。
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

/// @brief 全局 Qt 消息处理器
///
/// 替代默认的 Qt 消息输出行为，将日志同时分发到：
/// 1. LogPanel 单例（通过 QueuedConnection 保证线程安全）
/// 2. 磁盘日志文件
/// 3. stderr 控制台
///
/// @param type     消息类型（Debug/Warning/Critical/Fatal）
/// @param context  消息上下文（源文件、行号、分类）
/// @param msg      消息内容
static void motorDevMessageHandler(QtMsgType type,
                                   const QMessageLogContext &context,
                                   const QString &msg) {
    // 将日志投递到 LogPanel UI 面板（跨线程安全）
    if (LogPanel::instance() != nullptr) {
        QMetaObject::invokeMethod(
            LogPanel::instance(),
            "appendMessage",
            Qt::QueuedConnection,
            Q_ARG(QtMsgType, type),
            Q_ARG(QString, logCategory(context)),
            Q_ARG(QString, msg));
    }

    // 格式化日志并写入文件和 stderr
    const QString formatted = qFormatLogMessage(type, context, msg);
    if (g_logStream != nullptr) {
        (*g_logStream) << formatted << Qt::endl;
        g_logStream->flush();
    }
    std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
    std::fflush(stderr);
}

/// @brief 程序入口函数
int main(int argc, char *argv[]) {
    // 设置日志格式模板：[时间] [级别] [分类] 消息
    qSetMessagePattern(QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] [%{category}] %{message}"));
    // 注册 QtMsgType 元类型，使其可通过 QueuedConnection 跨线程传递
    qRegisterMetaType<QtMsgType>("QtMsgType");

    // 显式锁定 high-DPI scale factor 取整策略（保持 Qt 6 默认 PassThrough，不改变现有视觉表现）。
    // 必须在 QApplication 构造之前调用，否则无效。配合 Windows manifest 中的 PerMonitorV2 声明，
    // 确保锁屏-解锁等边界条件下进程仍获取稳定的 per-monitor DPI，避免字体随系统 DPI 误判而漂移。
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    initializeLogFile();
    qInstallMessageHandler(motorDevMessageHandler);
    app.setWindowIcon(QIcon(QStringLiteral(":/motordev_logo.svg")));

    // 加载全局 QSS 样式表
    QFile qssFile(QStringLiteral(":/styles/motordev.qss"));
    if (qssFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qssFile.readAll()));
    }

    // 显示启动画面（Splash Screen）
    QPixmap splashPixmap(QStringLiteral(":/motordev_logo.svg"));
    if (!splashPixmap.isNull()) {
        splashPixmap = splashPixmap.scaled(360, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QSplashScreen splash(splashPixmap, Qt::WindowStaysOnTopHint);
    splash.show();
    app.processEvents();  // 确保启动画面立即渲染

    // 一次性启动诊断日志：复现"锁屏后字体变小"问题时用于对比正常/异常两次启动的 DPI 取值。
    // 此时 LogPanel 单例尚未创建，日志会落到磁盘日志文件与 stderr。
    if (auto *primary = QGuiApplication::primaryScreen()) {
        qDebug().noquote() << QStringLiteral("[startup] primaryScreen=%1 logicalDPI=%2 devicePixelRatio=%3")
                                  .arg(primary->name())
                                  .arg(primary->logicalDotsPerInch())
                                  .arg(primary->devicePixelRatio());
    }

    // 创建并显示主窗口
    MainWindow window;
    window.setWindowIcon(QIcon(QStringLiteral(":/motordev_logo.svg")));
    window.show();
    splash.finish(&window);  // 主窗口显示后关闭启动画面

    const int exitCode = app.exec();

    // 清理全局日志资源
    delete g_logStream;
    g_logStream = nullptr;
    delete g_logFile;
    g_logFile = nullptr;
    return exitCode;
}
