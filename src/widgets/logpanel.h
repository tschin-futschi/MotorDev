// =============================================================================
// @file    logpanel.h
// @brief   日志输出面板 — 主窗口底部的实时日志显示区域
//
// LogPanel 是全局单例（通过 instance() 访问），接收 Qt 日志系统的消息，
// 以颜色编码方式显示在 HTML 富文本区域中。
// 支持 Debug/Info/Warning/Critical/Fatal 级别，最多保留 500 行。
// =============================================================================
#pragma once

#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>
#include <QtGlobal>

/// @brief 日志输出面板（全局单例）
///
/// 位于主窗口底部，固定高度。接收来自 Qt 日志系统（qDebug/qInfo/qWarning 等）
/// 的消息，按级别着色显示。支持跨线程调用（自动通过 QueuedConnection 转发）。
class LogPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造日志面板并注册为全局单例
    explicit LogPanel(QWidget *parent = nullptr);
    ~LogPanel() override;

    /// @brief 获取全局单例指针
    /// @return 当前 LogPanel 实例，若未创建则为 nullptr
    static LogPanel *instance() { return s_instance; }

public slots:
    /// @brief 追加日志消息（线程安全）
    ///
    /// 若从非 GUI 线程调用，会自动通过 QueuedConnection 转发到主线程。
    /// 消息格式：[级别] [分类] 内容，按级别着色。
    ///
    /// @param type 消息级别（QtDebugMsg/QtInfoMsg/QtWarningMsg/QtCriticalMsg/QtFatalMsg）
    /// @param category 日志分类（如 "motordev.scope"），空则显示 "default"
    /// @param msg 日志内容
    void appendMessage(QtMsgType type, const QString &category, const QString &msg);

private:
    /// @brief 构建 UI 布局（标题 + 清空按钮 + 文本区）
    void setupUi();

    QLabel *m_titleLabel = nullptr;         ///< "输出"标题标签
    QPushButton *m_clearButton = nullptr;   ///< 清空日志按钮
    QTextEdit *m_textEdit = nullptr;        ///< 日志文本区（只读 HTML 富文本）
    static LogPanel *s_instance;            ///< 全局单例指针
};
