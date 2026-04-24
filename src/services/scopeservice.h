// ScopeService — 采样状态机、协议命令序列、流数据转发
#pragma once

#include <QByteArray>
#include <QObject>
#include <QVector>

#include <cstdint>

class QTimer;
class ScopeChannelModel;
class CommandDispatcher;
class SerialManager;

struct ScopeStreamPacket {
    uint8_t channelMask = 0x00;
    QVector<int16_t> samples;
};

using ScopeStreamBatch = QVector<ScopeStreamPacket>;

Q_DECLARE_METATYPE(ScopeStreamBatch)

class ScopeStreamBatcher : public QObject {
    Q_OBJECT

public:
    explicit ScopeStreamBatcher(QObject *parent = nullptr);

public slots:
    void enqueue(uint8_t channelMask, const QVector<int16_t> &samples);
    void clearPending();

signals:
    void batchReady(const ScopeStreamBatch &batch);

private:
    void ensureTimer();

    ScopeStreamBatch m_pendingBatch;
    QTimer *m_flushTimer = nullptr;
};

class ScopeService : public QObject {
    Q_OBJECT

public:
    explicit ScopeService(SerialManager *serialManager,
                          CommandDispatcher *dispatcher,
                          ScopeChannelModel *channelModel,
                          QObject *parent = nullptr);
    ~ScopeService() override;

    bool isRunning() const { return m_running; }

    void requestStart(uint8_t sampleIntervalIndex, int displayWindowMs);
    void requestStop();

    void setDebugStreamActive(bool active) { m_debugStreamActive = active; }
    void ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples);

    int perfBatchCount() const { return m_perfBatchCount; }
    int perfSampleCount() const { return m_perfSampleCount; }
    void resetPerfCounters() { m_perfBatchCount = 0; m_perfSampleCount = 0; }

signals:
    void runningChanged(bool running);
    void samplesReceived(uint8_t channelMask, const QVector<int16_t> &samples);
    void acquisitionConfigured(int sampleIntervalUs, int displayWindowMs);
    void resetViewRequested();
    void startError(const QString &message);

private slots:
    void handleResponse(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleStreamBatchReceived(const ScopeStreamBatch &batch);

private:
    enum class PendingCommand {
        None,
        SetSampleInterval,
        SetSampleChannels,
        SetChannelRegisterMap,
        StartSampling,
        StopSampling
    };

    void setRunning(bool running);
    void startSamplingSequence();
    void sendNextStartCommand();
    void finishPendingCommand();
    void clearPendingCommandState();
    void clearPendingStreamBatch();
    void logStartSnapshot() const;
    static uint8_t commandForPending(PendingCommand pending);
    bool sendCommand(uint8_t cmd, const QByteArray &data);

    static constexpr int StreamWatchdogMs = 5000;

    SerialManager *m_serialManager = nullptr;
    CommandDispatcher *m_dispatcher = nullptr;
    ScopeChannelModel *m_channelModel = nullptr;
    ScopeStreamBatcher *m_streamBatcher = nullptr;
    QTimer *m_streamWatchdog = nullptr;

    uint8_t m_sampleIntervalIndex = 0x04;
    int m_displayWindowMs = 50;
    uint8_t m_lastStreamMask = 0x00;
    PendingCommand m_pendingCommand = PendingCommand::None;
    bool m_startPending = false;
    bool m_stopPending = false;
    bool m_hasReceivedStream = false;
    bool m_debugStreamActive = false;
    QString m_lastError;
    bool m_running = false;
    int m_perfBatchCount = 0;
    int m_perfSampleCount = 0;
};
