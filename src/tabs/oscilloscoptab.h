#pragma once

#include "ui/scopeviewmode.h"

#include <QColor>
#include <QKeyEvent>
#include <QObject>
#include <QString>
#include <QVector>
#include <QWidget>
#include <memory>

class ScopeBottomPanel;
class SerialManager;
class QTimer;

namespace Ui {
class OscilloscopTab;
}

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
    ~OscilloscopTab() override;

signals:
    void runningChanged(bool running);
    void viewModeChanged(int mode);

public slots:
    void setDebugStreamActive(bool active);
    void ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples);
    void onViewModeChangeRequested(int mode);
    void onSamplingToggleRequested(bool running);
    void onStyleToggleRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

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
    void togglePlotFullscreen();
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
    static ScopeViewMode viewModeFromInt(int mode);
    static int viewModeToInt(ScopeViewMode mode);
    bool sendCommand(uint8_t cmd, const QByteArray &data);
    void logPlaceholderAction(const QString &action);
    void onPerfTimerTick();

    std::unique_ptr<Ui::OscilloscopTab> ui;
    SerialManager *m_serialManager = nullptr;
    ScopeBottomPanel *m_bottomPanel = nullptr;
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
    ScopeViewMode m_viewMode = ScopeViewMode::Overlay;
    QTimer *m_perfTimer = nullptr;
    QWidget *m_fullscreenWindow = nullptr;
    int m_plotLayoutIndex = -1;
    int m_perfBatchCount = 0;
    int m_perfSampleCount = 0;
};