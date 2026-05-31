// =============================================================================
// @file    flashstoragetab.h
// @brief   STM32 自身 FLASH 文件存储 Tab — 上传 / 下载 / 容量显示
//
// 协议 v2.10 / 0x39~0x3E（详见 protocol.md §4.3.6）。
//
// UI 布局（单栏垂直堆叠，简单清晰）：
//   1. 容量行：[已用 X / 总 896 KB | 剩余 Y]  [刷新]
//   2. 操作行：[上传文件...] [下载到本地...] [取消]  阶段标签
//   3. 进度条
//   4. 操作日志（复用 FwFlashLogPanel，4 级着色）
//
// 业务全部下沉到 FlashStoreService；本类仅做 UI 控件 + 信号转发。
// =============================================================================
#pragma once

#include "services/flashstoreservice.h"

#include <QString>
#include <QWidget>

class SerialManager;
class FwFlashLogPanel;
class QLabel;
class QProgressBar;
class QPushButton;
class QJsonObject;

class FlashStorageTab : public QWidget {
    Q_OBJECT

public:
    explicit FlashStorageTab(SerialManager *serialManager, QWidget *parent = nullptr);
    ~FlashStorageTab() override;

    /// @brief 采集 FLASH 存储页功能参数（上次上传/下载目录）为 JSON
    QJsonObject collectFlashStoreConfig() const;

    /// @brief 回填 FLASH 存储页功能参数（上次目录，作为后续文件对话框起始目录）
    void applyFlashStoreConfig(const QJsonObject &flashStore);

private slots:
    void onUploadClicked();
    void onDownloadClicked();
    void onCancelClicked();
    void onRefreshClicked();
    void onWipeClicked();

    void onServiceStateChanged(FlashStoreService::State newState);
    void onServiceStage(const QString &message);
    void onServiceProgress(qint64 done, qint64 total);
    void onServiceLog(FlashStoreService::LogLevel level, const QString &message);
    void onServiceFinished(bool success, const QString &summary);
    void onServiceInfoUpdated(quint32 totalCapacity, quint32 usedSize);
    void onServiceReadCompleted(const QString &savedFilePath);

private:
    void setupUi();
    void connectSignals();
    void updateButtonsEnabled();
    void updateCapacityLabel(quint32 totalCapacity, quint32 usedSize);
    static QString humanBytes(quint64 v);

    FlashStoreService *m_service = nullptr;

    QLabel *m_capacityLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_uploadBtn = nullptr;
    QPushButton *m_downloadBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_wipeBtn = nullptr;
    QLabel *m_stageLabel = nullptr;
    QProgressBar *m_progress = nullptr;
    FwFlashLogPanel *m_logPanel = nullptr;

    QString m_lastDir;  ///< 上次上传/下载使用的目录（文件对话框起始目录；由配置文件存取）
};
