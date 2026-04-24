// ConfigService — 串口连接管理、I2C 扫描、IC 地址设置、帧响应处理
#include "services/configservice.h"

#include "devicecontext.h"
#include "protocol/motor_protocol.h"
#include "services/commanddispatcher.h"
#include "serialmanager.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QtMath>

Q_LOGGING_CATEGORY(lcConfig, "motordev.config")

namespace {
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
}

ConfigService::ConfigService(SerialManager *serialManager,
                             CommandDispatcher *dispatcher,
                             DeviceContext *deviceContext,
                             QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_dispatcher(dispatcher)
    , m_deviceContext(deviceContext) {
    m_pmicTimeoutTimer = new QTimer(this);
    m_pmicTimeoutTimer->setSingleShot(true);
    m_pmicTimeoutTimer->setInterval(5000);
    connect(m_pmicTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_pmicState == PmicState::Idle) {
            return;
        }
        qCWarning(lcConfig) << "PMIC configuration timeout, resetting state";
        m_pmicState = PmicState::Idle;
        emit pmicConfigFailed(QStringLiteral("Timeout"));
    });

    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::connected, this, &ConfigService::onSerialConnected);
        connect(m_serialManager, &SerialManager::disconnected, this, &ConfigService::onSerialDisconnected);
        connect(m_serialManager, &SerialManager::errorOccurred, this, [this](const QString &message) {
            const bool serialLevelError =
                message == QStringLiteral("Serial port is not open")
                || message.startsWith(QStringLiteral("Failed to open"))
                || message.startsWith(QStringLiteral("Serial port error"));
            if (serialLevelError || !m_isConnected) {
                emit serialError(message);
            }
        });
    }
}

ConfigService::~ConfigService() = default;

QStringList ConfigService::availablePorts() const {
    return SerialManager::availablePorts();
}

void ConfigService::connectToPort(const QString &portName, qint32 baudRate) {
    if (m_serialManager == nullptr) {
        return;
    }
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
    if (m_serialManager == nullptr) {
        return;
    }
    qCInfo(lcConfig) << "Disconnect request";
    QMetaObject::invokeMethod(m_serialManager, "closePort", Qt::QueuedConnection);
}

void ConfigService::startI2cScan() {
    if (m_dispatcher == nullptr || !m_isConnected) {
        qCWarning(lcConfig) << "I2cBusScan blocked: serial not connected";
        return;
    }

    const QByteArray payload = MotorProtocol::encodeI2cBusScan();
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX bus=I2C2 payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdI2cBusScan)))
               .arg(formatPayloadHex(payload));

    QPointer<ConfigService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdI2cBusScan, payload, CommandDispatcher::Normal,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            Q_UNUSED(seq);
            if (guard == nullptr) {
                return;
            }

            if (cmd == MotorProtocol::CmdErrorResponse) {
                const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
                qCWarning(lcConfig).noquote()
                    << QStringLiteral("%1 RX errorCode=0x%2 (%3) payload=%4")
                           .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                           .arg(errorCode, 2, 16, QLatin1Char('0'))
                           .arg(ConfigService::errorNameForCode(errorCode))
                           .arg(formatPayloadHex(data))
                           .toUpper();
                emit guard->protocolError(errorCode);
                return;
            }

            if (cmd != MotorProtocol::CmdI2cBusScan) {
                return;
            }
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
            emit guard->i2cScanResult(addresses);
        });
}

void ConfigService::setMotorIcAddress(uint8_t addr) {
    if (m_dispatcher == nullptr || !m_isConnected) {
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

    QPointer<ConfigService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdSetMotorIcAddr, payload, CommandDispatcher::Normal,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            Q_UNUSED(seq);
            if (guard == nullptr) {
                return;
            }

            if (cmd == MotorProtocol::CmdErrorResponse) {
                const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
                qCWarning(lcConfig).noquote()
                    << QStringLiteral("%1 RX errorCode=0x%2 (%3) payload=%4")
                           .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                           .arg(errorCode, 2, 16, QLatin1Char('0'))
                           .arg(ConfigService::errorNameForCode(errorCode))
                           .arg(formatPayloadHex(data))
                           .toUpper();
                emit guard->protocolError(errorCode);
                return;
            }

            if (cmd != MotorProtocol::CmdSetMotorIcAddr) {
                return;
            }

            qCInfo(lcConfig).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));
            emit guard->setAddrSuccess();
        });
}

void ConfigService::configurePmic(double drvvddV, double iovddV, double vcmvddV) {
    if (m_dispatcher == nullptr || !m_isConnected) {
        if (m_pmicTimeoutTimer != nullptr) {
            m_pmicTimeoutTimer->stop();
        }
        emit pmicConfigFailed(QStringLiteral("Serial not connected"));
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
        if (m_pmicTimeoutTimer != nullptr) {
            m_pmicTimeoutTimer->stop();
        }
        emit pmicConfigFailed(QStringLiteral("Voltage out of range"));
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

    m_pmicState = PmicState::WaitingVoltageAck;
    m_pmicTimeoutTimer->start();
    QPointer<ConfigService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdSetPmicVoltage, payload, CommandDispatcher::Normal,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            Q_UNUSED(seq);
            if (guard == nullptr || guard->m_pmicState != PmicState::WaitingVoltageAck) {
                return;
            }

            if (cmd == MotorProtocol::CmdErrorResponse) {
                const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
                const QString errorName = ConfigService::errorNameForCode(errorCode);
                qCWarning(lcConfig).noquote()
                    << QStringLiteral("%1 RX errorCode=0x%2 (%3) payload=%4")
                           .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                           .arg(errorCode, 2, 16, QLatin1Char('0'))
                           .arg(errorName)
                           .arg(formatPayloadHex(data))
                           .toUpper();
                guard->m_pmicTimeoutTimer->stop();
                guard->m_pmicState = PmicState::Idle;
                emit guard->protocolError(errorCode);
                emit guard->pmicConfigFailed(errorName);
                return;
            }

            if (cmd != MotorProtocol::CmdSetPmicVoltage) {
                return;
            }

            qCInfo(lcConfig).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));

            const QByteArray enablePayload = MotorProtocol::encodePmicEnable();
            qCInfo(lcConfig).noquote()
                << QStringLiteral("%1 TX payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdPmicEnable)))
                       .arg(formatPayloadHex(enablePayload));

            guard->m_pmicState = PmicState::WaitingEnableAck;
            guard->m_dispatcher->submitCommand(MotorProtocol::CmdPmicEnable, enablePayload, CommandDispatcher::Normal,
                [guard](uint8_t enableCmd, uint8_t enableSeq, const QByteArray &enableData) {
                    Q_UNUSED(enableSeq);
                    if (guard == nullptr || guard->m_pmicState != PmicState::WaitingEnableAck) {
                        return;
                    }

                    if (enableCmd == MotorProtocol::CmdErrorResponse) {
                        const uint8_t errorCode = MotorProtocol::decodeErrorCode(enableData);
                        const QString errorName = ConfigService::errorNameForCode(errorCode);
                        qCWarning(lcConfig).noquote()
                            << QStringLiteral("%1 RX errorCode=0x%2 (%3) payload=%4")
                                   .arg(QString::fromLatin1(MotorProtocol::commandName(enableCmd)))
                                   .arg(errorCode, 2, 16, QLatin1Char('0'))
                                   .arg(errorName)
                                   .arg(formatPayloadHex(enableData))
                                   .toUpper();
                        guard->m_pmicTimeoutTimer->stop();
                        guard->m_pmicState = PmicState::Idle;
                        emit guard->protocolError(errorCode);
                        emit guard->pmicConfigFailed(errorName);
                        return;
                    }

                    if (enableCmd != MotorProtocol::CmdPmicEnable) {
                        return;
                    }

                    guard->m_pmicState = PmicState::Idle;
                    guard->m_pmicTimeoutTimer->stop();
                    qCInfo(lcConfig).noquote()
                        << QStringLiteral("%1 RX ACK payload=%2")
                               .arg(QString::fromLatin1(MotorProtocol::commandName(enableCmd)))
                               .arg(formatPayloadHex(enableData));
                    emit guard->pmicConfigSuccess();
                });
        });
}

void ConfigService::onSerialConnected() {
    qCInfo(lcConfig) << "Serial connected";
    m_isConnected = true;
    emit serialConnected(m_connectedPort, m_connectedBaud);
}

void ConfigService::onSerialDisconnected() {
    qCInfo(lcConfig) << "Serial disconnected";
    m_isConnected = false;
    if (m_pmicTimeoutTimer != nullptr) {
        m_pmicTimeoutTimer->stop();
    }
    m_pmicState = PmicState::Idle;
    emit serialDisconnected();
}

QString ConfigService::errorNameForCode(uint8_t errorCode) {
    switch (errorCode) {
    case 0x01:
        return QStringLiteral("CRC check failed");
    case 0x02:
        return QStringLiteral("Unknown command");
    case 0x03:
        return QStringLiteral("Execution failed");
    case CommandDispatcher::LocalErrorCode:
        return QStringLiteral("Dispatcher transport error");
    default:
        return QStringLiteral("Unknown error");
    }
}
