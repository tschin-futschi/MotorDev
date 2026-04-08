#pragma once

#include "widgets/scopetoolbar.h"

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class ScopeBottomPanel;
class ScopePlotWidget;
class ScopeToolBar;
class SerialManager;

class OscilloscopTab : public QWidget {
    Q_OBJECT

public:
    explicit OscilloscopTab(SerialManager *serialManager, QWidget *parent = nullptr);

private:
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
        QVector<double> samples;
    };

    void setupUi();
    void connectSignals();
    void onRunningToggleRequested(bool running);
    void setRunning(bool running, bool updateUi);
    void updateChannelEnabled(int index, bool enabled);
    void updateChannelDescription(int index, const QString &text);
    void updateChannelAddress(int index, const QString &text);
    void updateSampleInterval(const QString &text);
    void startSamplingSequence();
    void sendNextStartCommand();
    void finishPendingCommand();
    void handleFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleErrorOccurred(const QString &message);
    void handleStreamDataReceived(uint8_t channelMask, const QVector<int16_t> &samples);
    void refreshPlotData();
    bool hasValidSamplingConfig() const;
    uint8_t currentChannelMask() const;
    QVector<quint16> currentRegisterMap() const;
    static quint16 parseRegisterAddress(const QString &text);
    static uint8_t sampleIntervalIndexForText(const QString &text);
    bool sendCommand(uint8_t cmd, const QByteArray &data);
    void logPlaceholderAction(const QString &action);

    SerialManager *m_serialManager = nullptr;
    ScopeToolBar *m_toolBar = nullptr;
    ScopePlotWidget *m_plotWidget = nullptr;
    ScopeBottomPanel *m_bottomPanel = nullptr;
    QVector<ScopeChannelState> m_channels;
    uint8_t m_sampleIntervalIndex = 0x05;
    uint8_t m_lastStreamMask = 0x00;
    PendingCommand m_pendingCommand = PendingCommand::None;
    bool m_startPending = false;
    bool m_stopPending = false;
    bool m_hasReceivedStream = false;
    QString m_lastError;
    bool m_running = false;
    ScopeToolBar::ViewMode m_viewMode = ScopeToolBar::ViewMode::Overlay;
};
