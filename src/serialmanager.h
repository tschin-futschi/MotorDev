#pragma once

#include "frameparser.h"
#include "protocol/motor_protocol.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <cstdint>

class QSerialPort;
class QThread;
class QTimer;

class SerialManager : public QObject {
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager() override;

    static QStringList availablePorts();

public slots:
    void init();
    void openPort(const QString &portName, qint32 baudRate);
    void closePort();
    void startHeartbeat();
    void stopHeartbeat();
    void sendCommand(uint8_t cmd, const QByteArray &data = {});

signals:
    void connected();
    void disconnected();
    void commandSent(uint8_t cmd, uint8_t seq);
    void frameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void streamDataReceived(uint8_t channelMask, const QVector<int16_t> &samples);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onRetryTimeout();
    void onHeartbeatTimeout();
    void onSerialErrorOccurred(int error);

private:
    void clearPendingCommand();
    void stopRetryTimer();
    void stopHeartbeatTimer();
    void handleControlFrame(const ControlFrame &frame);
    void closePortInternal(bool emitDisconnectedSignal);

    QThread *m_thread = nullptr;
    QSerialPort *m_serial = nullptr;
    FrameParser m_parser;
    uint8_t m_nextSeq = 0;
    QTimer *m_retryTimer = nullptr;
    QByteArray m_pendingFrame;
    uint8_t m_pendingSeq = 0;
    uint8_t m_pendingCmd = 0;
    int m_retryCount = 0;
    QTimer *m_heartbeatTimer = nullptr;
    int m_missedHeartbeats = 0;

    static constexpr int MaxRetries = 2;
    static constexpr int RetryTimeoutMs = 100;
    static constexpr int HeartbeatIntervalMs = 1000;
    static constexpr int MaxMissedHeartbeats = 3;
};
