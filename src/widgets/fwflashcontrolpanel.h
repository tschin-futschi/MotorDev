// =============================================================================
// @file    fwflashcontrolpanel.h
// @brief   烧录控制面板 — 开始/取消按钮、进度条、阶段文本
//
// 纯展示与转发：所有状态由外部（FwFlashTab/FwFlashService）注入。
// 通过 startRequested / cancelRequested 信号上报用户意图。
// =============================================================================
#pragma once

#include <QGroupBox>

class QEvent;
class QLabel;
class QProgressBar;
class QPushButton;

class FwFlashControlPanel : public QGroupBox {
    Q_OBJECT

public:
    explicit FwFlashControlPanel(QWidget *parent = nullptr);
    ~FwFlashControlPanel() override;

    void setStartEnabled(bool enabled);
    void setCancelEnabled(bool enabled);
    void setProgress(qint64 sentBytes, qint64 totalBytes);
    void setStageText(const QString &text);
    void resetProgress();

signals:
    void startRequested();
    void cancelRequested();

protected:
    /// @brief 语言切换（QEvent::LanguageChange）时刷新标题/按钮（空闲态阶段文字）
    void changeEvent(QEvent *event) override;

private:
    void setupUi();

    /// @brief 重设标题、开始/取消按钮文字；若处于空闲态则同步刷新阶段文字
    void retranslateUi();

    QPushButton *m_startBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_stageLabel = nullptr;
    bool m_stageIsIdle = true;          ///< 阶段标签是否处于"空闲"默认态（语言切换时随之重译）
};
