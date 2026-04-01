#include "serialmanager.h"

#include <QDebug>
#include <QMetaObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QTimer>

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
} // namespace

SerialManager::SerialManager(QObject *parent)
    : QObject(nullptr) {
    Q_UNUSED(parent);

    qRegisterMetaType<QVector<int16_t>>("QVector<int16_t>");

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

    qDebug().noquote() << QStringLiteral("Opening port %1 at %2 (8N1)...")
                              .arg(portName)
                              .arg(baudRate);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        const QString message = QStringLiteral("Failed to open %1: %2").arg(portName, m_serial->errorString());
        qWarning().noquote() << message;
        emit errorOccurred(message);
        return;
    }

    m_parser.reset();
    clearPendingCommand();
    m_missedHeartbeats = 0;
    qDebug().noquote() << QStringLiteral("Port %1 opened successfully").arg(portName);
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
    qDebug() << "Heartbeat started";
}

void SerialManager::stopHeartbeat() {
    stopHeartbeatTimer();
    m_missedHeartbeats = 0;
    qDebug() << "Heartbeat stopped";
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

    qDebug().noquote() << QStringLiteral("TX control frame: seq=%1 cmd=%2 len=%3")
                              .arg(formatByte(seq))
                              .arg(formatByte(cmd))
                              .arg(data.size());
    m_serial->write(m_pendingFrame);
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
            qDebug().noquote() << QStringLiteral("RX stream frame: channels=%1 samples=%2")
                                      .arg(formatByte(frame.channelMask))
                                      .arg(frame.samples.size());
            emit streamDataReceived(frame.channelMask, frame.samples);
        }
    }
}

void SerialManager::onRetryTimeout() {
    if (m_serial == nullptr || !m_serial->isOpen() || m_pendingFrame.isEmpty()) {
        return;
    }

    if (m_retryCount >= MaxRetries) {
        qWarning().noquote() << QStringLiteral("Command timeout: seq=%1 cmd=%2 after 3 attempts")
                                    .arg(formatByte(m_pendingSeq))
                                    .arg(formatByte(m_pendingCmd));
        clearPendingCommand();
        emit errorOccurred(QStringLiteral("Command timeout"));
        return;
    }

    ++m_retryCount;
    qWarning().noquote() << QStringLiteral("Retry #%1 for seq=%2 cmd=%3")
                                .arg(m_retryCount)
                                .arg(formatByte(m_pendingSeq))
                                .arg(formatByte(m_pendingCmd));
    m_serial->write(m_pendingFrame);
    if (m_retryTimer != nullptr) {
        m_retryTimer->start(RetryTimeoutMs);
    }
}

void SerialManager::onHeartbeatTimeout() {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        return;
    }

    const QByteArray heartbeatFrame = FrameParser::encodeControlFrame(m_nextSeq++, HeartbeatCommand, {});
    m_serial->write(heartbeatFrame);

    ++m_missedHeartbeats;
    qDebug().noquote() << QStringLiteral("Heartbeat sent (missed: %1)").arg(m_missedHeartbeats);
    if (m_missedHeartbeats >= MaxMissedHeartbeats) {
        qWarning().noquote() << QStringLiteral("Heartbeat lost: %1 missed, disconnecting")
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
    qWarning().noquote() << message;
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
    if (frame.cmd == HeartbeatCommand) {
        qDebug() << "Heartbeat response received";
        m_missedHeartbeats = 0;
        return;
    }

    qDebug().noquote() << QStringLiteral("RX control frame: seq=%1 cmd=%2 len=%3")
                              .arg(formatByte(frame.seq))
                              .arg(formatByte(frame.cmd))
                              .arg(frame.data.size());

    if (frame.cmd == ErrorResponseCommand) {
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
        qDebug() << "Port closed";
    }

    if (emitDisconnectedSignal && wasOpen) {
        emit disconnected();
    }
}
