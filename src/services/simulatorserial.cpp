#include "services/simulatorserial.h"

#include <QIODevice>
#include <QMetaObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>

namespace {
QString makeSerialErrorMessage(const QString &prefix, const QString &detail) {
    if (detail.isEmpty()) {
        return prefix;
    }
    return QStringLiteral("%1: %2").arg(prefix, detail);
}
} // namespace

SimulatorSerial::SimulatorSerial(QObject *parent)
    : QObject(nullptr) {
    Q_UNUSED(parent);

    m_thread = new QThread();
    connect(m_thread, &QThread::started, this, &SimulatorSerial::init);

    moveToThread(m_thread);
    m_thread->start();
}

SimulatorSerial::~SimulatorSerial() {
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

QStringList SimulatorSerial::availablePorts() {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports.append(info.portName());
    }
    ports.sort();
    return ports;
}

void SimulatorSerial::init() {
    if (m_serial != nullptr) {
        return;
    }

    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &SimulatorSerial::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        onSerialErrorOccurred(static_cast<int>(error));
    });
}

void SimulatorSerial::openPort(const QString &portName, qint32 baudRate) {
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

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QStringLiteral("Failed to open %1: %2").arg(portName, m_serial->errorString()));
        return;
    }

    m_parser.reset();
    emit connected();
}

void SimulatorSerial::closePort() {
    closePortInternal(true);
}

void SimulatorSerial::sendRawFrame(const QByteArray &frame) {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        emit errorOccurred(QStringLiteral("Simulator serial port is not open"));
        return;
    }

    m_serial->write(frame);
}

void SimulatorSerial::onReadyRead() {
    if (m_serial == nullptr) {
        return;
    }

    const QByteArray bytes = m_serial->readAll();
    for (char byte : bytes) {
        const FrameParser::FrameType frameType = m_parser.feedByte(static_cast<uint8_t>(byte));
        if (frameType != FrameParser::FrameType::Control) {
            continue;
        }

        const ControlFrame &frame = m_parser.controlFrame();
        emit frameReceived(frame.cmd, frame.seq, frame.data);
    }
}

void SimulatorSerial::onSerialErrorOccurred(int error) {
    const auto serialError = static_cast<QSerialPort::SerialPortError>(error);
    if (serialError == QSerialPort::NoError || m_serial == nullptr) {
        return;
    }

    emit errorOccurred(makeSerialErrorMessage(QStringLiteral("Simulator serial error"), m_serial->errorString()));

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

void SimulatorSerial::closePortInternal(bool emitDisconnected) {
    m_parser.reset();

    const bool wasOpen = (m_serial != nullptr && m_serial->isOpen());
    if (wasOpen) {
        m_serial->close();
    }

    if (emitDisconnected && wasOpen) {
        emit disconnected();
    }
}
