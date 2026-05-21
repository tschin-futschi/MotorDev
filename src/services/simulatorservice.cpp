// =============================================================================
// @file    simulatorservice.cpp
// @brief   模拟器服务实现 — 命令分发、模拟响应、正弦波形生成、流线程管理
//
// SimulatorService 模拟 STM32 固件行为，在第二个串口上响应上位机的所有
// 协议命令。主要职责：
//
// 1. 命令分发 — 接收 SimulatorSerial 解析出的帧，按 cmd 分发到对应 handler
// 2. 模拟响应 — 每个 handler 构造 ACK/错误帧并通过 SimulatorSerial 发回
// 3. 波形生成 — 在独立 std::thread 中生成正弦波流数据，模拟示波器数据流
// 4. 调试注入 — 生成的流数据通过 debugStreamGenerated 信号注入 ScopeService
//
// 线程模型：
//   - 命令分发和响应：主线程（Qt 事件循环）
//   - 波形生成：独立 std::thread（streamWorkerLoop），通过 mutex + cv 同步
//   - 流数据投递：streamWorkerLoop → queueDebugStreamFrame → flushPendingDebugStream
//     （通过 QMetaObject::invokeMethod 从 std::thread 投递到主线程）
// =============================================================================
#include "services/simulatorservice.h"

#include "frameparser.h"
#include "protocol/motor_protocol.h"
#include "services/simulatorserial.h"

#include <QDebug>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>

#include <cmath>
#include <chrono>
#include <utility>



namespace {
constexpr uint8_t kFixedSampleIntervalIndex = 0x04;    ///< 模拟器固定采样间隔索引
constexpr int kFixedSampleIntervalUs = 1000;            ///< 模拟器固定采样间隔（1 ms）
constexpr int kStreamWakeBlockUs = 10000;               ///< 流线程每次唤醒块时长（10 ms）
constexpr int kMaxWakeBlocksPerCycle = 4;               ///< 每个唤醒周期最大补发块数

/// @brief 格式化单字节为 "0x00" 形式。
QString formatByte(uint8_t value) { return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(); }
/// @brief 格式化双字节为 "0x0000" 形式。
QString formatWord(quint16 value) { return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper(); }
/// @brief 将 16 位值编码为大端字节序。
QByteArray encodeWord(quint16 value) { QByteArray data; data.append(static_cast<char>((value >> 8) & 0xFF)); data.append(static_cast<char>(value & 0xFF)); return data; }
/// @brief 返回模拟器固定采样间隔（忽略 index 参数）。
int sampleIntervalUs(uint8_t index) { Q_UNUSED(index); return kFixedSampleIntervalUs; }
}

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造模拟器服务：创建 SimulatorSerial、初始化通道映射、启动流线程。
SimulatorService::SimulatorService(QObject *parent)
    : QObject(parent) {
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<QVector<int16_t>>("QVector<int16_t>");

    // 创建模拟器专用串口（运行在独立线程）
    m_simulator = new SimulatorSerial();

    // 默认通道寄存器映射：CH1=0x0010，其余无效
    m_channelRegisterMap = QVector<quint16>(8, 0xFFFF);
    m_channelRegisterMap[0] = 0x0010;

    // 连接 SimulatorSerial 信号
    connect(m_simulator, &SimulatorSerial::connected, this, &SimulatorService::onSimulatorConnected);
    connect(m_simulator, &SimulatorSerial::disconnected, this, &SimulatorService::onSimulatorDisconnected);
    connect(m_simulator, &SimulatorSerial::errorOccurred, this, [this](const QString &message) {
        emit sysLogEntry(message, true);
        emit errorOccurred(message);
    });
    connect(m_simulator, &SimulatorSerial::frameReceived, this, &SimulatorService::onFrameReceived);

    // 启动独立的流数据生成线程
    m_streamThread = std::thread(&SimulatorService::streamWorkerLoop, this);
}

/// @brief 安全销毁：停止流线程，等待 join，销毁 SimulatorSerial。
SimulatorService::~SimulatorService() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamStopRequested = true;
        m_sampling = false;
        m_pendingDebugFrames.clear();
        m_debugFlushQueued = false;
    }
    m_streamCv.notify_all();
    if (m_streamThread.joinable()) m_streamThread.join();
    delete m_simulator;
    m_simulator = nullptr;
}

// =============================================================================
// 公共配置接口
// =============================================================================

/// @brief 设置寄存器读取的模拟返回值（同时更新流波形基准值）。
///
/// @param text  十六进制或十进制文本（如 "0x1000"）
void SimulatorService::setRegisterReadValue(const QString &text) {
    QString trimmed = text.trimmed();
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) trimmed = trimmed.mid(2);
    bool ok = false;
    const quint16 raw = trimmed.toUShort(&ok, 16);
    const qint16 value = ok ? static_cast<qint16>(raw) : 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_regReadValue = value;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamBaseValue = value;                      // 同步更新流波形 DC 偏移
    }
}

/// @brief 返回当前模拟的寄存器读取值。
qint16 SimulatorService::registerReadValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_regReadValue;
}

// =============================================================================
// 串口连接管理
// =============================================================================

/// @brief 请求连接模拟器串口（跨线程调用 SimulatorSerial::openPort）。
void SimulatorService::connectToPort(const QString &portName, qint32 baudRate) {
    QMetaObject::invokeMethod(m_simulator, "openPort", Qt::QueuedConnection,
        Q_ARG(QString, portName), Q_ARG(qint32, baudRate));
}

/// @brief 请求断开模拟器串口。
void SimulatorService::disconnectPort() {
    QMetaObject::invokeMethod(m_simulator, "closePort", Qt::QueuedConnection);
}

/// @brief 返回模拟器是否已连接。
bool SimulatorService::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isConnected;
}

// =============================================================================
// 模拟参数设置
// =============================================================================

/// @brief 设置 I2C 扫描返回的模拟地址列表（逗号或空格分隔的十六进制地址）。
void SimulatorService::setScanAddresses(const QString &text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_scanAddressText = text;
}

/// @brief 设置 IC 地址设置命令的模拟结果（成功/失败）。
void SimulatorService::setIcAddrResultSuccess(bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_icAddrSuccess = success;
}

/// @brief 设置寄存器写入命令的模拟结果（成功/失败）。
void SimulatorService::setWriteResultSuccess(bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_writeSuccess = success;
}

// =============================================================================
// 连接状态回调
// =============================================================================

/// @brief 模拟器串口连接成功。
void SimulatorService::onSimulatorConnected() {
    { std::lock_guard<std::mutex> lock(m_mutex); m_isConnected = true; }
    m_streamCv.notify_all();                            // 唤醒流线程
    emit sysLogEntry(tr("模拟器已连接"), false);
    emit connected();
}

/// @brief 模拟器串口断开：停止采样、清空队列。
void SimulatorService::onSimulatorDisconnected() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isConnected = false;
        m_sampling = false;
        m_pendingDebugFrames.clear();
        m_debugFlushQueued = false;
    }
    m_streamCv.notify_all();
    emit debugStreamActiveChanged(false);
    emit sysLogEntry(tr("模拟器已断开"), false);
    emit disconnected();
}

// =============================================================================
// 命令描述（用于日志面板）
// =============================================================================

/// @brief 将接收到的命令生成人类可读的描述文本。
QString SimulatorService::describeIncoming(uint8_t cmd, const QByteArray &data) {
    switch (cmd) {
    case MotorProtocol::CmdHeartbeat: return QStringLiteral("HEARTBEAT");
    case MotorProtocol::CmdI2cBusScan: return QStringLiteral("I2C_SCAN");
    case MotorProtocol::CmdSetMotorIcAddr: return !data.isEmpty() ? QStringLiteral("SET_IC_ADDR addr=%1").arg(formatByte(static_cast<uint8_t>(data.at(0)))) : QStringLiteral("SET_IC_ADDR");
    case MotorProtocol::CmdSetPmicVoltage: {
        if (data.size() >= 6) {
            const quint16 drvvdd = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
            const quint16 iovdd = static_cast<quint16>((static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3)));
            const quint16 vcmvdd = static_cast<quint16>((static_cast<quint8>(data.at(4)) << 8) | static_cast<quint8>(data.at(5)));
            return QStringLiteral("SET_PMIC_VOLTAGE DRVVDD=%1 IOVDD=%2 VCMVDD=%3")
                .arg(drvvdd / 100.0, 0, 'f', 2)
                .arg(iovdd / 100.0, 0, 'f', 2)
                .arg(vcmvdd / 100.0, 0, 'f', 2);
        }
        return QStringLiteral("SET_PMIC_VOLTAGE");
    }
    case MotorProtocol::CmdPmicEnable: return QStringLiteral("PMIC_ENABLE");
    case MotorProtocol::CmdPmicDisable: return QStringLiteral("PMIC_DISABLE");
    case MotorProtocol::CmdReadRegister:
        if (data.size() >= 2) {
            const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
            return QStringLiteral("READ addr=%1").arg(formatWord(addr));
        }
        return QStringLiteral("READ");
    case MotorProtocol::CmdWriteRegister:
        if (data.size() >= 4) {
            const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
            const quint16 val = static_cast<quint16>((static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3)));
            return QStringLiteral("WRITE addr=%1 val=%2").arg(formatWord(addr), formatWord(val));
        }
        return QStringLiteral("WRITE");
    case MotorProtocol::CmdStartSampling: return QStringLiteral("SCOPE_START");
    case MotorProtocol::CmdStopSampling: return QStringLiteral("SCOPE_STOP");
    case MotorProtocol::CmdSetSampleInterval: return !data.isEmpty() ? QStringLiteral("SCOPE_INTERVAL idx=%1").arg(formatByte(static_cast<uint8_t>(data.at(0)))) : QStringLiteral("SCOPE_INTERVAL");
    case MotorProtocol::CmdSetSampleChannels: return !data.isEmpty() ? QStringLiteral("SCOPE_CHANNELS mask=%1").arg(formatByte(static_cast<uint8_t>(data.at(0)))) : QStringLiteral("SCOPE_CHANNELS");
    case MotorProtocol::CmdSetChannelRegisterMap: return QStringLiteral("SCOPE_MAP");
    case MotorProtocol::CmdStartLinearGen: return QStringLiteral("GEN_LINEAR");
    case MotorProtocol::CmdStartCosineGen: return QStringLiteral("GEN_COSINE");
    case MotorProtocol::CmdStartSawtoothGen: return QStringLiteral("GEN_SAWTOOTH");
    case MotorProtocol::CmdStopGenerator: return QStringLiteral("GEN_STOP");
    default: return QStringLiteral("UNKNOWN");
    }
}

// =============================================================================
// 帧接收与命令分发
// =============================================================================

/// @brief SimulatorSerial 解析出完整帧后的回调入口。
void SimulatorService::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    emit logEntry(QStringLiteral("RX"), cmd, seq, data, describeIncoming(cmd, data));
    dispatchWithDelay(cmd, seq, data);
}

/// @brief 命令分发：可配置延迟后调用对应 handler。
///
/// 若 m_responseDelayMs > 0，使用 QTimer::singleShot 模拟网络延迟。
void SimulatorService::dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    // 心跳包立即响应，不走延迟
    if (cmd == MotorProtocol::CmdHeartbeat) { handleHeartbeat(seq); return; }

    const auto dispatch = [this, cmd, seq, data]() {
        switch (cmd) {
        case MotorProtocol::CmdI2cBusScan: handleI2cScan(seq, data); break;
        case MotorProtocol::CmdSetMotorIcAddr: handleSetIcAddr(seq, data); break;
        case MotorProtocol::CmdSetPmicVoltage: handleSetPmicVoltage(seq, data); break;
        case MotorProtocol::CmdReset: handleReset(seq); break;
        case MotorProtocol::CmdMotorTest: handleMotorTest(seq); break;
        case MotorProtocol::CmdPmicEnable: handlePmicEnable(seq); break;
        case MotorProtocol::CmdPmicDisable: handlePmicDisable(seq); break;
        case MotorProtocol::CmdReadRegister: handleReadRegister(seq, data); break;
        case MotorProtocol::CmdWriteRegister: handleWriteRegister(seq, data); break;
        case MotorProtocol::CmdStartSampling: handleStartSampling(seq); break;
        case MotorProtocol::CmdStopSampling: handleStopSampling(seq); break;
        case MotorProtocol::CmdSetSampleInterval: handleSetSampleInterval(seq, data); break;
        case MotorProtocol::CmdSetSampleChannels: handleSetSampleChannels(seq, data); break;
        case MotorProtocol::CmdSetChannelRegisterMap: handleSetChannelRegisterMap(seq, data); break;
        case MotorProtocol::CmdStartLinearGen: handleStartLinearGen(seq, data); break;
        case MotorProtocol::CmdStartCosineGen: handleStartCosineGen(seq, data); break;
        case MotorProtocol::CmdStartSawtoothGen: handleStartSawtoothGen(seq, data); break;
        case MotorProtocol::CmdStopGenerator: handleStopGenerator(seq); break;
        default: handleUnknownCommand(cmd, seq); break;
        }
    };

    if (m_responseDelayMs > 0) { QTimer::singleShot(m_responseDelayMs, this, dispatch); return; }
    dispatch();
}

// =============================================================================
// 命令 Handler — 基础命令
// =============================================================================

/// @brief 心跳回显。
void SimulatorService::handleHeartbeat(uint8_t seq) {
    sendResponseFrame(seq, MotorProtocol::CmdHeartbeat, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdHeartbeat, seq, {}, QStringLiteral("HEARTBEAT echo"));
}

/// @brief I2C 扫描：解析 m_scanAddressText 中的地址列表，构造响应。
void SimulatorService::handleI2cScan(uint8_t seq, const QByteArray &data) {
    Q_UNUSED(data);
    QString scanAddressText;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        scanAddressText = m_scanAddressText;
    }

    // 解析逗号/空格分隔的十六进制地址
    QByteArray response; QStringList accepted;
    const QStringList tokens = scanAddressText.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        QString text = token.trimmed();
        if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) text = text.mid(2);
        bool ok = false; const uint value = text.toUInt(&ok, 16);
        if (!ok || value > 0x7F) continue;              // 忽略无效地址
        response.append(static_cast<char>(value));
        accepted.append(QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
    }
    // 响应格式：[地址数量][地址1][地址2]...
    response.prepend(static_cast<char>(response.size()));
    sendResponseFrame(seq, MotorProtocol::CmdI2cBusScan, response);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdI2cBusScan, seq, response, accepted.isEmpty() ? QStringLiteral("I2C_SCAN → (空)") : QStringLiteral("I2C_SCAN → %1").arg(accepted.join(QStringLiteral(", "))));
}

/// @brief IC 地址设置：根据 m_icAddrSuccess 返回 ACK 或错误。
void SimulatorService::handleSetIcAddr(uint8_t seq, const QByteArray &data) {
    Q_UNUSED(data);
    bool icAddrSuccess = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        icAddrSuccess = m_icAddrSuccess;
    }
    if (icAddrSuccess) {
        sendResponseFrame(seq, MotorProtocol::CmdSetMotorIcAddr, {});
        emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetMotorIcAddr, seq, {}, QStringLiteral("SET_IC_ADDR → 成功"));
        return;
    }
    sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed);
}

/// @brief 设备复位：始终返回 ACK。
void SimulatorService::handleReset(uint8_t seq) {
    sendResponseFrame(seq, MotorProtocol::CmdReset, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdReset, seq, {}, QStringLiteral("RESET → ACK"));
}

/// @brief 电机测试：始终返回 ACK。
void SimulatorService::handleMotorTest(uint8_t seq) {
    sendResponseFrame(seq, MotorProtocol::CmdMotorTest, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdMotorTest, seq, {}, QStringLiteral("MOTOR_TEST → ACK"));
}

// =============================================================================
// 命令 Handler — PMIC 相关
// =============================================================================

/// @brief PMIC 使能：记录状态，返回 ACK。
void SimulatorService::handlePmicEnable(uint8_t seq) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pmicEnabled = true;
    }
    sendResponseFrame(seq, MotorProtocol::CmdPmicEnable, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdPmicEnable, seq, {}, QStringLiteral("PMIC_ENABLE → ACK"));
}

/// @brief PMIC 禁用：记录状态，返回 ACK。
void SimulatorService::handlePmicDisable(uint8_t seq) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pmicEnabled = false;
    }
    sendResponseFrame(seq, MotorProtocol::CmdPmicDisable, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdPmicDisable, seq, {}, QStringLiteral("PMIC_DISABLE → ACK"));
}

/// @brief PMIC 电压设置：校验 6 字节 payload 和电压范围 [0.60V, 3.77V]。
void SimulatorService::handleSetPmicVoltage(uint8_t seq, const QByteArray &data) {
    if (data.size() != 6) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }

    // 解析三路电压（大端序，单位 V×100）
    const quint16 drvvdd = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
    const quint16 iovdd = static_cast<quint16>((static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3)));
    const quint16 vcmvdd = static_cast<quint16>((static_cast<quint8>(data.at(4)) << 8) | static_cast<quint8>(data.at(5)));

    // 范围校验
    if (drvvdd < 60 || drvvdd > 377 || iovdd < 60 || iovdd > 377 || vcmvdd < 60 || vcmvdd > 377) {
        sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pmicDrvvdd = drvvdd;
        m_pmicIovdd = iovdd;
        m_pmicVcmvdd = vcmvdd;
    }
    sendResponseFrame(seq, MotorProtocol::CmdSetPmicVoltage, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetPmicVoltage, seq, {},
                  QStringLiteral("SET_PMIC_VOLTAGE → ACK (DRVVDD=%1V IOVDD=%2V VCMVDD=%3V)")
                      .arg(drvvdd / 100.0, 0, 'f', 2).arg(iovdd / 100.0, 0, 'f', 2).arg(vcmvdd / 100.0, 0, 'f', 2));
}

// =============================================================================
// 命令 Handler — 寄存器读写
// =============================================================================

/// @brief 寄存器读取：返回预设的 m_regReadValue。
void SimulatorService::handleReadRegister(uint8_t seq, const QByteArray &data) {
    if (data.size() < 2) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
    Q_UNUSED(addr);
    const qint16 value = registerReadValue();
    const QByteArray response = encodeWord(static_cast<quint16>(value));
    sendResponseFrame(seq, MotorProtocol::CmdReadRegister, response);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdReadRegister, seq, response,
                  QStringLiteral("READ → %1").arg(formatWord(static_cast<quint16>(value))));
}

/// @brief 寄存器写入：根据 m_writeSuccess 返回 ACK 或错误。
void SimulatorService::handleWriteRegister(uint8_t seq, const QByteArray &data) {
    if (data.size() < 4) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    bool writeSuccess = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        writeSuccess = m_writeSuccess;
    }
    if (!writeSuccess) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    sendResponseFrame(seq, MotorProtocol::CmdWriteRegister, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdWriteRegister, seq, {}, QStringLiteral("WRITE → ACK"));
    Q_UNUSED(data);
}

// =============================================================================
// 命令 Handler — 示波器采样控制
// =============================================================================

/// @brief 启动采样：校验通道映射有效性，启动流线程。
void SimulatorService::handleStartSampling(uint8_t seq) {
    QVector<quint16> channelRegisterMap;
    uint8_t channelMask = 0x00;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        channelRegisterMap = m_channelRegisterMap;
        channelMask = m_channelMask;
    }

    // 校验至少有一个有效的通道映射
    bool hasValidMapping = false;
    for (int index = 0; index < channelRegisterMap.size(); ++index) {
        if ((channelMask & (1u << index)) != 0 && channelRegisterMap.at(index) != 0xFFFF) {
            hasValidMapping = true;
            break;
        }
    }
    if (!hasValidMapping) {
        sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed);
        sendDebugInfo(QStringLiteral("Start sampling failed: no valid channel mapping"));
        return;
    }

    // 初始化采样状态并唤醒流线程
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sampling = true;
        m_streamTick = 0;
        m_pendingDebugFrames.clear();
        m_debugFlushQueued = false;
        m_lastStreamActualUs = -1;
        m_streamActualUsAccumulator = 0;
        m_streamActualUsSamples = 0;
    }
    m_streamElapsedTimer.restart();
    m_streamCv.notify_all();

    sendResponseFrame(seq, MotorProtocol::CmdStartSampling, {});
    emit debugStreamActiveChanged(true);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStartSampling, seq, {}, QStringLiteral("SCOPE_START → ACK"));
}

/// @brief 停止采样：停止流线程，清空队列。
void SimulatorService::handleStopSampling(uint8_t seq) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sampling = false;
        m_pendingDebugFrames.clear();
        m_debugFlushQueued = false;
        m_lastStreamActualUs = -1;
        m_streamActualUsAccumulator = 0;
        m_streamActualUsSamples = 0;
    }
    m_streamCv.notify_all();
    sendResponseFrame(seq, MotorProtocol::CmdStopSampling, {});
    emit debugStreamActiveChanged(false);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStopSampling, seq, {}, QStringLiteral("SCOPE_STOP → ACK"));
}

/// @brief 设置采样间隔：模拟器始终使用固定间隔（忽略客户端指定值）。
void SimulatorService::handleSetSampleInterval(uint8_t seq, const QByteArray &data) {
    if (data.size() != 1) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    const uint8_t index = static_cast<uint8_t>(data.at(0));
    if (!MotorProtocol::isValidSampleIntervalIndex(index)) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sampleIntervalIndex = kFixedSampleIntervalIndex;  // 强制使用固定值
    }
    m_streamCv.notify_all();
    sendResponseFrame(seq, MotorProtocol::CmdSetSampleInterval, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetSampleInterval, seq, {},
                  QStringLiteral("SCOPE_INTERVAL → %1").arg(formatByte(kFixedSampleIntervalIndex)));
}

/// @brief 设置采样通道掩码。
void SimulatorService::handleSetSampleChannels(uint8_t seq, const QByteArray &data) {
    if (data.size() != 1) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    const uint8_t mask = static_cast<uint8_t>(data.at(0));
    if (!MotorProtocol::isValidSampleChannelMask(mask)) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_channelMask = mask;
    }
    sendResponseFrame(seq, MotorProtocol::CmdSetSampleChannels, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetSampleChannels, seq, {},
                  QStringLiteral("SCOPE_CHANNELS → %1").arg(formatByte(mask)));
}

/// @brief 设置通道→寄存器映射表（16 字节 = 8 × uint16 大端序）。
void SimulatorService::handleSetChannelRegisterMap(uint8_t seq, const QByteArray &data) {
    if (data.size() != 16) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int index = 0; index < 8; ++index) {
            const int offset = index * 2;
            m_channelRegisterMap[index] = static_cast<quint16>(
                (static_cast<quint8>(data.at(offset)) << 8) | static_cast<quint8>(data.at(offset + 1)));
        }
    }
    sendResponseFrame(seq, MotorProtocol::CmdSetChannelRegisterMap, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetChannelRegisterMap, seq, {}, QStringLiteral("SCOPE_MAP → ACK"));
}

// =============================================================================
// 命令 Handler — 波形发生器
// =============================================================================

/// @brief 线性扫描启动：校验 10 字节 payload，返回 ACK。
void SimulatorService::handleStartLinearGen(uint8_t seq, const QByteArray &data) {
    if (data.size() != 10) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    sendResponseFrame(seq, MotorProtocol::CmdStartLinearGen, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStartLinearGen, seq, {}, QStringLiteral("GEN_LINEAR → ACK"));
}

/// @brief 余弦波启动：校验最小 11 字节 payload，返回 ACK。
void SimulatorService::handleStartCosineGen(uint8_t seq, const QByteArray &data) {
    if (data.size() < 11) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    sendResponseFrame(seq, MotorProtocol::CmdStartCosineGen, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStartCosineGen, seq, {}, QStringLiteral("GEN_COSINE → ACK"));
}

/// @brief 锯齿波测试启动：校验 8 字节 payload，返回 ACK。
void SimulatorService::handleStartSawtoothGen(uint8_t seq, const QByteArray &data) {
    if (data.size() != 8) { sendErrorFrame(seq, MotorProtocol::ErrorCode::ExecutionFailed); return; }
    sendResponseFrame(seq, MotorProtocol::CmdStartSawtoothGen, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStartSawtoothGen, seq, {}, QStringLiteral("GEN_SAWTOOTH → ACK"));
}

/// @brief 停止波形发生器。
void SimulatorService::handleStopGenerator(uint8_t seq) {
    sendResponseFrame(seq, MotorProtocol::CmdStopGenerator, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStopGenerator, seq, {}, QStringLiteral("GEN_STOP → ACK"));
}

/// @brief 未知命令：返回错误码 0x02。
void SimulatorService::handleUnknownCommand(uint8_t cmd, uint8_t seq) {
    sendErrorFrame(seq, MotorProtocol::ErrorCode::UnknownCmd);
    emit logEntry(QStringLiteral("TX"), cmd, seq, {}, tr("未知命令"));
}

// =============================================================================
// 帧发送
// =============================================================================

/// @brief 编码 Control 帧并通过 SimulatorSerial 发送（跨线程队列连接）。
void SimulatorService::sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data) {
    const QByteArray frame = FrameParser::encodeControlFrame(seq, cmd, data);
    QMetaObject::invokeMethod(m_simulator, "sendRawFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame));
}

/// @brief 发送错误响应帧。
void SimulatorService::sendErrorFrame(uint8_t seq, uint8_t errorCode) {
    const QByteArray data(1, static_cast<char>(errorCode));
    sendResponseFrame(seq, MotorProtocol::CmdErrorResponse, data);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdErrorResponse, seq, data,
                  QStringLiteral("ERROR %1").arg(formatByte(errorCode)));
}

/// @brief 发送调试信息帧（cmd=0x06，seq=0xFF）。
void SimulatorService::sendDebugInfo(const QString &message) {
    const QByteArray data = message.toUtf8().left(255);
    sendResponseFrame(0xFF, MotorProtocol::CmdDebugInfo, data);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdDebugInfo, 0xFF, data, QStringLiteral("DEBUG_INFO"));
}

// =============================================================================
// 流数据生成
// =============================================================================

/// @brief 生成一帧流数据（由 streamWorkerLoop 在 std::thread 中调用）。
///
/// 波形公式：
///   value = baseValue + sin(2π × 1Hz × t + channelPhase) × 2000
///                     + sin(2π × 100Hz × t + channelPhase) × 100
///
/// 其中 t = streamTick × 0.001s，channelPhase = index × 0.35 rad。
void SimulatorService::emitOneStreamFrame() {
    QVector<quint16> channelRegisterMap;
    uint8_t channelMask = 0x00;
    quint32 streamTick = 0;
    qint16 baseValue = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isConnected || !m_sampling || m_simulator == nullptr) return;
        channelRegisterMap = m_channelRegisterMap;
        channelMask = m_channelMask;
        streamTick = m_streamTick;
        baseValue = m_streamBaseValue;
    }

    // 逐通道计算波形值
    QVector<qint16> samples;
    samples.reserve(8);
    for (int index = 0; index < channelRegisterMap.size(); ++index) {
        if ((channelMask & (1u << index)) == 0) continue;
        const quint16 reg = channelRegisterMap.at(index);
        qint16 value = 0;
        if (reg != 0xFFFF) {
            // 正弦波参数
            constexpr double kSampleIntervalSeconds = 0.001;
            constexpr double kFundamentalHz = 1.0;
            constexpr double kFundamentalAmplitude = 2000.0;
            constexpr double kRippleHz = 100.0;
            constexpr double kRippleAmplitude = 100.0;
            constexpr double kChannelPhaseStep = 0.35;  // 通道间相位偏移（rad）

            const double timeSeconds = static_cast<double>(streamTick) * kSampleIntervalSeconds;
            const double base = static_cast<double>(baseValue);
            const double channelPhase = static_cast<double>(index) * kChannelPhaseStep;
            // 基波（1 Hz）+ 纹波（100 Hz）
            const double fundamental = std::sin((2.0 * M_PI * kFundamentalHz * timeSeconds) + channelPhase) * kFundamentalAmplitude;
            const double ripple = std::sin((2.0 * M_PI * kRippleHz * timeSeconds) + channelPhase) * kRippleAmplitude;
            value = static_cast<qint16>(std::lround(base + fundamental + ripple));
        }
        samples.append(value);
    }

    // 入队待投递到主线程
    queueDebugStreamFrame(channelMask, samples);

    // 统计实际采样间隔（用于性能分析）
    const qint64 nowUs = m_streamElapsedTimer.isValid() ? m_streamElapsedTimer.nsecsElapsed() / 1000 : -1;
    const qint64 actualDeltaUs = (m_lastStreamActualUs >= 0 && nowUs >= 0) ? (nowUs - m_lastStreamActualUs) : -1;
    if (actualDeltaUs >= 0) { m_streamActualUsAccumulator += actualDeltaUs; ++m_streamActualUsSamples; }
    m_lastStreamActualUs = nowUs;
    { std::lock_guard<std::mutex> lock(m_mutex); ++m_streamTick; }
}

/// @brief 将一帧调试流数据加入待投递队列，并调度主线程 flush。
///
/// 使用 m_debugFlushQueued 标志避免重复调度 flushPendingDebugStream。
void SimulatorService::queueDebugStreamFrame(uint8_t channelMask, const QVector<int16_t> &samples) {
    bool shouldScheduleFlush = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_sampling || m_streamStopRequested) return;
        PendingDebugFrame frame;
        frame.channelMask = channelMask;
        frame.samples = samples;
        m_pendingDebugFrames.append(std::move(frame));
        if (!m_debugFlushQueued) {
            m_debugFlushQueued = true;
            shouldScheduleFlush = true;
        }
    }
    if (!shouldScheduleFlush) return;
    // 跨线程调度：std::thread → 主线程事件循环
    QMetaObject::invokeMethod(this, &SimulatorService::flushPendingDebugStream, Qt::QueuedConnection);
}

/// @brief 在主线程中批量发射 debugStreamGenerated 信号。
///
/// 取走所有待投递帧后逐帧发射信号。若发射期间又有新帧入队，
/// 则再次调度自身，确保不丢帧。
void SimulatorService::flushPendingDebugStream() {
    QVector<PendingDebugFrame> frames;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        frames.swap(m_pendingDebugFrames);
        m_debugFlushQueued = false;
    }
    // 逐帧发射信号
    for (const PendingDebugFrame &frame : frames)
        emit debugStreamGenerated(frame.channelMask, frame.samples);

    // 检查是否有新帧需要再次 flush
    bool shouldScheduleAgain = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_pendingDebugFrames.isEmpty() && !m_debugFlushQueued) {
            m_debugFlushQueued = true;
            shouldScheduleAgain = true;
        }
    }
    if (shouldScheduleAgain)
        QMetaObject::invokeMethod(this, &SimulatorService::flushPendingDebugStream, Qt::QueuedConnection);
}

// =============================================================================
// 流数据工作线程
// =============================================================================

/// @brief std::thread 入口：按采样间隔持续生成流数据。
///
/// 线程模型：
///   1. 等待条件变量：m_sampling && m_isConnected
///   2. 每 10 ms（kStreamWakeBlockUs）唤醒一次
///   3. 根据采样间隔计算本次应生成的帧数
///   4. 若因延迟导致滞后，最多补发 kMaxWakeBlocksPerCycle 个块
///   5. m_streamStopRequested 为 true 时退出
void SimulatorService::streamWorkerLoop() {
    using clock = std::chrono::steady_clock;
    const auto wakeBlock = std::chrono::microseconds(kStreamWakeBlockUs);
    auto nextWake = clock::now();
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_streamStopRequested) {
        // 等待采样启动
        m_streamCv.wait(lock, [this]() { return m_streamStopRequested || (m_sampling && m_isConnected); });
        if (m_streamStopRequested) break;

        nextWake = clock::now();
        // 持续生成流数据直到停止
        while (!m_streamStopRequested && m_sampling && m_isConnected) {
            const int intervalUs = sampleIntervalUs(m_sampleIntervalIndex);
            // 每个 10ms 唤醒块应生成的帧数
            const int framesPerBlock = qMax(1, kStreamWakeBlockUs / qMax(1, intervalUs));
            nextWake += wakeBlock;
            lock.unlock();
            std::this_thread::sleep_until(nextWake);

            // 计算需要补发的块数（处理延迟抖动）
            const auto wakeTime = clock::now();
            int blocksToEmit = 1;
            if (wakeTime > nextWake)
                blocksToEmit += static_cast<int>((wakeTime - nextWake) / wakeBlock);
            blocksToEmit = qBound(1, blocksToEmit, kMaxWakeBlocksPerCycle);

            // 批量生成帧
            const int framesToEmit = framesPerBlock * blocksToEmit;
            for (int frameIndex = 0; frameIndex < framesToEmit; ++frameIndex)
                emitOneStreamFrame();

            nextWake += wakeBlock * (blocksToEmit - 1);
            lock.lock();
        }
    }
}
