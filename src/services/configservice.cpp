// ConfigService — 串口连接管理、I2C 扫描、IC 地址设置、帧响应处理
#include "services/configservice.h"

#include "devicecontext.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QDebug>
#include <QMetaObject>



ConfigService::ConfigService(SerialManager *serialManager, DeviceContext *deviceContext, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_deviceContext(deviceContext) {
    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::connected, this, &ConfigService::onSerialConnected);
        connect(m_serialManager, &SerialManager::disconnected, this, &ConfigService::onSerialDisconnected);
        connect(m_serialManager, &SerialManager::errorOccurred, this, &ConfigService::serialError);
        connect(m_serialManager, &SerialManager::frameReceived, this, &ConfigService::onFrameReceived);
    }
}

ConfigService::~ConfigService() = default;

QStringList ConfigService::availablePorts() const {
    return SerialManager::availablePorts();
}

void ConfigService::connectToPort(const QString &portName, qint32 baudRate) {
    if (m_serialManager == nullptr) return;
    if (m_isConnected) {
        qWarning() << "Already connected, disconnect first";
        return;
    }
    m_connectedPort = portName;
    m_connectedBaud = baudRate;
    qDebug().noquote() << QStringLiteral("Connecting to %1 at %2...").arg(portName).arg(baudRate);
    QMetaObject::invokeMethod(m_serialManager, "openPort", Qt::QueuedConnection,
        Q_ARG(QString, portName), Q_ARG(qint32, baudRate));
}

void ConfigService::disconnectPort() {
    if (m_serialManager == nullptr) return;
    qDebug() << "Disconnecting...";
    QMetaObject::invokeMethod(m_serialManager, "closePort", Qt::QueuedConnection);
}

void ConfigService::startI2cScan() {
    if (m_serialManager == nullptr || !m_isConnected) {
        qWarning() << "I2C scan failed: serial not connected";
        return;
    }
    qDebug() << "I2C scan started on bus I2C2";
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdI2cBusScan),
        Q_ARG(QByteArray, MotorProtocol::encodeI2cBusScan()));
}

void ConfigService::setMotorIcAddress(uint8_t addr) {
    if (m_serialManager == nullptr || !m_isConnected) {
        qWarning() << "IC connect failed: serial not connected";
        return;
    }
    qDebug().noquote() << QStringLiteral("Setting motor IC address to 0x%1")
                              .arg(addr, 2, 16, QLatin1Char('0')).toUpper();
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdSetMotorIcAddr),
        Q_ARG(QByteArray, MotorProtocol::encodeSetMotorIcAddr(addr)));
}

void ConfigService::onSerialConnected() {
    qDebug() << "Serial connected";
    m_isConnected = true;
    emit serialConnected(m_connectedPort, m_connectedBaud);
}

void ConfigService::onSerialDisconnected() {
    qDebug() << "Serial disconnected";
    m_isConnected = false;
    emit serialDisconnected();
}

void ConfigService::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);
    switch (cmd) {
    case MotorProtocol::CmdI2cBusScan: {
        if (data.isEmpty()) {
            qWarning() << "I2C scan response: empty data";
            return;
        }
        const QList<uint8_t> addresses = MotorProtocol::decodeI2cScanResponse(data);
        qDebug().noquote() << QStringLiteral("I2C scan found %1 devices").arg(addresses.size());
        for (uint8_t addr : addresses) {
            qDebug().noquote() << QStringLiteral("  Device: 0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper();
        }
        if (addresses.isEmpty()) qDebug() << "No I2C devices found";
        emit i2cScanResult(addresses);
        break;
    }
    case MotorProtocol::CmdSetMotorIcAddr: {
        qDebug().noquote() << QStringLiteral("Motor IC address set successfully");
        emit setAddrSuccess();
        break;
    }
    case MotorProtocol::CmdErrorResponse: {
        const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
        QString errorName;
        switch (errorCode) {
        case 0x01: errorName = QStringLiteral("CRC check failed"); break;
        case 0x02: errorName = QStringLiteral("Unknown command"); break;
        case 0x03: errorName = QStringLiteral("Execution failed"); break;
        default: errorName = QStringLiteral("Unknown error"); break;
        }
        qWarning().noquote() << QStringLiteral("Error response: code=0x%1 (%2)")
                                    .arg(errorCode, 2, 16, QLatin1Char('0')).toUpper()
                                    .arg(errorName);
        emit protocolError(errorCode);
        break;
    }
    default:
        break;
    }
}
