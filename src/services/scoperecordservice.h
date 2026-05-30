// =============================================================================
// @file    scoperecordservice.h
// @brief   示波器数据记录服务（服务层）— 采样期间将全速率原始采样持续写入 CSV
//
// 生命周期与采样绑定：采样启动即开录、停止即停录，每次会话一个新文件。
// - tap 点：ScopeService::samplesReceived（降采样前的原始 int16，无损）
// - 文件名：Scope_YYYYMMDD_HHMMSS.csv（PC 本地时间，同秒冲突追加 _N）
// - 列：采样会话开始时实际启用的通道（与 currentChannelMask 一致）
// - time_us 列：帧序号 × 采样间隔（us）
// - 目录由外部设置；未设置 / 无效目录则不录制并通过 recordError 上报
//
// 写盘：主线程缓冲写 QTextStream + 1s 定时 flush（崩溃安全），停止时最终 flush。
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>

class ScopeChannelModel;
class QFile;
class QTextStream;
class QTimer;

class ScopeRecordService : public QObject {
    Q_OBJECT

public:
    explicit ScopeRecordService(ScopeChannelModel *channelModel, QObject *parent = nullptr);
    ~ScopeRecordService() override;

    /// @brief 设置记录目录（由 UI / QSettings 提供）
    void setRecordDirectory(const QString &dir);
    QString recordDirectory() const { return m_recordDir; }

    bool isRecording() const { return m_recording; }
    QString currentFileName() const { return m_currentFileName; }

    /// @brief 返回记录目录下最新的 Scope_*.csv 完整路径（按文件名/起始时间取最大）
    /// @return 完整路径；目录未设/无效/无记录文件时返回空字符串
    QString latestRecordFile() const;

public slots:
    /// @brief 采样参数配置（取 sampleIntervalUs 用于 time_us 计算）
    void onAcquisitionConfigured(int sampleIntervalUs, int displayWindowMs);

    /// @brief 采样运行状态变化：true 开录、false 停录
    void onRunningChanged(bool running);

    /// @brief 收到一帧采样：写一行（time_us + 各记录通道值）
    void onSamplesReceived(uint8_t channelMask, const QVector<int16_t> &samples);

signals:
    /// @brief 录制状态变化（供跑马灯显示）
    /// @param recording true=开始录制；false=结束
    /// @param fileName  录制文件名（basename），结束时为刚结束的文件名
    void recordingChanged(bool recording, const QString &fileName);

    /// @brief 录制无法开始 / 失败的提示（如未设目录、目录无效、建文件失败）
    void recordError(const QString &message);

private:
    void startRecording();
    void stopRecording();

    ScopeChannelModel *m_channelModel = nullptr;
    QString m_recordDir;
    QString m_currentFileName;
    int m_sampleIntervalUs = 0;
    bool m_recording = false;
    qint64 m_rowIndex = 0;
    QVector<int> m_recordedChannels;        ///< 会话开始时的记录通道索引（0~7）
    QFile *m_file = nullptr;
    QTextStream *m_stream = nullptr;
    QTimer *m_flushTimer = nullptr;          ///< 1s 周期 flush，崩溃安全
};
