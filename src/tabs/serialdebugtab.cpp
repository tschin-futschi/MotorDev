#include "tabs/serialdebugtab.h"

#include "frameparser.h"
#include "protocol/motor_protocol.h"
#include "services/simulatorserial.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"
#include "ui_serialdebugtab.h"

#include <QByteArray>
#include <QComboBox>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextCursor>
#include <QTextEdit>
#include <QTime>
#include <QTimer>

#include <cmath>
#include <chrono>
#include <utility>

using namespace MotorDev;

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

SerialDebugTab::SerialDebugTab(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowCloseButtonHint)
    , ui(std::make_unique<Ui::SerialDebugTab>()) {
    setWindowTitle(tr("串口调试模拟器"));
    resize(820, 560);
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<QVector<int16_t>>("QVector<int16_t>");

    m_simulator = new SimulatorSerial();
    m_channelRegisterMap = QVector<quint16>(8, 0xFFFF);
    m_channelRegisterMap[0] = 0x0010;

    ui->setupUi(this);
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);

    m_portCombo = ui->portCombo;
    m_baudCombo = ui->baudCombo;
    m_scanButton = ui->scanButton;
    m_connectButton = ui->connectButton;
    m_statusBadge = ui->statusBadge;
    m_scanAddrEdit = ui->scanAddrEdit;
    m_icAddrResultCombo = ui->icAddrResultCombo;
    m_regReadValueEdit = ui->regReadValueEdit;
    m_writeResultCombo = ui->writeResultCombo;
    m_delaySpinBox = ui->delaySpinBox;
    m_logEdit = ui->logEdit;
    m_clearLogButton = ui->clearLogButton;

    m_portCombo->setPlaceholderText(tr("选择端口"));
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_delaySpinBox->setValue(0);
    m_streamBaseValue = registerValueAt(0x0010);

    connectSignals();
    refreshPortList();
    setConnectedState(false);
    m_streamThread = std::thread(&SerialDebugTab::streamWorkerLoop, this);
}

SerialDebugTab::~SerialDebugTab() {
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

QString SerialDebugTab::describeIncoming(uint8_t cmd, const QByteArray &data) {
    switch (cmd) {
    case 0x00: return QStringLiteral("HEARTBEAT");
    case MotorProtocol::CmdI2cBusScan: return QStringLiteral("I2C_SCAN");
    case MotorProtocol::CmdSetMotorIcAddr: return !data.isEmpty() ? QStringLiteral("SET_IC_ADDR addr=%1").arg(formatByte(static_cast<uint8_t>(data.at(0)))) : QStringLiteral("SET_IC_ADDR");
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

void SerialDebugTab::connectSignals() {
    connect(m_scanButton, &QPushButton::clicked, this, &SerialDebugTab::refreshPortList);
    connect(m_clearLogButton, &QPushButton::clicked, m_logEdit, &QTextEdit::clear);
    connect(m_regReadValueEdit, &QLineEdit::textChanged, this, [this]() { std::lock_guard<std::mutex> lock(m_streamMutex); m_streamBaseValue = registerValueAt(0x0010); });
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_isConnected) { QMetaObject::invokeMethod(m_simulator, "closePort", Qt::QueuedConnection); return; }
        const QString portName = m_portCombo->currentText().trimmed();
        if (portName.isEmpty()) { appendSysLog(tr("未选择串口"), true); return; }
        QMetaObject::invokeMethod(m_simulator, "openPort", Qt::QueuedConnection, Q_ARG(QString, portName), Q_ARG(qint32, m_baudCombo->currentText().toInt()));
    });
    connect(m_simulator, &SimulatorSerial::connected, this, [this]() { setConnectedState(true); appendSysLog(tr("模拟器已连接")); });
    connect(m_simulator, &SimulatorSerial::disconnected, this, [this]() {
        setConnectedState(false);
        { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampling = false; m_pendingDebugFrames.clear(); m_debugFlushQueued = false; }
        emit debugStreamActiveChanged(false);
        m_streamCv.notify_all();
        appendSysLog(tr("模拟器已断开"));
    });
    connect(m_simulator, &SimulatorSerial::errorOccurred, this, [this](const QString &message) { appendSysLog(message, true); });
    connect(m_simulator, &SimulatorSerial::frameReceived, this, &SerialDebugTab::onFrameReceived);
}

void SerialDebugTab::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) { appendLog(QStringLiteral("RX"), cmd, seq, data, describeIncoming(cmd, data)); dispatchWithDelay(cmd, seq, data); }

void SerialDebugTab::dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (cmd == 0x00) { handleHeartbeat(seq); return; }
    const auto dispatch = [this, cmd, seq, data]() {
        switch (cmd) {
        case MotorProtocol::CmdI2cBusScan: handleI2cScan(seq, data); break;
        case MotorProtocol::CmdSetMotorIcAddr: handleSetIcAddr(seq, data); break;
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
    const int delayMs = m_delaySpinBox->value();
    if (delayMs > 0) { QTimer::singleShot(delayMs, this, dispatch); return; }
    dispatch();
}

void SerialDebugTab::handleHeartbeat(uint8_t seq) { sendResponseFrame(seq, 0x00, {}); appendLog(QStringLiteral("TX"), 0x00, seq, {}, QStringLiteral("HEARTBEAT echo")); }
void SerialDebugTab::handleI2cScan(uint8_t seq, const QByteArray &data) {
    Q_UNUSED(data);
    QByteArray response; QStringList accepted;
    const QStringList tokens = m_scanAddrEdit->text().split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
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
    appendLog(QStringLiteral("TX"), MotorProtocol::CmdI2cBusScan, seq, response, accepted.isEmpty() ? QStringLiteral("I2C_SCAN → (空)") : QStringLiteral("I2C_SCAN → %1").arg(accepted.join(QStringLiteral(", "))));
}
void SerialDebugTab::handleSetIcAddr(uint8_t seq, const QByteArray &data) { Q_UNUSED(data); if (m_icAddrResultCombo->currentIndex() == 0) { sendResponseFrame(seq, MotorProtocol::CmdSetMotorIcAddr, {}); appendLog(QStringLiteral("TX"), MotorProtocol::CmdSetMotorIcAddr, seq, {}, QStringLiteral("SET_IC_ADDR → 成功")); return; } sendErrorFrame(seq, 0x03); }
void SerialDebugTab::handleReadRegister(uint8_t seq, const QByteArray &data) { if (data.size() < 2) { sendErrorFrame(seq, 0x03); return; } const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1))); const qint16 value = registerValueAt(addr); const QByteArray response = encodeWord(static_cast<quint16>(value)); sendResponseFrame(seq, MotorProtocol::CmdReadRegister, response); appendLog(QStringLiteral("TX"), MotorProtocol::CmdReadRegister, seq, response, QStringLiteral("READ → %1").arg(formatWord(static_cast<quint16>(value)))); }
void SerialDebugTab::handleWriteRegister(uint8_t seq, const QByteArray &data) { if (data.size() < 4) { sendErrorFrame(seq, 0x03); return; } if (m_writeResultCombo->currentText() != QStringLiteral("ACK")) { sendErrorFrame(seq, 0x03); return; } sendResponseFrame(seq, MotorProtocol::CmdWriteRegister, {}); appendLog(QStringLiteral("TX"), MotorProtocol::CmdWriteRegister, seq, {}, QStringLiteral("WRITE → ACK")); Q_UNUSED(data); }
void SerialDebugTab::handleStartSampling(uint8_t seq) {
    bool hasValidMapping = false;
    for (int index = 0; index < m_channelRegisterMap.size(); ++index) { if ((m_channelMask & (1u << index)) != 0 && m_channelRegisterMap.at(index) != 0xFFFF) { hasValidMapping = true; break; } }
    if (!hasValidMapping) { sendErrorFrame(seq, 0x03); sendDebugInfo(QStringLiteral("Start sampling failed: no valid channel mapping")); return; }
    { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampling = true; m_streamTick = 0; m_pendingDebugFrames.clear(); m_debugFlushQueued = false; m_lastStreamActualUs = -1; m_streamActualUsAccumulator = 0; m_streamActualUsSamples = 0; }
    m_streamElapsedTimer.restart(); m_streamCv.notify_all(); sendResponseFrame(seq, MotorProtocol::CmdStartSampling, {}); emit debugStreamActiveChanged(true); appendLog(QStringLiteral("TX"), MotorProtocol::CmdStartSampling, seq, {}, QStringLiteral("SCOPE_START → ACK"));
}
void SerialDebugTab::handleStopSampling(uint8_t seq) { { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampling = false; m_pendingDebugFrames.clear(); m_debugFlushQueued = false; m_lastStreamActualUs = -1; m_streamActualUsAccumulator = 0; m_streamActualUsSamples = 0; } m_streamCv.notify_all(); sendResponseFrame(seq, MotorProtocol::CmdStopSampling, {}); emit debugStreamActiveChanged(false); appendLog(QStringLiteral("TX"), MotorProtocol::CmdStopSampling, seq, {}, QStringLiteral("SCOPE_STOP → ACK")); }
void SerialDebugTab::handleSetSampleInterval(uint8_t seq, const QByteArray &data) { if (data.size() != 1) { sendErrorFrame(seq, 0x03); return; } const uint8_t index = static_cast<uint8_t>(data.at(0)); if (!MotorProtocol::isValidSampleIntervalIndex(index)) { sendErrorFrame(seq, 0x03); return; } { std::lock_guard<std::mutex> lock(m_streamMutex); m_sampleIntervalIndex = kFixedSampleIntervalIndex; } m_streamCv.notify_all(); sendResponseFrame(seq, MotorProtocol::CmdSetSampleInterval, {}); appendLog(QStringLiteral("TX"), MotorProtocol::CmdSetSampleInterval, seq, {}, QStringLiteral("SCOPE_INTERVAL → %1").arg(formatByte(kFixedSampleIntervalIndex))); }
void SerialDebugTab::handleSetSampleChannels(uint8_t seq, const QByteArray &data) { if (data.size() != 1) { sendErrorFrame(seq, 0x03); return; } const uint8_t mask = static_cast<uint8_t>(data.at(0)); if (!MotorProtocol::isValidSampleChannelMask(mask)) { sendErrorFrame(seq, 0x03); return; } { std::lock_guard<std::mutex> lock(m_streamMutex); m_channelMask = mask; } sendResponseFrame(seq, MotorProtocol::CmdSetSampleChannels, {}); appendLog(QStringLiteral("TX"), MotorProtocol::CmdSetSampleChannels, seq, {}, QStringLiteral("SCOPE_CHANNELS → %1").arg(formatByte(mask))); }
void SerialDebugTab::handleSetChannelRegisterMap(uint8_t seq, const QByteArray &data) { if (data.size() != 16) { sendErrorFrame(seq, 0x03); return; } { std::lock_guard<std::mutex> lock(m_streamMutex); for (int index = 0; index < 8; ++index) { const int offset = index * 2; m_channelRegisterMap[index] = static_cast<quint16>((static_cast<quint8>(data.at(offset)) << 8) | static_cast<quint8>(data.at(offset + 1))); } } sendResponseFrame(seq, MotorProtocol::CmdSetChannelRegisterMap, {}); appendLog(QStringLiteral("TX"), MotorProtocol::CmdSetChannelRegisterMap, seq, {}, QStringLiteral("SCOPE_MAP → ACK")); }
void SerialDebugTab::handleUnknownCommand(uint8_t cmd, uint8_t seq) { sendErrorFrame(seq, 0x02); appendLog(QStringLiteral("TX"), cmd, seq, {}, tr("未知命令")); }

void SerialDebugTab::sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data) { const QByteArray frame = FrameParser::encodeControlFrame(seq, cmd, data); QMetaObject::invokeMethod(m_simulator, "sendRawFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame)); }
void SerialDebugTab::sendErrorFrame(uint8_t seq, uint8_t errorCode) { const QByteArray data(1, static_cast<char>(errorCode)); sendResponseFrame(seq, MotorProtocol::CmdErrorResponse, data); appendLog(QStringLiteral("TX"), MotorProtocol::CmdErrorResponse, seq, data, QStringLiteral("ERROR %1").arg(formatByte(errorCode))); }
void SerialDebugTab::sendDebugInfo(const QString &message) { const QByteArray data = message.toUtf8().left(255); sendResponseFrame(0xFF, 0x06, data); appendLog(QStringLiteral("TX"), 0x06, 0xFF, data, QStringLiteral("DEBUG_INFO")); }

void SerialDebugTab::emitOneStreamFrame() {
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

void SerialDebugTab::queueDebugStreamFrame(uint8_t channelMask, const QVector<int16_t> &samples) {
    bool shouldScheduleFlush = false;
    { std::lock_guard<std::mutex> lock(m_streamMutex); if (!m_sampling || m_streamStopRequested) return; PendingDebugFrame frame; frame.channelMask = channelMask; frame.samples = samples; m_pendingDebugFrames.append(std::move(frame)); if (!m_debugFlushQueued) { m_debugFlushQueued = true; shouldScheduleFlush = true; } }
    if (!shouldScheduleFlush) return;
    QMetaObject::invokeMethod(this, &SerialDebugTab::flushPendingDebugStream, Qt::QueuedConnection);
}

void SerialDebugTab::flushPendingDebugStream() {
    QVector<PendingDebugFrame> frames;
    { std::lock_guard<std::mutex> lock(m_streamMutex); frames.swap(m_pendingDebugFrames); m_debugFlushQueued = false; }
    for (const PendingDebugFrame &frame : frames) emit debugStreamGenerated(frame.channelMask, frame.samples);
    bool shouldScheduleAgain = false;
    { std::lock_guard<std::mutex> lock(m_streamMutex); if (!m_pendingDebugFrames.isEmpty() && !m_debugFlushQueued) { m_debugFlushQueued = true; shouldScheduleAgain = true; } }
    if (shouldScheduleAgain) QMetaObject::invokeMethod(this, &SerialDebugTab::flushPendingDebugStream, Qt::QueuedConnection);
}

void SerialDebugTab::streamWorkerLoop() {
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

void SerialDebugTab::appendSysLog(const QString &message, bool isError) {
    const QString color = isError ? Style::Color::LogError.name() : Style::Color::MutedText.name();
    const QString line = QStringLiteral("[%1] SYS  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), message);
    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

void SerialDebugTab::appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note) {
    QString color = Style::Color::AppText.name();
    if (dir == QStringLiteral("TX")) color = (cmd == MotorProtocol::CmdErrorResponse) ? Style::Color::LogError.name() : Style::Color::ReadButtonForeground.name();
    QString dataText;
    if (!data.isEmpty()) { QStringList bytes; bytes.reserve(data.size()); for (char byte : data) bytes.append(formatByte(static_cast<uint8_t>(byte))); dataText = bytes.join(QLatin1Char(' ')); }
    QString line = QStringLiteral("[%1] %2 cmd=%3 seq=%4").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"))).arg(dir).arg(formatByte(cmd)).arg(formatByte(seq));
    if (!dataText.isEmpty()) line.append(QStringLiteral("  %1").arg(dataText));
    if (!note.isEmpty()) line.append(QStringLiteral("  (%1)").arg(note));
    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

void SerialDebugTab::refreshPortList() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = SimulatorSerial::availablePorts();
    const QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) m_portCombo->setCurrentIndex(currentIndex);
}

void SerialDebugTab::setConnectedState(bool connected) {
    m_isConnected = connected;
    { std::lock_guard<std::mutex> lock(m_streamMutex); m_isConnected = connected; }
    m_streamCv.notify_all();
    m_connectButton->setText(connected ? tr("断开") : tr("连接"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_statusBadge->setText(connected ? tr("● 已连接") : tr("● 未连接"));
    m_statusBadge->setProperty("connected", connected);
    UiUtil::repolish(m_statusBadge);
}

qint16 SerialDebugTab::registerValueAt(quint16 addr) const {
    Q_UNUSED(addr);
    bool ok = false;
    QString text = m_regReadValueEdit->text().trimmed();
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) text = text.mid(2);
    const quint16 raw = text.toUShort(&ok, 16);
    return ok ? static_cast<qint16>(raw) : 0;
}
