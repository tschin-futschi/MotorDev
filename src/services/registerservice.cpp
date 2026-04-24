// RegisterService — 寄存器读写命令队列、协议收发、超时管理
#include "services/registerservice.h"

#include "protocol/motor_protocol.h"
#include "services/commanddispatcher.h"

#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>

Q_LOGGING_CATEGORY(lcRegister, "motordev.register")

namespace {
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
}

RegisterService::RegisterService(CommandDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_dispatcher(dispatcher) {
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RegisterService::onTimeout);
    if (m_dispatcher != nullptr) {
        connect(m_dispatcher, &CommandDispatcher::commandDispatched,
                this, &RegisterService::onCommandDispatched);
    }
}

RegisterService::~RegisterService() = default;

void RegisterService::readSingleRow(int globalRow, quint16 addr) {
    RowRequest req;
    req.globalRow = globalRow;
    req.addr = addr;
    req.isWrite = false;
    if (m_hasPending) {
        m_pendingQueue.prepend(req);
        emit rowPending(globalRow);
        return;
    }
    m_isWriteOp = false;
    m_priority = CommandDispatcher::Normal;
    m_currentRequest = req;
    m_hasPending = true;
    emit rowPending(globalRow);
    submitCurrentRequest();
}

void RegisterService::writeSingleRow(int globalRow, quint16 addr, qint16 value,
                                     CommandDispatcher::Priority priority) {
    RowRequest req;
    req.globalRow = globalRow;
    req.addr = addr;
    req.value = value;
    req.isWrite = true;
    if (m_hasPending) {
        m_pendingQueue.prepend(req);
        return;
    }
    m_isWriteOp = true;
    m_priority = priority;
    m_currentRequest = req;
    m_hasPending = true;
    submitCurrentRequest();
}

void RegisterService::readBatch(const QVector<RowRequest> &rows) {
    enqueueAndStart(rows, false, CommandDispatcher::Normal);
}

void RegisterService::writeBatch(const QVector<RowRequest> &rows) {
    enqueueAndStart(rows, true, CommandDispatcher::Normal);
}

void RegisterService::enqueueAndStart(const QVector<RowRequest> &rows, bool isWrite,
                                      CommandDispatcher::Priority priority) {
    clearQueue();
    m_priority = priority;
    for (RowRequest r : rows) {
        r.isWrite = isWrite;
        m_pendingQueue.enqueue(r);
    }
    processNextInQueue();
}

void RegisterService::submitCurrentRequest() {
    if (m_dispatcher == nullptr) {
        if (m_isWriteOp) {
            emit rowWriteError(m_currentRequest.globalRow);
        } else {
            emit rowReadError(m_currentRequest.globalRow);
        }
        processNextInQueue();
        return;
    }

    const uint8_t cmd = m_isWriteOp ? MotorProtocol::CmdWriteRegister : MotorProtocol::CmdReadRegister;
    const QByteArray payload = m_isWriteOp
        ? MotorProtocol::encodeWriteRegister(m_currentRequest.addr, m_currentRequest.value)
        : MotorProtocol::encodeReadRegister(m_currentRequest.addr);

    if (m_isWriteOp) {
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 TX row=%2 addr=0x%3 value=0x%4 payload=%5")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(m_currentRequest.globalRow)
                   .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
                   .arg(static_cast<quint16>(m_currentRequest.value), 4, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(payload))
                   .toUpper();
    } else {
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 TX row=%2 addr=0x%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(m_currentRequest.globalRow)
                   .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(payload))
                   .toUpper();
    }

    const uint32_t sessionId = m_sessionId;
    QPointer<RegisterService> guard(this);
    m_activeTicket = m_dispatcher->submitCommand(
        cmd, payload, m_priority,
        [guard, sessionId](uint8_t responseCmd, uint8_t responseSeq, const QByteArray &responseData) {
            if (guard == nullptr) {
                return;
            }
            guard->handleResponse(responseCmd, responseSeq, responseData, sessionId);
        });
}

void RegisterService::handleResponse(uint8_t cmd, uint8_t seq, const QByteArray &data, uint32_t sessionId) {
    Q_UNUSED(seq);
    if (!m_hasPending || sessionId != m_sessionId) {
        return;
    }

    m_timeoutTimer->stop();
    m_activeTicket = 0;

    const int row = m_currentRequest.globalRow;
    if (cmd == MotorProtocol::CmdReadRegister && !m_isWriteOp) {
        qint16 value = 0;
        if (MotorProtocol::decodeReadRegisterResponse(data, &value)) {
            qCInfo(lcRegister).noquote()
                << QStringLiteral("%1 RX ACK row=%2 value=0x%3 payload=%4")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(row)
                       .arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0'))
                       .arg(formatPayloadHex(data))
                       .toUpper();
            emit rowReadOk(row, value);
        } else {
            qCWarning(lcRegister).noquote()
                << QStringLiteral("%1 RX decode failed row=%2 payload=%3")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(row)
                       .arg(formatPayloadHex(data));
            emit rowReadError(row);
        }
        processNextInQueue();
        return;
    }

    if (cmd == MotorProtocol::CmdWriteRegister && m_isWriteOp) {
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 RX ACK row=%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(row)
                   .arg(formatPayloadHex(data));
        emit rowWriteOk(row);
        processNextInQueue();
        return;
    }

    if (cmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
        qCWarning(lcRegister).noquote()
            << QStringLiteral("%1 RX row=%2 errorCode=0x%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(row)
                   .arg(errorCode, 2, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(data))
                   .toUpper();
        if (m_isWriteOp) {
            emit rowWriteError(row);
        } else {
            emit rowReadError(row);
        }
        processNextInQueue();
    }
}

void RegisterService::processNextInQueue() {
    if (m_dispatcher == nullptr) {
        clearQueue();
        return;
    }
    if (m_pendingQueue.isEmpty()) {
        m_timeoutTimer->stop();
        qCDebug(lcRegister).noquote()
            << QStringLiteral("Queue done (%1)")
                   .arg(m_isWriteOp ? QStringLiteral("write") : QStringLiteral("read"));
        m_hasPending = false;
        emit queueFinished(m_isWriteOp);
        return;
    }

    m_currentRequest = m_pendingQueue.dequeue();
    m_hasPending = true;
    m_isWriteOp = m_currentRequest.isWrite;
    if (!m_isWriteOp) {
        emit rowPending(m_currentRequest.globalRow);
    }
    submitCurrentRequest();
}

void RegisterService::onCommandDispatched(uint32_t ticket, uint8_t cmd, uint8_t seq) {
    Q_UNUSED(cmd);
    Q_UNUSED(seq);
    if (ticket == m_activeTicket) {
        m_timeoutTimer->start(500);
    }
}

void RegisterService::onTimeout() {
    if (!m_hasPending) {
        return;
    }

    const int row = m_currentRequest.globalRow;
    qCWarning(lcRegister).noquote()
        << QStringLiteral("Timeout waiting for %1 row=%2 addr=0x%3")
               .arg(QString::fromLatin1(MotorProtocol::commandName(
                   m_isWriteOp ? MotorProtocol::CmdWriteRegister : MotorProtocol::CmdReadRegister)))
               .arg(row)
               .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
               .toUpper();
    if (m_isWriteOp) {
        emit rowWriteError(row);
    } else {
        emit rowReadError(row);
    }
    ++m_sessionId;
    m_activeTicket = 0;
    processNextInQueue();
}

void RegisterService::clearQueue() {
    m_timeoutTimer->stop();
    ++m_sessionId;
    m_activeTicket = 0;
    m_hasPending = false;
    m_pendingQueue.clear();
}
