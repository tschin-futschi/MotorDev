#pragma once

#include <QByteArray>
#include <QObject>
#include <QVector>

#include <cstdint>
#include <functional>

class SerialManager;

class CommandDispatcher : public QObject {
    Q_OBJECT

public:
    enum Priority {
        Urgent = 0,
        High = 1,
        Normal = 2,
        Low = 3,
    };
    Q_ENUM(Priority)

    using ResponseCallback = std::function<void(uint8_t cmd, uint8_t seq, const QByteArray &data)>;
    using Ticket = uint32_t;

    static constexpr Ticket InvalidTicket = 0;
    static constexpr uint8_t LocalErrorCode = 0xFF;

    explicit CommandDispatcher(SerialManager *serialManager, QObject *parent = nullptr);

    Ticket submitCommand(uint8_t cmd,
                         const QByteArray &data,
                         Priority priority,
                         ResponseCallback onResponse);
    bool cancelCommand(Ticket ticket);

signals:
    void commandDispatched(uint32_t ticket, uint8_t cmd, uint8_t seq);
    void unsolicitedFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);

private slots:
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);
    void onCommandSent(uint8_t cmd, uint8_t seq);
    void onSerialError(const QString &message);
    void onSerialDisconnected();

private:
    struct PendingEntry {
        Ticket ticket = InvalidTicket;
        uint8_t cmd = 0;
        QByteArray data;
        Priority priority = Normal;
        ResponseCallback onResponse;
    };

    static QByteArray localErrorPayload();
    static bool isSerialLevelError(const QString &message);

    void trySendNext();
    void clearActive();
    void failActive(const QString &message);
    void failQueued(const QString &message);

    SerialManager *m_serialManager = nullptr;
    QVector<PendingEntry> m_queue;
    Ticket m_nextTicket = 1;

    bool m_waitingResponse = false;
    Ticket m_activeTicket = InvalidTicket;
    uint8_t m_activeSeq = 0xFF;
    uint8_t m_activeCmd = 0;
    ResponseCallback m_activeCallback;
};
