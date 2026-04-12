#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QVector>
#include <QWidget>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class SimulatorSerial;

class SerialDebugTab : public QWidget {
    Q_OBJECT

public:
    explicit SerialDebugTab(QWidget *parent = nullptr);
    ~SerialDebugTab() override;

signals:
    void debugStreamGenerated(uint8_t channelMask, const QVector<int16_t> &samples);
    void debugStreamActiveChanged(bool active);

private:
    struct PendingDebugFrame {
        uint8_t channelMask = 0x00;
        QVector<int16_t> samples;
    };

    void setupUi();
    void connectSignals();
    QWidget *createConnectionBar();
    QWidget *createLeftPanel();
    QWidget *createLogPanel();
    static QString describeIncoming(uint8_t cmd, const QByteArray &data);

    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void handleHeartbeat(uint8_t seq);
    void handleI2cScan(uint8_t seq, const QByteArray &data);
    void handleSetIcAddr(uint8_t seq, const QByteArray &data);
    void handleReadRegister(uint8_t seq, const QByteArray &data);
    void handleWriteRegister(uint8_t seq, const QByteArray &data);
    void handleStartSampling(uint8_t seq);
    void handleStopSampling(uint8_t seq);
    void handleSetSampleInterval(uint8_t seq, const QByteArray &data);
    void handleSetSampleChannels(uint8_t seq, const QByteArray &data);
    void handleSetChannelRegisterMap(uint8_t seq, const QByteArray &data);
    void handleUnknownCommand(uint8_t cmd, uint8_t seq);
    void streamWorkerLoop();

    void sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data);
    void sendErrorFrame(uint8_t seq, uint8_t errorCode);
    void sendDebugInfo(const QString &message);
    void emitOneStreamFrame();
    void queueDebugStreamFrame(uint8_t channelMask, const QVector<int16_t> &samples);
    void flushPendingDebugStream();
    void appendSysLog(const QString &message, bool isError = false);
    void appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note = {});
    void refreshPortList();
    void setConnectedState(bool connected);
    qint16 registerValueAt(quint16 addr) const;

    SimulatorSerial *m_simulator = nullptr;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_statusBadge = nullptr;

    QLineEdit *m_scanAddrEdit = nullptr;
    QComboBox *m_icAddrResultCombo = nullptr;
    QLineEdit *m_regReadValueEdit = nullptr;
    QComboBox *m_writeResultCombo = nullptr;
    QSpinBox *m_delaySpinBox = nullptr;

    QTextEdit *m_logEdit = nullptr;
    QPushButton *m_clearLogButton = nullptr;

    bool m_isConnected = false;
    bool m_sampling = false;
    uint8_t m_sampleIntervalIndex = 0x05;
    uint8_t m_channelMask = 0x01;
    QVector<quint16> m_channelRegisterMap;
    quint32 m_streamTick = 0;
    qint16 m_streamBaseValue = 0;
    QElapsedTimer m_streamElapsedTimer;
    qint64 m_lastStreamActualUs = -1;
    qint64 m_streamActualUsAccumulator = 0;
    int m_streamActualUsSamples = 0;

    std::thread m_streamThread;
    std::mutex m_streamMutex;
    std::condition_variable m_streamCv;
    bool m_streamStopRequested = false;
    QVector<PendingDebugFrame> m_pendingDebugFrames;
    bool m_debugFlushQueued = false;
};
