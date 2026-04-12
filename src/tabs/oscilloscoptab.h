#pragma once

#include "widgets/scopetoolbar.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>
#include <QWidget>

class ScopeBottomPanel;
class ScopePlotWidget;
class ScopeStylePanel;
class ScopeToolBar;
class SerialManager;
class QSplitter;
class QTimer;

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

class OscilloscopTab : public QWidget {
    Q_OBJECT

public:
    explicit OscilloscopTab(SerialManager *serialManager, QWidget *parent = nullptr);

public slots:
    void setDebugStreamActive(bool active);
    void ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples);

private:
    static constexpr uint8_t InvalidSeq = 0xFF;

    enum class PendingCommand {
        None,
        SetSampleInterval,
        SetSampleChannels,
        SetChannelRegisterMap,
        StartSampling,
        StopSampling
    };

    struct ScopeChannelState {
        bool enabled = false;
        QString description;
        QString addressText;
        quint16 registerAddress = 0xFFFF;
        QColor color;
        qreal lineWidth = 4.0;
        Qt::PenStyle lineStyle = Qt::SolidLine;
        bool showDataPoints = false;
    };

    void setupUi();
    void connectSignals();
    void onRunningToggleRequested(bool running);
    void setRunning(bool running, bool updateUi);
    void updateChannelEnabled(int index, bool enabled);
    void updateChannelDescription(int index, const QString &text);
    void updateChannelAddress(int index, const QString &text);
    void updateChannelColor(int index, const QColor &color);
    void updateChannelLineWidth(int index, int width);
    void updateChannelLineStyle(int index, Qt::PenStyle style);
    void updateChannelShowDataPoints(int index, bool show);
    void resetChannelStylesToDefault();
    void updateSampleInterval(const QString &text);
    void updateDisplayWindow(const QString &text);
    void toggleStylePanel();
    void startSamplingSequence();
    void sendNextStartCommand();
    void finishPendingCommand();
    void clearPendingCommandState();
    void logStartSnapshot() const;
    void handleCommandSent(uint8_t cmd, uint8_t seq);
    static uint8_t commandForPending(PendingCommand pending);
    void handleFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleErrorOccurred(const QString &message);
    void handleStreamDataReceived(uint8_t channelMask, const QVector<int16_t> &samples);
    void handleStreamBatchReceived(const ScopeStreamBatch &batch);
    void clearPendingStreamBatch();
    void refreshPlotData();
    bool hasValidSamplingConfig() const;
    uint8_t currentChannelMask() const;
    QVector<quint16> currentRegisterMap() const;
    static quint16 parseRegisterAddress(const QString &text);
    static uint8_t sampleIntervalIndexForText(const QString &text);
    static int sampleIntervalUsForIndex(uint8_t index);
    static int displayWindowMsForText(const QString &text);
    bool sendCommand(uint8_t cmd, const QByteArray &data);
    void logPlaceholderAction(const QString &action);
    void onPerfTimerTick();

    SerialManager *m_serialManager = nullptr;
    ScopeToolBar *m_toolBar = nullptr;
    ScopePlotWidget *m_plotWidget = nullptr;
    ScopeBottomPanel *m_bottomPanel = nullptr;
    ScopeStylePanel *m_stylePanel = nullptr;
    QSplitter *m_splitter = nullptr;
    ScopeStreamBatcher *m_streamBatcher = nullptr;
    QVector<ScopeChannelState> m_channels;
    QVector<ScopeChannelState> m_defaultChannels;
    uint8_t m_sampleIntervalIndex = 0x05;
    int m_displayWindowMs = 50;
    uint8_t m_lastStreamMask = 0x00;
    PendingCommand m_pendingCommand = PendingCommand::None;
    uint8_t m_pendingSeq = InvalidSeq;
    bool m_startPending = false;
    bool m_stopPending = false;
    bool m_hasReceivedStream = false;
    bool m_debugStreamActive = false;
    QString m_lastError;
    bool m_running = false;
    ScopeToolBar::ViewMode m_viewMode = ScopeToolBar::ViewMode::Overlay;

    // 1 Hz performance counters (Step 1)
    QTimer *m_perfTimer = nullptr;
    int m_perfBatchCount = 0;
    int m_perfSampleCount = 0;
};
