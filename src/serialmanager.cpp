#include "serialmanager.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QTimer>

Q_LOGGING_CATEGORY(lcSerialManager, "motordev.serial")

namespace {
QString makeSerialErrorMessage(const QString &prefix, const QString &detail) {
    if (detail.isEmpty()) {
        return prefix;
    }
    return QStringLiteral("%1: %2").arg(prefix, detail);
}

QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
} // namespace

SerialManager::SerialManager(QObject *parent)
    : QObject(nullptr) {
    Q_UNUSED(parent);

    qRegisterMetaType<QVector<int16_t>>("QVector<int16_t>");

    // QThread must not have a parent when the QObject using it calls
    // moveToThread() — Qt forbids moving objects with a parent to
    // another thread. Manually deleted in ~SerialManager().
    m_thread = new QThread();
    connect(m_thread, &QThread::started, this, &SerialManager::init);

    moveToThread(m_thread);
    m_thread->start();
}

SerialManager::~SerialManager() {
    if (m_thread == nullptr) {
        return;
    }

    if (m_thread->isRunning()) {
        QMetaObject::invokeMethod(this, [this]() { closePortInternal(false); }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }

    delete m_thread;
    m_thread = nullptr;
}

QStringList SerialManager::availablePorts() {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports.append(info.portName());
    }
    ports.sort();
    return ports;
}

void SerialManager::init() {
    if (m_serial == nullptr) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
            onSerialErrorOccurred(static_cast<int>(error));
        });
    }

    if (m_retryTimer == nullptr) {
        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        connect(m_retryTimer, &QTimer::timeout, this, &SerialManager::onRetryTimeout);
    }

    if (m_heartbeatTimer == nullptr) {
        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(HeartbeatIntervalMs);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &SerialManager::onHeartbeatTimeout);
    }
}

void SerialManager::openPort(const QString &portName, qint32 baudRate) {
    if (m_serial == nullptr) {
        init();
    }

    if (m_serial->isOpen()) {
        closePortInternal(true);
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    qCInfo(lcSerialManager).noquote() << QStringLiteral("Opening port %1 at %2 (8N1)...")
                              .arg(portName)
                              .arg(baudRate);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        const QString message = QStringLiteral("Failed to open %1: %2").arg(portName, m_serial->errorString());
        qCWarning(lcSerialManager).noquote() << message;
        emit errorOccurred(message);
        return;
    }

    m_parser.reset();
    clearPendingCommand();
    m_missedHeartbeats = 0;
    qCInfo(lcSerialManager).noquote() << QStringLiteral("Port %1 opened successfully").arg(portName);
    emit connected();
}

void SerialManager::closePort() {
    closePortInternal(true);
}

void SerialManager::startHeartbeat() {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        return;
    }

    m_missedHeartbeats = 0;
    if (m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->start();
    }
    qCInfo(lcSerialManager) << "Heartbeat started";
}

void SerialManager::stopHeartbeat() {
    stopHeartbeatTimer();
    m_missedHeartbeats = 0;
    qCInfo(lcSerialManager) << "Heartbeat stopped";
}

void SerialManager::sendCommand(uint8_t cmd, const QByteArray &data) {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        emit errorOccurred(QStringLiteral("Serial port is not open"));
        return;
    }

    if (!m_pendingFrame.isEmpty()) {
        emit errorOccurred(QStringLiteral("Another command is pending"));
        return;
    }

    const uint8_t seq = m_nextSeq++;
    m_pendingFrame = FrameParser::encodeControlFrame(seq, cmd, data);
    m_pendingSeq = seq;
    m_pendingCmd = cmd;
    m_retryCount = 0;

    qCDebug(lcSerialManager).noquote()
        << QStringLiteral("TX control frame: seq=%1 cmd=%2(%3) len=%4 payload=%5")
                              .arg(formatByte(seq))
                              .arg(formatByte(cmd))
                              .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                              .arg(data.size())
                              .arg(formatPayloadHex(data));
    m_serial->write(m_pendingFrame);
    emit commandSent(cmd, seq);
    if (m_retryTimer != nullptr) {
        m_retryTimer->start(RetryTimeoutMs);
    }
}

void SerialManager::onReadyRead() {
    if (m_serial == nullptr) {
        return;
    }

    const QByteArray bytes = m_serial->readAll();
    for (char byte : bytes) {
        const FrameParser::FrameType frameType = m_parser.feedByte(static_cast<uint8_t>(byte));
        if (frameType == FrameParser::FrameType::Control) {
            handleControlFrame(m_parser.controlFrame());
        } else if (frameType == FrameParser::FrameType::Stream) {
            const StreamFrame &frame = m_parser.streamFrame();
            emit streamDataReceived(frame.channelMask, frame.samples);
        }
    }
}

void SerialManager::onRetryTimeout() {
    if (m_serial == nullptr || !m_serial->isOpen() || m_pendingFrame.isEmpty()) {
        return;
    }

    if (m_retryCount >= MaxRetries) {
        qCWarning(lcSerialManager).noquote()
            << QStringLiteral("Command timeout: seq=%1 cmd=%2(%3) after %4 attempts")
                                    .arg(formatByte(m_pendingSeq))
                                    .arg(formatByte(m_pendingCmd))
                                    .arg(QString::fromLatin1(MotorProtocol::commandName(m_pendingCmd)))
                                    .arg(MaxRetries + 1);
        clearPendingCommand();
        emit errorOccurred(QStringLiteral("Command timeout"));
        return;
    }

    ++m_retryCount;
    qCWarning(lcSerialManager).noquote()
        << QStringLiteral("Retry #%1 for seq=%2 cmd=%3(%4)")
                                .arg(m_retryCount)
                                .arg(formatByte(m_pendingSeq))
                                .arg(formatByte(m_pendingCmd))
                                .arg(QString::fromLatin1(MotorProtocol::commandName(m_pendingCmd)));
    m_serial->write(m_pendingFrame);
    if (m_retryTimer != nullptr) {
        m_retryTimer->start(RetryTimeoutMs);
    }
}

void SerialManager::onHeartbeatTimeout() {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        return;
    }

    // Heartbeat is sent regardless of pending command state.
    // Response matching is safe: handleControlFrame() dispatches heartbeat
    // responses by cmd==0x00 before checking seq, so no conflict with
    // pending command responses.
    const QByteArray heartbeatFrame = FrameParser::encodeControlFrame(m_nextSeq++, MotorProtocol::CmdHeartbeat, {});
    m_serial->write(heartbeatFrame);

    ++m_missedHeartbeats;
    qCDebug(lcSerialManager).noquote() << QStringLiteral("Heartbeat sent (missed: %1)").arg(m_missedHeartbeats);
    if (m_missedHeartbeats >= MaxMissedHeartbeats) {
        qCWarning(lcSerialManager).noquote() << QStringLiteral("Heartbeat lost: %1 missed, disconnecting")
                                    .arg(m_missedHeartbeats);
        closePortInternal(true);
    }
}

void SerialManager::onSerialErrorOccurred(int error) {
    const auto serialError = static_cast<QSerialPort::SerialPortError>(error);
    if (serialError == QSerialPort::NoError || m_serial == nullptr) {
        return;
    }

    const QString message = makeSerialErrorMessage(QStringLiteral("Serial port error"), m_serial->errorString());
    qCWarning(lcSerialManager).noquote() << message;
    emit errorOccurred(message);

    switch (serialError) {
    case QSerialPort::ResourceError:
    case QSerialPort::PermissionError:
    case QSerialPort::DeviceNotFoundError:
        closePortInternal(true);
        break;
    default:
        break;
    }
}

void SerialManager::clearPendingCommand() {
    stopRetryTimer();
    m_pendingFrame.clear();
    m_pendingSeq = 0;
    m_pendingCmd = 0;
    m_retryCount = 0;
}

void SerialManager::stopRetryTimer() {
    if (m_retryTimer != nullptr) {
        m_retryTimer->stop();
    }
}

void SerialManager::stopHeartbeatTimer() {
    if (m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->stop();
    }
}

void SerialManager::handleControlFrame(const ControlFrame &frame) {
    if (frame.cmd == MotorProtocol::CmdHeartbeat) {
        qCDebug(lcSerialManager) << "Heartbeat response received";
        m_missedHeartbeats = 0;
        return;
    }

    qCDebug(lcSerialManager).noquote()
        << QStringLiteral("RX control frame: seq=%1 cmd=%2(%3) len=%4 payload=%5")
                              .arg(formatByte(frame.seq))
                              .arg(formatByte(frame.cmd))
                              .arg(QString::fromLatin1(MotorProtocol::commandName(frame.cmd)))
                              .arg(frame.data.size())
                              .arg(formatPayloadHex(frame.data));

    if (frame.cmd == MotorProtocol::CmdErrorResponse) {
        clearPendingCommand();
        emit frameReceived(frame.cmd, frame.seq, frame.data);
        return;
    }

    if (m_pendingFrame.isEmpty()) {
        emit frameReceived(frame.cmd, frame.seq, frame.data);
        return;
    }

    if (frame.seq == m_pendingSeq) {
        clearPendingCommand();
        emit frameReceived(frame.cmd, frame.seq, frame.data);
    }
}

void SerialManager::closePortInternal(bool emitDisconnectedSignal) {
    stopHeartbeatTimer();
    clearPendingCommand();
    m_parser.reset();
    m_missedHeartbeats = 0;

    const bool wasOpen = (m_serial != nullptr && m_serial->isOpen());
    if (wasOpen) {
        m_serial->close();
        qCInfo(lcSerialManager) << "Port closed";
    }

    if (emitDisconnectedSignal && wasOpen) {
        emit disconnected();
    }
}
