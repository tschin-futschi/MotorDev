// =============================================================================
// @file    fwflashlogpanel.h
// @brief   烧录操作日志面板 — 时间戳 + 级别上色 + 自动滚动
//
// 与全局 LogPanel 解耦：本面板只承载烧录页面的局部操作日志，行格式：
//   [HH:mm:ss.zzz] [INFO|WARN|ERROR|OK] <内容>
// 仅当滚动条已在底部时新日志才自动滚到底部，避免打断用户阅读历史。
// =============================================================================
#pragma once

#include <QGroupBox>

class QEvent;
class QPlainTextEdit;
class QPushButton;

class FwFlashLogPanel : public QGroupBox {
    Q_OBJECT

public:
    explicit FwFlashLogPanel(QWidget *parent = nullptr);
    ~FwFlashLogPanel() override;

public slots:
    void appendInfo(const QString &message);
    void appendWarn(const QString &message);
    void appendError(const QString &message);
    void appendOk(const QString &message);
    void clearAll();

protected:
    /// @brief 语言切换（QEvent::LanguageChange）时刷新标题与清空按钮
    void changeEvent(QEvent *event) override;

private:
    void setupUi();

    /// @brief 重设标题与清空按钮文字
    void retranslateUi();
    void appendColored(const QString &color, const QString &level, const QString &message);

    QPlainTextEdit *m_textEdit = nullptr;
    QPushButton *m_clearBtn = nullptr;
};
