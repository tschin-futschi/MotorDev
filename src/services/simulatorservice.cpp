// SimulatorService — 模拟器协议调度、波形生成、流线程管理
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
constexpr uint8_t kFixedSampleIntervalIndex = 0x05;
constexpr int kFixedSampleIntervalUs = 1000;
constexpr int kStreamWakeBlockUs = 10000;
constexpr int kMaxWakeBlocksPerCycle = 4;

QString formatByte(uint8_t value) { return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(); }
QString formatWord(quint16 value) { return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper(); }
QByteArray encodeWord(quint16 value) { QByteArray data; data.append(static_cast<char>((value >> 8) & 0xFF)); data.append(static_cast<char>(value & 0xFF)); return data; }
int sampleIntervalUs(uint8_t index) { Q_UNUSED(index); return kFixedSampleIntervalUs; }
}

SimulatorService::SimulatorService(QObject *parent)
    : QObject(parent) {
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<QVector<int16_t>>("QVector<int16_t>");

    m_simulator = new SimulatorSerial();
    m_channelRegisterMap = QVector<quint16>(8, 0xFFFF);
    m_channelRegisterMap[0] = 0x0010;

    connect(m_simulator, &SimulatorSerial::connected, this, &SimulatorService::onSimulatorConnected);
    connect(m_simulator, &SimulatorSerial::disconnected, this, &SimulatorService::onSimulatorDisconnected);
    connect(m_simulator, &SimulatorSerial::errorOccurred, this, [this](const QString &message) {
        emit sysLogEntry(message, true);
        emit errorOccurred(message);
    });
    connect(m_simulator, &SimulatorSerial::frameReceived, this, &SimulatorService::onFrameReceived);

    m_streamThread = std::thread(&SimulatorService::streamWorkerLoop, this);
}

SimulatorService::~SimulatorService() {
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
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

void SimulatorService::setRegisterReadValue(const QString &text) {
    QString trimmed = text.trimmed();
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) trimmed = trimmed.mid(2);
    bool ok = false;
    const quint16 raw = trimmed.toUShort(&ok, 16);
    const qint16 value = ok ? static_cast<qint16>(raw) : 0;
    {
        std::lock_guard<std::mutex> lock(m_regReadMutex);
        m_regReadValue = value;
    }
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        m_streamBaseValue = value;
    }
}

qint16 SimulatorService::registerReadValue() const {
    std::lock_guard<std::mutex> lock(m_regReadMutex);
    return m_regReadValue;
}

void SimulatorService::connectToPort(const QString &portName, qint32 baudRate) {
    QMetaObject::invokeMethod(m_simulator, "openPort", Qt::QueuedConnection,
        Q_ARG(QString, portName), Q_ARG(qint32, baudRate));
}

void SimulatorService::disconnectPort() {
    QMetaObject::invokeMethod(m_simulator, "closePort", Qt::QueuedConnection);
}

bool SimulatorService::isConnected() const {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    return m_isConnected;
}

void SimulatorService::setScanAddresses(const QString &text) {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    m_scanAddressText = text;
}

void SimulatorService::setIcAddrResultSuccess(bool success) {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    m_icAddrSuccess = success;
}

void SimulatorService::setWriteResultSuccess(bool success) {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    m_writeSuccess = success;
}

void SimulatorService::onSimulatorConnected() {
    { std::lock_guard<std::mutex> lock(m_streamMutex); m_isConnected = true; }
    m_streamCv.notify_all();
    emit sysLogEntry(tr("模拟器已连接"), false);
    emit connected();
}

void SimulatorService::onSimulatorDisconnected() {
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
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

QString SimulatorService::describeIncoming(uint8_t cmd, const QByteArray &data) {
    switch (cmd) {
    case 0x00: return QStringLiteral("HEARTBEAT");
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
    case MotorProtocol::CmdReadRegister: if (data.size() >= 2) { const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1))); return QStringLiteral("READ addr=%1").arg(formatWord(addr)); } return QStringLiteral("READ");
    case MotorProtocol::CmdWriteRegister: if (data.size() >= 4) { const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1))); const quint16 val = static_cast<quint16>((static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3))); return QStringLiteral("WRITE addr=%1 val=%2").arg(formatWord(addr), formatWord(val)); } return QStringLiteral("WRITE");
    case MotorProtocol::CmdStartSampling: return QStringLiteral("SCOPE_START");
    case MotorProtocol::CmdStopSampling: return QStringLiteral("SCOPE_STOP");
    case MotorProtocol::CmdSetSampleInterval: return !data.isEmpty() ? QStringLiteral("SCOPE_INTERVAL idx=%1").arg(formatByte(static_cast<uint8_t>(data.at(0)))) : QStringLiteral("SCOPE_INTERVAL");
    case MotorProtocol::CmdSetSampleChannels: return !data.isEmpty() ? QStringLiteral("SCOPE_CHANNELS mask=%1").arg(formatByte(static_cast<uint8_t>(data.at(0)))) : QStringLiteral("SCOPE_CHANNELS");
    case MotorProtocol::CmdSetChannelRegisterMap: return QStringLiteral("SCOPE_MAP");
    default: return QStringLiteral("UNKNOWN");
    }
}

void SimulatorService::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    emit logEntry(QStringLiteral("RX"), cmd, seq, data, describeIncoming(cmd, data));
    dispatchWithDelay(cmd, seq, data);
}

void SimulatorService::dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (cmd == 0x00) { handleHeartbeat(seq); return; }
    const auto dispatch = [this, cmd, seq, data]() {
        switch (cmd) {
        case MotorProtocol::CmdI2cBusScan: handleI2cScan(seq, data); break;
        case MotorProtocol::CmdSetMotorIcAddr: handleSetIcAddr(seq, data); break;
        case MotorProtocol::CmdSetPmicVoltage: handleSetPmicVoltage(seq, data); break;
        case MotorProtocol::CmdPmicEnable: handlePmicEnable(seq); break;
        case MotorProtocol::CmdPmicDisable: handlePmicDisable(seq); break;
        case MotorProtocol::CmdReadRegister: handleReadRegister(seq, data); break;
        case MotorProtocol::CmdWriteRegister: handleWriteRegister(seq, data); break;
        case MotorProtocol::CmdStartSampling: handleStartSampling(seq); break;
        case MotorProtocol::CmdStopSampling: handleStopSampling(seq); break;
        case MotorProtocol::CmdSetSampleInterval: handleSetSampleInterval(seq, data); break;
        case MotorProtocol::CmdSetSampleChannels: handleSetSampleChannels(seq, data); break;
        case MotorProtocol::CmdSetChannelRegisterMap: handleSetChannelRegisterMap(seq, data); break;
        default: handleUnknownCommand(cmd, seq); break;
        }
    };
    if (m_responseDelayMs > 0) { QTimer::singleShot(m_responseDelayMs, this, dispatch); return; }
    dispatch();
}

void SimulatorService::handleHeartbeat(uint8_t seq) { sendResponseFrame(seq, 0x00, {}); emit logEntry(QStringLiteral("TX"), 0x00, seq, {}, QStringLiteral("HEARTBEAT echo")); }
void SimulatorService::handleI2cScan(uint8_t seq, const QByteArray &data) {
    Q_UNUSED(data);
    QString scanAddressText;
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        scanAddressText = m_scanAddressText;
    }
    QByteArray response; QStringList accepted;
    const QStringList tokens = scanAddressText.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        QString text = token.trimmed();
        if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) text = text.mid(2);
        bool ok = false; const uint value = text.toUInt(&ok, 16);
        if (!ok || value > 0x7F) continue;
        response.append(static_cast<char>(value));
        accepted.append(QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
    }
    response.prepend(static_cast<char>(response.size()));
    sendResponseFrame(seq, MotorProtocol::CmdI2cBusScan, response);
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdI2cBusScan, seq, response, accepted.isEmpty() ? QStringLiteral("I2C_SCAN → (空)") : QStringLiteral("I2C_SCAN → %1").arg(accepted.join(QStringLiteral(", "))));
}
void SimulatorService::handleSetIcAddr(uint8_t seq, const QByteArray &data) {
    Q_UNUSED(data);
    bool icAddrSuccess = false;
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        icAddrSuccess = m_icAddrSuccess;
    }
    if (icAddrSuccess) {
        sendResponseFrame(seq, MotorProtocol::CmdSetMotorIcAddr, {});
        emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetMotorIcAddr, seq, {}, QStringLiteral("SET_IC_ADDR → 成功"));
        return;
    }
    sendErrorFrame(seq, 0x03);
}
void SimulatorService::handlePmicEnable(uint8_t seq) {
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        m_pmicEnabled = true;
    }
    sendResponseFrame(seq, MotorProtocol::CmdPmicEnable, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdPmicEnable, seq, {}, QStringLiteral("PMIC_ENABLE → ACK"));
}
void SimulatorService::handlePmicDisable(uint8_t seq) {
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        m_pmicEnabled = false;
    }
    sendResponseFrame(seq, MotorProtocol::CmdPmicDisable, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdPmicDisable, seq, {}, QStringLiteral("PMIC_DISABLE → ACK"));
}
void SimulatorService::handleSetPmicVoltage(uint8_t seq, const QByteArray &data) {
    if (data.size() != 6) { sendErrorFrame(seq, 0x03); return; }
    const quint16 drvvdd = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
    const quint16 iovdd = static_cast<quint16>((static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3)));
    const quint16 vcmvdd = static_cast<quint16>((static_cast<quint8>(data.at(4)) << 8) | static_cast<quint8>(data.at(5)));
    if (drvvdd < 60 || drvvdd > 377 || iovdd < 60 || iovdd > 377 || vcmvdd < 60 || vcmvdd > 377) { sendErrorFrame(seq, 0x03); return; }
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        m_pmicDrvvdd = drvvdd;
        m_pmicIovdd = iovdd;
        m_pmicVcmvdd = vcmvdd;
    }
    sendResponseFrame(seq, MotorProtocol::CmdSetPmicVoltage, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetPmicVoltage, seq, {}, QStringLiteral("SET_PMIC_VOLTAGE → ACK (DRVVDD=%1V IOVDD=%2V VCMVDD=%3V)").arg(drvvdd / 100.0, 0, 'f', 2).arg(iovdd / 100.0, 0, 'f', 2).arg(vcmvdd / 100.0, 0, 'f', 2));
}
void SimulatorService::handleReadRegister(uint8_t seq, const QByteArray &data) { if (data.size() < 2) { sendErrorFrame(seq, 0x03); return; } const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1))); Q_UNUSED(addr); const qint16 value = registerReadValue(); const QByteArray response = encodeWord(static_cast<quint16>(value)); sendResponseFrame(seq, MotorProtocol::CmdReadRegister, response); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdReadRegister, seq, response, QStringLiteral("READ → %1").arg(formatWord(static_cast<quint16>(value)))); }
void SimulatorService::handleWriteRegister(uint8_t seq, const QByteArray &data) {
    if (data.size() < 4) { sendErrorFrame(seq, 0x03); return; }
    bool writeSuccess = false;
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        writeSuccess = m_writeSuccess;
    }
    if (!writeSuccess) { sendErrorFrame(seq, 0x03); return; }
    sendResponseFrame(seq, MotorProtocol::CmdWriteRegister, {});
    emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdWriteRegister, seq, {}, QStringLiteral("WRITE → ACK"));
    Q_UNUSED(data);
}
void SimulatorService::handleStartSampling(uint8_t seq) {
    QVector<quint16> channelRegisterMap;
    uint8_t channelMask = 0x00;
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        channelRegisterMap = m_channelRegisterMap;
        channelMask = m_channelMask;
    }
    bool hasValidMapping = false;
    for (int index = 0; index < channelRegisterMap.size(); ++index) {
        if ((channelMask & (1u << index)) != 0 && channelRegisterMap.at(index) != 0xFFFF) {
            hasValidMapping = true;
            break;
        }
    }
    if (!hasValidMapping) { sendErrorFrame(seq, 0x03); sendDebugInfo(QStringLiteral("Start sampling failed: no valid channel mapping")); return; }
    { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampling = true; m_streamTick = 0; m_pendingDebugFrames.clear(); m_debugFlushQueued = false; m_lastStreamActualUs = -1; m_streamActualUsAccumulator = 0; m_streamActualUsSamples = 0; }
    m_streamElapsedTimer.restart(); m_streamCv.notify_all(); sendResponseFrame(seq, MotorProtocol::CmdStartSampling, {}); emit debugStreamActiveChanged(true); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStartSampling, seq, {}, QStringLiteral("SCOPE_START → ACK"));
}
void SimulatorService::handleStopSampling(uint8_t seq) { { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampling = false; m_pendingDebugFrames.clear(); m_debugFlushQueued = false; m_lastStreamActualUs = -1; m_streamActualUsAccumulator = 0; m_streamActualUsSamples = 0; } m_streamCv.notify_all(); sendResponseFrame(seq, MotorProtocol::CmdStopSampling, {}); emit debugStreamActiveChanged(false); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdStopSampling, seq, {}, QStringLiteral("SCOPE_STOP → ACK")); }
void SimulatorService::handleSetSampleInterval(uint8_t seq, const QByteArray &data) { if (data.size() != 1) { sendErrorFrame(seq, 0x03); return; } const uint8_t index = static_cast<uint8_t>(data.at(0)); if (!MotorProtocol::isValidSampleIntervalIndex(index)) { sendErrorFrame(seq, 0x03); return; } { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampleIntervalIndex = kFixedSampleIntervalIndex; } m_streamCv.notify_all(); sendResponseFrame(seq, MotorProtocol::CmdSetSampleInterval, {}); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetSampleInterval, seq, {}, QStringLiteral("SCOPE_INTERVAL → %1").arg(formatByte(kFixedSampleIntervalIndex))); }
void SimulatorService::handleSetSampleChannels(uint8_t seq, const QByteArray &data) { if (data.size() != 1) { sendErrorFrame(seq, 0x03); return; } const uint8_t mask = static_cast<uint8_t>(data.at(0)); if (!MotorProtocol::isValidSampleChannelMask(mask)) { sendErrorFrame(seq, 0x03); return; } { std::lock_guard<std::mutex> lock(m_streamMutex); m_channelMask = mask; } sendResponseFrame(seq, MotorProtocol::CmdSetSampleChannels, {}); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetSampleChannels, seq, {}, QStringLiteral("SCOPE_CHANNELS → %1").arg(formatByte(mask))); }
void SimulatorService::handleSetChannelRegisterMap(uint8_t seq, const QByteArray &data) { if (data.size() != 16) { sendErrorFrame(seq, 0x03); return; } { std::lock_guard<std::mutex> lock(m_streamMutex); for (int index = 0; index < 8; ++index) { const int offset = index * 2; m_channelRegisterMap[index] = static_cast<quint16>((static_cast<quint8>(data.at(offset)) << 8) | static_cast<quint8>(data.at(offset + 1))); } } sendResponseFrame(seq, MotorProtocol::CmdSetChannelRegisterMap, {}); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdSetChannelRegisterMap, seq, {}, QStringLiteral("SCOPE_MAP → ACK")); }
void SimulatorService::handleUnknownCommand(uint8_t cmd, uint8_t seq) { sendErrorFrame(seq, 0x02); emit logEntry(QStringLiteral("TX"), cmd, seq, {}, tr("未知命令")); }

void SimulatorService::sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data) { const QByteArray frame = FrameParser::encodeControlFrame(seq, cmd, data); QMetaObject::invokeMethod(m_simulator, "sendRawFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame)); }
void SimulatorService::sendErrorFrame(uint8_t seq, uint8_t errorCode) { const QByteArray data(1, static_cast<char>(errorCode)); sendResponseFrame(seq, MotorProtocol::CmdErrorResponse, data); emit logEntry(QStringLiteral("TX"), MotorProtocol::CmdErrorResponse, seq, data, QStringLiteral("ERROR %1").arg(formatByte(errorCode))); }
void SimulatorService::sendDebugInfo(const QString &message) { const QByteArray data = message.toUtf8().left(255); sendResponseFrame(0xFF, 0x06, data); emit logEntry(QStringLiteral("TX"), 0x06, 0xFF, data, QStringLiteral("DEBUG_INFO")); }

void SimulatorService::emitOneStreamFrame() {
    QVector<quint16> channelRegisterMap; uint8_t channelMask = 0x00; quint32 streamTick = 0; qint16 baseValue = 0;
    { std::lock_guard<std::mutex> lock(m_streamMutex); if (!m_isConnected || !m_sampling || m_simulator == nullptr) return; channelRegisterMap = m_channelRegisterMap; channelMask = m_channelMask; streamTick = m_streamTick; baseValue = m_streamBaseValue; }
    QVector<qint16> samples; samples.reserve(8);
    for (int index = 0; index < channelRegisterMap.size(); ++index) {
        if ((channelMask & (1u << index)) == 0) continue;
        const quint16 reg = channelRegisterMap.at(index);
        qint16 value = 0;
        if (reg != 0xFFFF) {
            constexpr double kSampleIntervalSeconds = 0.001; constexpr double kFundamentalHz = 1.0; constexpr double kFundamentalAmplitude = 2000.0; constexpr double kRippleHz = 100.0; constexpr double kRippleAmplitude = 100.0; constexpr double kChannelPhaseStep = 0.35;
            const double timeSeconds = static_cast<double>(streamTick) * kSampleIntervalSeconds;
            const double base = static_cast<double>(baseValue);
            const double channelPhase = static_cast<double>(index) * kChannelPhaseStep;
            const double fundamental = std::sin((2.0 * M_PI * kFundamentalHz * timeSeconds) + channelPhase) * kFundamentalAmplitude;
            const double ripple = std::sin((2.0 * M_PI * kRippleHz * timeSeconds) + channelPhase) * kRippleAmplitude;
            value = static_cast<qint16>(std::lround(base + fundamental + ripple));
        }
        samples.append(value);
    }
    queueDebugStreamFrame(channelMask, samples);
    const qint64 nowUs = m_streamElapsedTimer.isValid() ? m_streamElapsedTimer.nsecsElapsed() / 1000 : -1;
    const qint64 actualDeltaUs = (m_lastStreamActualUs >= 0 && nowUs >= 0) ? (nowUs - m_lastStreamActualUs) : -1;
    if (actualDeltaUs >= 0) { m_streamActualUsAccumulator += actualDeltaUs; ++m_streamActualUsSamples; }
    m_lastStreamActualUs = nowUs;
    { std::lock_guard<std::mutex> lock(m_streamMutex); ++m_streamTick; }
}

void SimulatorService::queueDebugStreamFrame(uint8_t channelMask, const QVector<int16_t> &samples) {
    bool shouldScheduleFlush = false;
    { std::lock_guard<std::mutex> lock(m_streamMutex); if (!m_sampling || m_streamStopRequested) return; PendingDebugFrame frame; frame.channelMask = channelMask; frame.samples = samples; m_pendingDebugFrames.append(std::move(frame)); if (!m_debugFlushQueued) { m_debugFlushQueued = true; shouldScheduleFlush = true; } }
    if (!shouldScheduleFlush) return;
    QMetaObject::invokeMethod(this, &SimulatorService::flushPendingDebugStream, Qt::QueuedConnection);
}

void SimulatorService::flushPendingDebugStream() {
    QVector<PendingDebugFrame> frames;
    { std::lock_guard<std::mutex> lock(m_streamMutex); frames.swap(m_pendingDebugFrames); m_debugFlushQueued = false; }
    for (const PendingDebugFrame &frame : frames) emit debugStreamGenerated(frame.channelMask, frame.samples);
    bool shouldScheduleAgain = false;
    { std::lock_guard<std::mutex> lock(m_streamMutex); if (!m_pendingDebugFrames.isEmpty() && !m_debugFlushQueued) { m_debugFlushQueued = true; shouldScheduleAgain = true; } }
    if (shouldScheduleAgain) QMetaObject::invokeMethod(this, &SimulatorService::flushPendingDebugStream, Qt::QueuedConnection);
}

void SimulatorService::streamWorkerLoop() {
    using clock = std::chrono::steady_clock;
    const auto wakeBlock = std::chrono::microseconds(kStreamWakeBlockUs);
    auto nextWake = clock::now();
    std::unique_lock<std::mutex> lock(m_streamMutex);
    while (!m_streamStopRequested) {
        m_streamCv.wait(lock, [this]() { return m_streamStopRequested || (m_sampling && m_isConnected); });
        if (m_streamStopRequested) break;
        nextWake = clock::now();
        while (!m_streamStopRequested && m_sampling && m_isConnected) {
            const int intervalUs = sampleIntervalUs(m_sampleIntervalIndex);
            const int framesPerBlock = qMax(1, kStreamWakeBlockUs / qMax(1, intervalUs));
            nextWake += wakeBlock;
            lock.unlock();
            std::this_thread::sleep_until(nextWake);
            const auto wakeTime = clock::now();
            int blocksToEmit = 1;
            if (wakeTime > nextWake) blocksToEmit += static_cast<int>((wakeTime - nextWake) / wakeBlock);
            blocksToEmit = qBound(1, blocksToEmit, kMaxWakeBlocksPerCycle);
            const int framesToEmit = framesPerBlock * blocksToEmit;
            for (int frameIndex = 0; frameIndex < framesToEmit; ++frameIndex) emitOneStreamFrame();
            nextWake += wakeBlock * (blocksToEmit - 1);
            lock.lock();
        }
    }
}
