// =============================================================================
// @file    fwflashcontrolpanel.h
// @brief   烧录控制面板 — 开始/取消按钮、进度条、阶段文本
//
// 纯展示与转发：所有状态由外部（FwFlashTab/FwFlashService）注入。
// 通过 startRequested / cancelRequested 信号上报用户意图。
// =============================================================================
#pragma once

#include <QGroupBox>

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

private:
    void setupUi();

    QPushButton *m_startBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_stageLabel = nullptr;
};
