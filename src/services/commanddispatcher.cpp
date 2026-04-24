#include "services/commanddispatcher.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QMetaObject>

namespace {
constexpr uint8_t kInvalidSeq = 0xFF;
}

CommandDispatcher::CommandDispatcher(SerialManager *serialManager, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager) {
    if (m_serialManager == nullptr) {
        return;
    }

    connect(m_serialManager, &SerialManager::frameReceived,
            this, &CommandDispatcher::onFrameReceived);
    connect(m_serialManager, &SerialManager::commandSent,
            this, &CommandDispatcher::onCommandSent);
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &CommandDispatcher::onSerialError);
    connect(m_serialManager, &SerialManager::disconnected,
            this, &CommandDispatcher::onSerialDisconnected);
}

CommandDispatcher::Ticket CommandDispatcher::submitCommand(uint8_t cmd,
                                                           const QByteArray &data,
                                                           Priority priority,
                                                           ResponseCallback onResponse) {
    if (m_serialManager == nullptr) {
        if (onResponse) {
            onResponse(MotorProtocol::CmdErrorResponse, 0, localErrorPayload());
        }
        return InvalidTicket;
    }

    PendingEntry entry;
    entry.ticket = m_nextTicket++;
    entry.cmd = cmd;
    entry.data = data;
    entry.priority = priority;
    entry.onResponse = std::move(onResponse);

    int insertIndex = m_queue.size();
    for (int i = 0; i < m_queue.size(); ++i) {
        if (priority < m_queue.at(i).priority) {
            insertIndex = i;
            break;
        }
    }
    m_queue.insert(insertIndex, entry);
    trySendNext();
    return entry.ticket;
}

bool CommandDispatcher::cancelCommand(Ticket ticket) {
    if (ticket == InvalidTicket || ticket == m_activeTicket) {
        return false;
    }

    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).ticket != ticket) {
            continue;
        }
        m_queue.removeAt(i);
        return true;
    }
    return false;
}

void CommandDispatcher::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (!m_waitingResponse) {
        emit unsolicitedFrameReceived(cmd, seq, data);
        return;
    }

    if (cmd == MotorProtocol::CmdErrorResponse) {
        const ResponseCallback callback = m_activeCallback;
        clearActive();
        if (callback) {
            callback(cmd, seq, data);
        }
        trySendNext();
        return;
    }

    const bool matchesBySeq = (m_activeSeq != kInvalidSeq && seq == m_activeSeq);
    const bool matchesBeforeSeqKnown = (m_activeSeq == kInvalidSeq && cmd == m_activeCmd);
    if (!matchesBySeq && !matchesBeforeSeqKnown) {
        emit unsolicitedFrameReceived(cmd, seq, data);
        return;
    }

    const ResponseCallback callback = m_activeCallback;
    clearActive();
    if (callback) {
        callback(cmd, seq, data);
    }
    trySendNext();
}

void CommandDispatcher::onCommandSent(uint8_t cmd, uint8_t seq) {
    if (!m_waitingResponse || cmd != m_activeCmd || m_activeTicket == InvalidTicket) {
        return;
    }

    m_activeSeq = seq;
    emit commandDispatched(m_activeTicket, cmd, seq);
}

void CommandDispatcher::onSerialError(const QString &message) {
    if (!m_waitingResponse && m_queue.isEmpty()) {
        return;
    }

    if (isSerialLevelError(message)) {
        failActive(message);
        failQueued(message);
        return;
    }

    failActive(message);
    trySendNext();
}

void CommandDispatcher::onSerialDisconnected() {
    failActive(QStringLiteral("Serial port disconnected"));
    failQueued(QStringLiteral("Serial port disconnected"));
}

QByteArray CommandDispatcher::localErrorPayload() {
    return QByteArray(1, static_cast<char>(LocalErrorCode));
}

bool CommandDispatcher::isSerialLevelError(const QString &message) {
    return message == QStringLiteral("Serial port is not open")
        || message.startsWith(QStringLiteral("Failed to open"))
        || message.startsWith(QStringLiteral("Serial port error"));
}

void CommandDispatcher::trySendNext() {
    if (m_waitingResponse || m_queue.isEmpty() || m_serialManager == nullptr) {
        return;
    }

    const PendingEntry entry = m_queue.takeFirst();
    m_waitingResponse = true;
    m_activeTicket = entry.ticket;
    m_activeSeq = kInvalidSeq;
    m_activeCmd = entry.cmd;
    m_activeCallback = entry.onResponse;

    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, entry.cmd),
        Q_ARG(QByteArray, entry.data));
}

void CommandDispatcher::clearActive() {
    m_waitingResponse = false;
    m_activeTicket = InvalidTicket;
    m_activeSeq = kInvalidSeq;
    m_activeCmd = 0;
    m_activeCallback = ResponseCallback();
}

void CommandDispatcher::failActive(const QString &message) {
    Q_UNUSED(message);
    if (!m_waitingResponse) {
        return;
    }

    const ResponseCallback callback = m_activeCallback;
    clearActive();
    if (callback) {
        callback(MotorProtocol::CmdErrorResponse, 0, localErrorPayload());
    }
}

void CommandDispatcher::failQueued(const QString &message) {
    Q_UNUSED(message);
    const QVector<PendingEntry> queued = m_queue;
    m_queue.clear();
    for (const PendingEntry &entry : queued) {
        if (entry.onResponse) {
            entry.onResponse(MotorProtocol::CmdErrorResponse, 0, localErrorPayload());
        }
    }
}
