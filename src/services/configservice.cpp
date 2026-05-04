// =============================================================================
// @file    configservice.cpp
// @brief   配置服务实现 — 串口连接、I2C 扫描、IC 地址、PMIC 配置、复位/电机测试
//
// ConfigService 封装了设备配置阶段的所有命令操作：
//   - 串口连接/断开（通过 SerialManager，跨线程 invokeMethod）
//   - I2C 总线扫描
//   - 电机 IC 地址设置
//   - PMIC 电压配置（两步序列：SetVoltage → PmicEnable）
//   - PMIC 关闭
//   - 设备复位
//   - 电机测试
//
// 所有命令通过 CommandDispatcher 异步提交，响应在 lambda 回调中处理。
// 使用 QPointer<ConfigService> 防止回调时对象已析构。
// =============================================================================
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
/// @brief 将字节数组格式化为大写十六进制字符串，用于日志输出。
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
}

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造配置服务，连接 SerialManager 信号并初始化 PMIC 超时定时器。
///
/// @param serialManager   串口管理器（运行在独立线程）
/// @param dispatcher      命令分发器
/// @param deviceContext   设备上下文（存储全局设备参数）
/// @param parent          父对象
ConfigService::ConfigService(SerialManager *serialManager,
                             CommandDispatcher *dispatcher,
                             DeviceContext *deviceContext,
                             QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_dispatcher(dispatcher)
    , m_deviceContext(deviceContext) {
    // ---------- PMIC 配置超时定时器（5 秒） ----------
    // PMIC 配置是两步序列（SetVoltage → Enable），整体超时 5 秒
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

    // ---------- 监听 SerialManager 连接状态 ----------
    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::connected, this, &ConfigService::onSerialConnected);
        connect(m_serialManager, &SerialManager::disconnected, this, &ConfigService::onSerialDisconnected);
        // 错误信号：区分串口级错误和命令级错误
        connect(m_serialManager, &SerialManager::errorOccurred, this, [this](const QString &message) {
            const bool serialLevelError =
                message == QStringLiteral("Serial port is not open")
                || message.startsWith(QStringLiteral("Failed to open"))
                || message.startsWith(QStringLiteral("Serial port error"));
            // 串口级错误始终上报；命令级错误仅在未连接时上报
            if (serialLevelError || !m_isConnected) {
                emit serialError(message);
            }
        });
    }
}

ConfigService::~ConfigService() = default;

// =============================================================================
// 串口连接管理
// =============================================================================

/// @brief 返回系统可用串口列表（委托给 SerialManager 静态方法）。
QStringList ConfigService::availablePorts() const {
    return SerialManager::availablePorts();
}

/// @brief 请求连接到指定串口。
///
/// 通过 QueuedConnection 调用 SerialManager::openPort，
/// 因为 SerialManager 运行在独立线程中。
///
/// @param portName  串口名称（如 "COM3"）
/// @param baudRate  波特率
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
    // 跨线程调用：SerialManager 在 worker 线程
    QMetaObject::invokeMethod(m_serialManager, "openPort", Qt::QueuedConnection,
        Q_ARG(QString, portName), Q_ARG(qint32, baudRate));
}

/// @brief 请求断开当前串口连接。
void ConfigService::disconnectPort() {
    if (m_serialManager == nullptr) {
        return;
    }
    qCInfo(lcConfig) << "Disconnect request";
    QMetaObject::invokeMethod(m_serialManager, "closePort", Qt::QueuedConnection);
}

// =============================================================================
// I2C 总线扫描
// =============================================================================

/// @brief 发起 I2C 总线扫描命令，返回在线设备地址列表。
///
/// 响应处理：
/// - 成功：解析地址列表，发射 i2cScanResult(addresses)
/// - 错误：发射 protocolError(errorCode)
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

            // --- 错误响应处理 ---
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

            // --- 解析扫描结果：字节数组 → 地址列表 ---
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

// =============================================================================
// 电机 IC 地址设置
// =============================================================================

/// @brief 设置电机驱动 IC 的 I2C 地址。
///
/// @param addr  7 位 I2C 地址
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

// =============================================================================
// PMIC 电压配置（两步序列）
// =============================================================================

/// @brief 配置 PMIC 电压并使能。
///
/// 两步序列：
///   1. CmdSetPmicVoltage — 设置三路电压（DRVVDD / IOVDD / VCMVDD）
///   2. CmdPmicEnable     — 在收到电压设置 ACK 后自动发送使能命令
///
/// 电压单位转换：浮点 V → 整数 (V × 100)，有效范围 [0.60V, 3.77V]。
/// 整体超时 5 秒（由 m_pmicTimeoutTimer 控制）。
///
/// @param drvvddV   DRVVDD 电压（V）
/// @param iovddV    IOVDD 电压（V）
/// @param vcmvddV   VCMVDD 电压（V）
void ConfigService::configurePmic(double drvvddV, double iovddV, double vcmvddV) {
    if (m_dispatcher == nullptr || !m_isConnected) {
        if (m_pmicTimeoutTimer != nullptr) {
            m_pmicTimeoutTimer->stop();
        }
        emit pmicConfigFailed(QStringLiteral("Serial not connected"));
        return;
    }

    // 防止重入：同一时间只允许一个 PMIC 配置序列
    if (m_pmicState != PmicState::Idle) {
        qCWarning(lcConfig) << "PMIC configuration ignored: sequence already in progress";
        return;
    }

    // ---------- 电压单位转换：V → V×100 ----------
    const quint16 drvvdd = static_cast<quint16>(qRound(drvvddV * 100.0));
    const quint16 iovdd = static_cast<quint16>(qRound(iovddV * 100.0));
    const quint16 vcmvdd = static_cast<quint16>(qRound(vcmvddV * 100.0));

    // ---------- 范围校验：[0.60V, 3.77V] ----------
    constexpr quint16 kMinPmicVoltage = 60;             // 0.60 V
    constexpr quint16 kMaxPmicVoltage = 377;            // 3.77 V
    if (drvvdd < kMinPmicVoltage || drvvdd > kMaxPmicVoltage ||
        iovdd < kMinPmicVoltage || iovdd > kMaxPmicVoltage ||
        vcmvdd < kMinPmicVoltage || vcmvdd > kMaxPmicVoltage) {
        if (m_pmicTimeoutTimer != nullptr) {
            m_pmicTimeoutTimer->stop();
        }
        emit pmicConfigFailed(QStringLiteral("Voltage out of range"));
        return;
    }

    // ---------- 第 1 步：发送电压设置命令 ----------
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

            // --- 电压设置错误 ---
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

            // --- 电压设置 ACK → 第 2 步：发送使能命令 ---
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

                    // --- 使能错误 ---
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

                    // --- 使能 ACK → PMIC 配置完成 ---
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

// =============================================================================
// PMIC 关闭
// =============================================================================

/// @brief 发送 PMIC 关闭命令。
void ConfigService::disablePmic() {
    if (m_dispatcher == nullptr || !m_isConnected) {
        emit pmicDisableFailed(QStringLiteral("Serial not connected"));
        return;
    }

    const QByteArray payload = MotorProtocol::encodePmicDisable();
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdPmicDisable)))
               .arg(formatPayloadHex(payload));

    QPointer<ConfigService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdPmicDisable, payload, CommandDispatcher::Normal,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            Q_UNUSED(seq);
            Q_UNUSED(data);
            if (guard == nullptr) {
                return;
            }
            if (cmd == MotorProtocol::CmdErrorResponse) {
                emit guard->pmicDisableFailed(QStringLiteral("Device returned error"));
                return;
            }
            if (cmd != MotorProtocol::CmdPmicDisable) {
                return;
            }
            emit guard->pmicDisableSuccess();
        });
}

// =============================================================================
// 设备复位
// =============================================================================

/// @brief 发送设备复位命令。
void ConfigService::resetDevice() {
    if (m_dispatcher == nullptr || !m_isConnected) {
        emit resetFailed(QStringLiteral("Serial not connected"));
        return;
    }

    const QByteArray payload = MotorProtocol::encodeReset();
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdReset)))
               .arg(formatPayloadHex(payload));

    QPointer<ConfigService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdReset, payload, CommandDispatcher::Normal,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            Q_UNUSED(seq);
            Q_UNUSED(data);
            if (guard == nullptr) {
                return;
            }
            if (cmd == MotorProtocol::CmdErrorResponse) {
                emit guard->resetFailed(QStringLiteral("Device returned error"));
                return;
            }
            if (cmd != MotorProtocol::CmdReset) {
                return;
            }
            emit guard->resetSuccess();
        });
}

// =============================================================================
// 电机测试
// =============================================================================

/// @brief 发送电机测试命令。
void ConfigService::testMotor() {
    if (m_dispatcher == nullptr || !m_isConnected) {
        emit motorTestFailed(QStringLiteral("Serial not connected"));
        return;
    }

    const QByteArray payload = MotorProtocol::encodeMotorTest();
    qCInfo(lcConfig).noquote()
        << QStringLiteral("%1 TX payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdMotorTest)))
               .arg(formatPayloadHex(payload));

    QPointer<ConfigService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdMotorTest, payload, CommandDispatcher::Normal,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            Q_UNUSED(seq);
            Q_UNUSED(data);
            if (guard == nullptr) {
                return;
            }
            if (cmd == MotorProtocol::CmdErrorResponse) {
                emit guard->motorTestFailed(QStringLiteral("Motor test failed"));
                return;
            }
            if (cmd != MotorProtocol::CmdMotorTest) {
                return;
            }
            emit guard->motorTestSuccess();
        });
}

// =============================================================================
// 串口状态回调
// =============================================================================

/// @brief 串口连接成功回调。
void ConfigService::onSerialConnected() {
    qCInfo(lcConfig) << "Serial connected";
    m_isConnected = true;

    // 启动心跳检测（SerialManager 在工作线程，必须通过 QueuedConnection 调用）
    QMetaObject::invokeMethod(m_serialManager, "startHeartbeat", Qt::QueuedConnection);

    emit serialConnected(m_connectedPort, m_connectedBaud);
}

/// @brief 串口断开回调：重置 PMIC 状态机。
void ConfigService::onSerialDisconnected() {
    qCInfo(lcConfig) << "Serial disconnected";
    m_isConnected = false;

    // 停止心跳（防御性调用，closePortInternal 内部也会停止）
    QMetaObject::invokeMethod(m_serialManager, "stopHeartbeat", Qt::QueuedConnection);

    if (m_pmicTimeoutTimer != nullptr) {
        m_pmicTimeoutTimer->stop();
    }
    m_pmicState = PmicState::Idle;
    emit serialDisconnected();
}

// =============================================================================
// 工具方法
// =============================================================================

/// @brief 将协议错误码映射为可读名称。
///
/// @param errorCode  协议错误码
/// @return 错误名称字符串
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
