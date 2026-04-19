// ScopeService — 采样状态机、协议命令序列、流数据转发
#include "services/scopeservice.h"

#include "models/scopechannelmodel.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QDebug>
#include <QMetaObject>
#include <QStringList>
#include <QThread>
#include <QTimer>



namespace {
QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}
}

// --- ScopeStreamBatcher ---

ScopeStreamBatcher::ScopeStreamBatcher(QObject *parent)
    : QObject(parent) {
}

void ScopeStreamBatcher::enqueue(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (samples.isEmpty()) return;
    ensureTimer();
    ScopeStreamPacket packet;
    packet.channelMask = channelMask;
    packet.samples = samples;
    m_pendingBatch.append(packet);
    if (!m_flushTimer->isActive()) m_flushTimer->start();
}

void ScopeStreamBatcher::clearPending() {
    m_pendingBatch.clear();
    if (m_flushTimer != nullptr) m_flushTimer->stop();
}

void ScopeStreamBatcher::ensureTimer() {
    if (m_flushTimer != nullptr) return;
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(8);
    connect(m_flushTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingBatch.isEmpty()) { m_flushTimer->stop(); return; }
        ScopeStreamBatch batch = m_pendingBatch;
        m_pendingBatch.clear();
        emit batchReady(batch);
    });
}

// --- ScopeService ---

ScopeService::ScopeService(SerialManager *serialManager, ScopeChannelModel *channelModel, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_channelModel(channelModel) {
    qRegisterMetaType<ScopeStreamBatch>("ScopeStreamBatch");

    if (m_serialManager != nullptr) {
        m_streamBatcher = new ScopeStreamBatcher();
        m_streamBatcher->moveToThread(m_serialManager->thread());
        connect(m_serialManager->thread(), &QThread::finished, m_streamBatcher, &QObject::deleteLater);
        connect(m_serialManager, &SerialManager::commandSent, this, &ScopeService::handleCommandSent);
        connect(m_serialManager, &SerialManager::frameReceived, this, &ScopeService::handleFrameReceived);
        connect(m_serialManager, &SerialManager::streamDataReceived, m_streamBatcher, &ScopeStreamBatcher::enqueue, Qt::DirectConnection);
        connect(m_streamBatcher, &ScopeStreamBatcher::batchReady, this, &ScopeService::handleStreamBatchReceived, Qt::QueuedConnection);
        connect(m_serialManager, &SerialManager::errorOccurred, this, &ScopeService::handleErrorOccurred);
    }
}

ScopeService::~ScopeService() = default;

void ScopeService::requestStart(uint8_t sampleIntervalIndex, int displayWindowMs) {
    m_sampleIntervalIndex = sampleIntervalIndex;
    m_displayWindowMs = displayWindowMs;
    if (m_serialManager == nullptr || m_pendingCommand != PendingCommand::None) {
        emit runningChanged(m_running);
        return;
    }
    if (!m_channelModel->hasValidSamplingConfig()) {
        m_lastError = QStringLiteral("No valid scope channel mapping");
        setRunning(false);
        emit startError(m_lastError);
        qDebug().noquote() << QStringLiteral("[Scope GUI] Start blocked: %1").arg(m_lastError);
        return;
    }
    m_startPending = true;
    m_stopPending = false;
    m_hasReceivedStream = false;
    clearPendingStreamBatch();
    emit acquisitionConfigured(sampleIntervalUsForIndex(m_sampleIntervalIndex), m_displayWindowMs);
    emit resetViewRequested();
    logStartSnapshot();
    sendNextStartCommand();
}

void ScopeService::requestStop() {
    if (!m_running || m_pendingCommand != PendingCommand::None || m_serialManager == nullptr) {
        emit runningChanged(m_running);
        return;
    }
    const QByteArray payload = MotorProtocol::encodeStopSampling();
    if (!sendCommand(MotorProtocol::CmdStopSampling, payload)) {
        emit runningChanged(m_running);
        return;
    }
    clearPendingStreamBatch();
    m_stopPending = true;
    m_pendingCommand = PendingCommand::StopSampling;
    emit runningChanged(m_running);
    qDebug().noquote() << QStringLiteral("[Scope GUI] TX stop sampling");
}

void ScopeService::ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (!m_running || !m_debugStreamActive) return;
    m_lastStreamMask = channelMask;
    m_hasReceivedStream = true;
    m_perfBatchCount += 1;
    m_perfSampleCount += samples.size();
    emit samplesReceived(channelMask, samples);
}

void ScopeService::setRunning(bool running) {
    if (m_running == running) {
        emit runningChanged(running);
        return;
    }
    m_running = running;
    emit runningChanged(running);
    qDebug().noquote() << QStringLiteral("[Scope GUI] %1").arg(running ? QStringLiteral("Start") : QStringLiteral("Stop"));
}

void ScopeService::startSamplingSequence() {
    sendNextStartCommand();
}

void ScopeService::sendNextStartCommand() {
    if (!m_startPending) return;
    switch (m_pendingCommand) {
    case PendingCommand::None: {
        const QByteArray payload = MotorProtocol::encodeSetSampleInterval(m_sampleIntervalIndex);
        if (!sendCommand(MotorProtocol::CmdSetSampleInterval, payload)) { m_startPending = false; setRunning(false); return; }
        m_pendingCommand = PendingCommand::SetSampleInterval;
        qDebug().noquote() << QStringLiteral("[Scope GUI] TX set sample interval %1").arg(formatByte(m_sampleIntervalIndex));
        return;
    }
    case PendingCommand::SetSampleInterval: {
        const uint8_t mask = m_channelModel->currentChannelMask();
        const QByteArray payload = MotorProtocol::encodeSetSampleChannels(mask);
        if (!sendCommand(MotorProtocol::CmdSetSampleChannels, payload)) { m_startPending = false; setRunning(false); return; }
        m_pendingCommand = PendingCommand::SetSampleChannels;
        qDebug().noquote() << QStringLiteral("[Scope GUI] TX set sample channels %1").arg(formatByte(mask));
        return;
    }
    case PendingCommand::SetSampleChannels: {
        const QByteArray payload = MotorProtocol::encodeSetChannelRegisterMap(m_channelModel->currentRegisterMap());
        if (!sendCommand(MotorProtocol::CmdSetChannelRegisterMap, payload)) { m_startPending = false; setRunning(false); return; }
        m_pendingCommand = PendingCommand::SetChannelRegisterMap;
        qDebug().noquote() << QStringLiteral("[Scope GUI] TX set channel register map");
        return;
    }
    case PendingCommand::SetChannelRegisterMap: {
        const QByteArray payload = MotorProtocol::encodeStartSampling();
        if (!sendCommand(MotorProtocol::CmdStartSampling, payload)) { m_startPending = false; setRunning(false); return; }
        m_pendingCommand = PendingCommand::StartSampling;
        qDebug().noquote() << QStringLiteral("[Scope GUI] TX start sampling");
        return;
    }
    case PendingCommand::StartSampling:
    case PendingCommand::StopSampling:
        return;
    }
}

void ScopeService::finishPendingCommand() {
    switch (m_pendingCommand) {
    case PendingCommand::SetSampleInterval:
    case PendingCommand::SetSampleChannels:
    case PendingCommand::SetChannelRegisterMap:
        m_pendingSeq = InvalidSeq;
        sendNextStartCommand();
        return;
    case PendingCommand::StartSampling:
        m_startPending = false;
        clearPendingCommandState();
        setRunning(true);
        return;
    case PendingCommand::StopSampling:
        m_stopPending = false;
        clearPendingCommandState();
        setRunning(false);
        return;
    case PendingCommand::None:
        return;
    }
}

void ScopeService::clearPendingCommandState() { m_pendingCommand = PendingCommand::None; m_pendingSeq = InvalidSeq; }

void ScopeService::handleCommandSent(uint8_t cmd, uint8_t seq) {
    if (m_pendingCommand == PendingCommand::None) return;
    if (cmd != commandForPending(m_pendingCommand)) return;
    m_pendingSeq = seq;
}

uint8_t ScopeService::commandForPending(PendingCommand pending) {
    switch (pending) {
    case PendingCommand::SetSampleInterval: return MotorProtocol::CmdSetSampleInterval;
    case PendingCommand::SetSampleChannels: return MotorProtocol::CmdSetSampleChannels;
    case PendingCommand::SetChannelRegisterMap: return MotorProtocol::CmdSetChannelRegisterMap;
    case PendingCommand::StartSampling: return MotorProtocol::CmdStartSampling;
    case PendingCommand::StopSampling: return MotorProtocol::CmdStopSampling;
    case PendingCommand::None: return InvalidSeq;
    }
    return InvalidSeq;
}

void ScopeService::handleFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (cmd == MotorProtocol::CmdErrorResponse) {
        if (m_pendingSeq != InvalidSeq && seq != m_pendingSeq) return;
        m_lastError = QStringLiteral("Protocol error %1").arg(formatByte(MotorProtocol::decodeErrorCode(data)));
        m_startPending = false; m_stopPending = false;
        clearPendingCommandState();
        setRunning(false);
        qDebug().noquote() << QStringLiteral("[Scope GUI] %1").arg(m_lastError);
        return;
    }
    if (cmd == MotorProtocol::CmdDebugInfo) {
        m_lastError = QString::fromUtf8(data);
        qDebug().noquote() << QStringLiteral("[Scope GUI] Debug info=%1").arg(m_lastError);
        return;
    }
    if (m_pendingSeq != InvalidSeq && seq != m_pendingSeq) return;
    switch (m_pendingCommand) {
    case PendingCommand::SetSampleInterval: if (cmd == MotorProtocol::CmdSetSampleInterval) finishPendingCommand(); break;
    case PendingCommand::SetSampleChannels: if (cmd == MotorProtocol::CmdSetSampleChannels) finishPendingCommand(); break;
    case PendingCommand::SetChannelRegisterMap: if (cmd == MotorProtocol::CmdSetChannelRegisterMap) finishPendingCommand(); break;
    case PendingCommand::StartSampling: if (cmd == MotorProtocol::CmdStartSampling) finishPendingCommand(); break;
    case PendingCommand::StopSampling: if (cmd == MotorProtocol::CmdStopSampling) finishPendingCommand(); break;
    case PendingCommand::None: break;
    }
}

void ScopeService::handleErrorOccurred(const QString &message) {
    if (m_pendingCommand == PendingCommand::None) return;
    m_lastError = message;
    m_startPending = false; m_stopPending = false;
    clearPendingCommandState();
    setRunning(false);
    qDebug().noquote() << QStringLiteral("[Scope GUI] Serial error=%1").arg(message);
}

void ScopeService::handleStreamBatchReceived(const ScopeStreamBatch &batch) {
    if (!m_running || m_debugStreamActive || batch.isEmpty()) return;
    m_perfBatchCount += 1;
    for (const ScopeStreamPacket &packet : batch) {
        m_lastStreamMask = packet.channelMask;
        m_hasReceivedStream = true;
        m_perfSampleCount += packet.samples.size();
        emit samplesReceived(packet.channelMask, packet.samples);
    }
}

void ScopeService::clearPendingStreamBatch() {
    if (m_streamBatcher == nullptr) return;
    QMetaObject::invokeMethod(m_streamBatcher, &ScopeStreamBatcher::clearPending, Qt::QueuedConnection);
}

void ScopeService::logStartSnapshot() const {
    const int intervalUs = sampleIntervalUsForIndex(m_sampleIntervalIndex);
    const int rawWindowPoints = qMax(1, qRound((static_cast<double>(m_displayWindowMs) * 1000.0) / static_cast<double>(intervalUs)));
    const int bucket = qMax(1, (rawWindowPoints + 3000 - 1) / 3000);
    const int uiPoints = qMax(1, (rawWindowPoints + bucket - 1) / bucket);
    QStringList activeChannels;
    for (int index = 0; index < m_channelModel->channelCount(); ++index) {
        const ScopeChannelState &channel = m_channelModel->channel(index);
        if (!channel.enabled || channel.registerAddress == 0xFFFF) continue;
        activeChannels.append(QStringLiteral("CH%1=%2").arg(index + 1).arg(channel.addressText.isEmpty() ? QStringLiteral("0xFFFF") : channel.addressText));
    }
    qDebug().noquote() << QStringLiteral("[Scope Start] interval=%1us index=%2 window=%3ms raw_points=%4 bucket=%5 ui_points=%6 mask=%7 channels=%8")
                              .arg(intervalUs).arg(formatByte(m_sampleIntervalIndex)).arg(m_displayWindowMs).arg(rawWindowPoints).arg(bucket).arg(uiPoints).arg(formatByte(m_channelModel->currentChannelMask())).arg(activeChannels.join(QStringLiteral(", ")));
}

bool ScopeService::sendCommand(uint8_t cmd, const QByteArray &data) {
    if (m_serialManager == nullptr) {
        m_lastError = QStringLiteral("Serial manager is unavailable");
        qDebug().noquote() << QStringLiteral("[Scope GUI] %1").arg(m_lastError);
        return false;
    }
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, cmd), Q_ARG(QByteArray, data));
    return true;
}

uint8_t ScopeService::sampleIntervalIndexForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("100 us")) return 0x00;
    if (normalized == QStringLiteral("200 us")) return 0x01;
    if (normalized == QStringLiteral("300 us")) return 0x02;
    if (normalized == QStringLiteral("500 us")) return 0x03;
    if (normalized == QStringLiteral("750 us")) return 0x04;
    if (normalized == QStringLiteral("1500 us")) return 0x06;
    if (normalized == QStringLiteral("2000 us")) return 0x07;
    return 0x05;
}

int ScopeService::sampleIntervalUsForIndex(uint8_t index) {
    switch (index) {
    case 0x00: return 100; case 0x01: return 200; case 0x02: return 300;
    case 0x03: return 500; case 0x04: return 750; case 0x05: return 1000;
    case 0x06: return 1500; case 0x07: return 2000; default: return 1000;
    }
}

int ScopeService::displayWindowMsForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("50 ms")) return 50;
    if (normalized == QStringLiteral("200 ms")) return 200;
    if (normalized == QStringLiteral("500 ms")) return 500;
    if (normalized == QStringLiteral("1000 ms")) return 1000;
    if (normalized == QStringLiteral("2000 ms")) return 2000;
    if (normalized == QStringLiteral("4000 ms")) return 4000;
    return 50;
}
