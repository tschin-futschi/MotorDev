// RegisterService — 寄存器读写命令队列、协议收发、超时管理
#include "services/registerservice.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QTimer>

Q_LOGGING_CATEGORY(lcRegister, "motordev.register")

namespace {
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
}

RegisterService::RegisterService(SerialManager *serialManager, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager) {
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RegisterService::onTimeout);
    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::frameReceived, this, &RegisterService::onFrameReceived);
        connect(m_serialManager, &SerialManager::errorOccurred, this, &RegisterService::onSerialError);
    }
}

RegisterService::~RegisterService() = default;

void RegisterService::readSingleRow(int globalRow, quint16 addr) {
    const QByteArray payload = MotorProtocol::encodeReadRegister(addr);
    qCInfo(lcRegister).noquote()
        << QStringLiteral("%1 TX row=%2 addr=0x%3 payload=%4")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdReadRegister)))
               .arg(globalRow)
               .arg(addr, 4, 16, QLatin1Char('0'))
               .arg(formatPayloadHex(payload))
               .toUpper();
    RowRequest req;
    req.globalRow = globalRow;
    req.addr = addr;
    clearQueue();
    m_isWriteOp = false;
    m_currentRequest = req;
    m_hasPending = true;
    emit rowPending(globalRow);
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdReadRegister),
        Q_ARG(QByteArray, payload));
    m_timeoutTimer->start(500);
}

void RegisterService::writeSingleRow(int globalRow, quint16 addr, qint16 value) {
    const QByteArray payload = MotorProtocol::encodeWriteRegister(addr, value);
    qCInfo(lcRegister).noquote()
        << QStringLiteral("%1 TX row=%2 addr=0x%3 value=0x%4 payload=%5")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdWriteRegister)))
               .arg(globalRow)
               .arg(addr, 4, 16, QLatin1Char('0'))
               .arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0'))
               .arg(formatPayloadHex(payload))
               .toUpper();
    RowRequest req;
    req.globalRow = globalRow;
    req.addr = addr;
    req.value = value;
    clearQueue();
    m_isWriteOp = true;
    m_currentRequest = req;
    m_hasPending = true;
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdWriteRegister),
        Q_ARG(QByteArray, payload));
    m_timeoutTimer->start(500);
}

void RegisterService::readBatch(const QVector<RowRequest> &rows) {
    enqueueAndStart(rows, false);
}

void RegisterService::writeBatch(const QVector<RowRequest> &rows) {
    enqueueAndStart(rows, true);
}

void RegisterService::enqueueAndStart(const QVector<RowRequest> &rows, bool isWrite) {
    clearQueue();
    m_isWriteOp = isWrite;
    for (const RowRequest &r : rows) {
        m_pendingQueue.enqueue(r);
    }
    processNextInQueue();
}

void RegisterService::processNextInQueue() {
    if (m_serialManager == nullptr) {
        clearQueue();
        return;
    }
    if (m_pendingQueue.isEmpty()) {
        m_timeoutTimer->stop();
        qCDebug(lcRegister).noquote() << QStringLiteral("Queue done (%1)")
                                  .arg(m_isWriteOp ? QStringLiteral("write") : QStringLiteral("read"));
        m_hasPending = false;
        emit queueFinished(m_isWriteOp);
        return;
    }
    m_currentRequest = m_pendingQueue.dequeue();
    m_hasPending = true;
    if (m_isWriteOp) {
        const QByteArray payload = MotorProtocol::encodeWriteRegister(m_currentRequest.addr, m_currentRequest.value);
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 TX row=%2 addr=0x%3 value=0x%4 payload=%5")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdWriteRegister)))
                   .arg(m_currentRequest.globalRow)
                   .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
                   .arg(static_cast<quint16>(m_currentRequest.value), 4, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(payload))
                   .toUpper();
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
            Q_ARG(uint8_t, MotorProtocol::CmdWriteRegister),
            Q_ARG(QByteArray, payload));
    } else {
        emit rowPending(m_currentRequest.globalRow);
        const QByteArray payload = MotorProtocol::encodeReadRegister(m_currentRequest.addr);
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 TX row=%2 addr=0x%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdReadRegister)))
                   .arg(m_currentRequest.globalRow)
                   .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(payload))
                   .toUpper();
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
            Q_ARG(uint8_t, MotorProtocol::CmdReadRegister),
            Q_ARG(QByteArray, payload));
    }
    m_timeoutTimer->start(500);
}

void RegisterService::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);
    if (!m_hasPending) return;
    const int row = m_currentRequest.globalRow;
    if (cmd == MotorProtocol::CmdReadRegister && !m_isWriteOp) {
        m_timeoutTimer->stop();
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
    } else if (cmd == MotorProtocol::CmdWriteRegister && m_isWriteOp) {
        m_timeoutTimer->stop();
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 RX ACK row=%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(row)
                   .arg(formatPayloadHex(data));
        emit rowWriteOk(row);
        processNextInQueue();
    } else if (cmd == MotorProtocol::CmdErrorResponse) {
        m_timeoutTimer->stop();
        const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
        qCWarning(lcRegister).noquote()
            << QStringLiteral("%1 RX row=%2 errorCode=0x%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(row)
                   .arg(errorCode, 2, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(data))
                   .toUpper();
        if (m_isWriteOp) emit rowWriteError(row);
        else emit rowReadError(row);
        processNextInQueue();
    }
}

void RegisterService::onSerialError(const QString &message) {
    qCWarning(lcRegister).noquote() << QStringLiteral("Serial error: %1").arg(message);
    m_timeoutTimer->stop();
    if (m_hasPending) {
        const int row = m_currentRequest.globalRow;
        if (m_isWriteOp) emit rowWriteError(row);
        else emit rowReadError(row);
    }
    clearQueue();
}

void RegisterService::onTimeout() {
    if (!m_hasPending) return;
    const int row = m_currentRequest.globalRow;
    qCWarning(lcRegister).noquote()
        << QStringLiteral("Timeout waiting for %1 row=%2 addr=0x%3")
               .arg(QString::fromLatin1(MotorProtocol::commandName(
                   m_isWriteOp ? MotorProtocol::CmdWriteRegister : MotorProtocol::CmdReadRegister)))
               .arg(row)
               .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
               .toUpper();
    if (m_isWriteOp) emit rowWriteError(row);
    else emit rowReadError(row);
    processNextInQueue();
}

void RegisterService::clearQueue() {
    m_timeoutTimer->stop();
    m_hasPending = false;
    m_pendingQueue.clear();
}
