// ConfigService — 串口连接管理、I2C 扫描、IC 地址设置、帧响应处理
#include "services/configservice.h"

#include "devicecontext.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QtMath>

Q_LOGGING_CATEGORY(lcConfig, "motordev.config")

namespace {
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
}

ConfigService::ConfigService(SerialManager *serialManager, DeviceContext *deviceContext, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_deviceContext(deviceContext) {
    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::connected, this, &ConfigService::onSerialConnected);
        connect(m_serialManager, &SerialManager::disconnected, this, &ConfigService::onSerialDisconnected);
        connect(m_serialManager, &SerialManager::errorOccurred, this, [this](const QString &message) {
            if (m_pmicState != PmicState::Idle) {
                m_pmicState = PmicState::Idle;
                emit pmicConfigFailed(message);
            }
            emit serialError(message);
        });
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
        qCWarning(lcConfig) << "Already connected, disconnect first";
        return;
    }
    m_connectedPort = portName;
    m_connectedBaud = baudRate;
    qCInfo(lcConfig).noquote() << QStringLiteral("Connect request port=%1 baud=%2").arg(portName).arg(baudRate);
    QMetaObject::invokeMethod(m_serialManager, "openPort", Qt::QueuedConnection,
        Q_ARG(QString, portName), Q_ARG(qint32, baudRate));
}

void ConfigService::disconnectPort() {
    if (m_serialManager == nullptr) return;
    qCInfo(lcConfig) << "Disconnect request";
    QMetaObject::invokeMethod(m_serialManager, "closePort", Qt::QueuedConnection);
}

void ConfigService::startI2cScan() {
    if (m_serialManager == nullptr || !m_isConnected) {
        qCWarning(lcConfig) << "I2cBusScan blocked: serial not connected";
        return;
    }
    const QByteArray payload = MotorProtocol::encodeI2cBusScan();
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX bus=I2C2 payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdI2cBusScan)))
               .arg(formatPayloadHex(payload));
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdI2cBusScan),
        Q_ARG(QByteArray, payload));
}

void ConfigService::setMotorIcAddress(uint8_t addr) {
    if (m_serialManager == nullptr || !m_isConnected) {
        qCWarning(lcConfig) << "SetMotorIcAddr blocked: serial not connected";
        return;
    }
    const QByteArray payload = MotorProtocol::encodeSetMotorIcAddr(addr);
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX addr=0x%2 payload=%3")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdSetMotorIcAddr)))
               .arg(addr, 2, 16, QLatin1Char('0'))
               .arg(formatPayloadHex(payload))
               .toUpper();
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdSetMotorIcAddr),
        Q_ARG(QByteArray, payload));
}

void ConfigService::configurePmic(double drvvddV, double iovddV, double vcmvddV) {
    if (m_serialManager == nullptr || !m_isConnected) {
        const QString reason = QStringLiteral("Serial not connected");
        emit pmicConfigFailed(reason);
        return;
    }

    if (m_pmicState != PmicState::Idle) {
        qCWarning(lcConfig) << "PMIC configuration ignored: sequence already in progress";
        return;
    }

    const quint16 drvvdd = static_cast<quint16>(qRound(drvvddV * 100.0));
    const quint16 iovdd = static_cast<quint16>(qRound(iovddV * 100.0));
    const quint16 vcmvdd = static_cast<quint16>(qRound(vcmvddV * 100.0));

    constexpr quint16 kMinPmicVoltage = 60;
    constexpr quint16 kMaxPmicVoltage = 377;
    if (drvvdd < kMinPmicVoltage || drvvdd > kMaxPmicVoltage ||
        iovdd < kMinPmicVoltage || iovdd > kMaxPmicVoltage ||
        vcmvdd < kMinPmicVoltage || vcmvdd > kMaxPmicVoltage) {
        const QString reason = QStringLiteral("Voltage out of range");
        emit pmicConfigFailed(reason);
        return;
    }

    const QByteArray payload = MotorProtocol::encodePmicVoltage(drvvdd, iovdd, vcmvdd);
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX DRVDD=%2V IOVDD=%3V VCMVDD=%4V payload=%5")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdSetPmicVoltage)))
               .arg(drvvddV, 0, 'f', 2)
               .arg(iovddV, 0, 'f', 2)
               .arg(vcmvddV, 0, 'f', 2)
               .arg(formatPayloadHex(payload));
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdSetPmicVoltage),
        Q_ARG(QByteArray, payload));
    m_pmicState = PmicState::WaitingVoltageAck;
}

void ConfigService::onSerialConnected() {
    qCInfo(lcConfig) << "Serial connected";
    m_isConnected = true;
    emit serialConnected(m_connectedPort, m_connectedBaud);
}

void ConfigService::onSerialDisconnected() {
    qCInfo(lcConfig) << "Serial disconnected";
    m_isConnected = false;
    m_pmicState = PmicState::Idle;
    emit serialDisconnected();
}

void ConfigService::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);
    switch (cmd) {
    case MotorProtocol::CmdI2cBusScan: {
        if (data.isEmpty()) {
            qCWarning(lcConfig) << "I2cBusScan response empty";
            return;
        }
        const QList<uint8_t> addresses = MotorProtocol::decodeI2cScanResponse(data);
        QStringList addressTexts;
        for (uint8_t addr : addresses) {
            addressTexts.append(QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper());
        }
        qCInfo(lcConfig).noquote()
            << QStringLiteral("%1 RX count=%2 devices=%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(addresses.size())
                   .arg(addressTexts.isEmpty() ? QStringLiteral("<none>") : addressTexts.join(QStringLiteral(", ")))
                   .arg(formatPayloadHex(data));
        emit i2cScanResult(addresses);
        break;
    }
    case MotorProtocol::CmdSetMotorIcAddr: {
        qCInfo(lcConfig).noquote()
            << QStringLiteral("%1 RX ACK payload=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(formatPayloadHex(data));
        emit setAddrSuccess();
        break;
    }
    case MotorProtocol::CmdSetPmicVoltage: {
        if (m_pmicState != PmicState::WaitingVoltageAck) {
            break;
        }
        qCInfo(lcConfig).noquote()
            << QStringLiteral("%1 RX ACK payload=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(formatPayloadHex(data));
        const QByteArray payload = MotorProtocol::encodePmicEnable();
        qCInfo(lcConfig).noquote()
            << QStringLiteral("%1 TX payload=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdPmicEnable)))
                   .arg(formatPayloadHex(payload));
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
            Q_ARG(uint8_t, MotorProtocol::CmdPmicEnable),
            Q_ARG(QByteArray, payload));
        m_pmicState = PmicState::WaitingEnableAck;
        break;
    }
    case MotorProtocol::CmdPmicEnable: {
        if (m_pmicState != PmicState::WaitingEnableAck) {
            break;
        }
        m_pmicState = PmicState::Idle;
        qCInfo(lcConfig).noquote()
            << QStringLiteral("%1 RX ACK payload=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(formatPayloadHex(data));
        emit pmicConfigSuccess();
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
        qCWarning(lcConfig).noquote()
            << QStringLiteral("%1 RX errorCode=0x%2 (%3) payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(errorCode, 2, 16, QLatin1Char('0'))
                   .arg(errorName)
                   .arg(formatPayloadHex(data))
                   .toUpper();
        if (m_pmicState != PmicState::Idle) {
            m_pmicState = PmicState::Idle;
            emit pmicConfigFailed(errorName);
        }
        emit protocolError(errorCode);
        break;
    }
    default:
        break;
    }
}
