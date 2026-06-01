// =============================================================================
// @file    scopeservice.cpp
// @brief   示波器服务实现 — 采样命令序列调度、流帧批量接收、状态管理
//
// ScopeService 管理示波器采样的完整生命周期：
//
// 启动序列（4 步命令链）：
//   1. CmdSetSampleInterval    — 设置采样间隔
//   2. CmdSetSampleChannels    — 设置通道掩码
//   3. CmdSetChannelRegisterMap — 设置通道→寄存器映射表
//   4. CmdStartSampling        — 启动采样
//   每步等待 ACK 后才发送下一步，任一步失败则终止整个序列。
//
// 数据接收路径：
//   SerialManager(worker线程) → ScopeStreamBatcher(worker线程,批量聚合)
//   → ScopeService::handleStreamBatchReceived(主线程) → samplesReceived 信号
//
// 背压机制：
//   ScopeStreamBatcher 通过 m_uiProcessing 原子标志实现背压。
//   当主线程正在处理上一批数据时（标志为 1），batcher 暂停投递，
//   防止 UI 处理不及时导致内存溢出。
//
// 看门狗：
//   采样启动后若 5 秒内无数据到达，自动停止采样并报告错误。
// =============================================================================
#include "services/scopeservice.h"

#include "models/channelbuffer.h"
#include "models/scopechannelmodel.h"
#include "protocol/motor_protocol.h"
#include "protocol/sampling_config.h"
#include "services/commanddispatcher.h"
#include "serialmanager.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QTimer>

Q_LOGGING_CATEGORY(lcScope, "motordev.scope")

namespace {
/// @brief 格式化单字节为 "0x00" 形式。
QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

/// @brief 格式化字节数组为大写十六进制字符串。
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
}

// =============================================================================
// ScopeStreamBatcher — 流数据批量聚合器（运行在 SerialManager 线程）
// =============================================================================

/// @brief 构造批量聚合器。
///
/// @param uiBusyFlag  指向主线程原子标志，用于背压控制
ScopeStreamBatcher::ScopeStreamBatcher(QAtomicInt *uiBusyFlag, QObject *parent)
    : QObject(parent)
    , m_uiBusy(uiBusyFlag) {
}

/// @brief 入队一帧流数据。
///
/// - 队列超过 kMaxPendingPackets 时丢弃最旧的帧（背压溢出保护）
/// - 若定时器未运行则启动，由定时器统一批量投递到主线程
void ScopeStreamBatcher::enqueue(uint8_t channelMask, QVector<int16_t> samples) {
    if (samples.isEmpty()) {
        return;
    }
    ensureTimer();

    // 溢出保护：队列过深时丢弃最旧的帧
    if (m_pendingBatch.size() >= kMaxPendingPackets) {
        m_pendingBatch.removeFirst();
        if (!m_overflowWarned) {
            m_overflowWarned = true;
            qCWarning(lcScope) << "ScopeStreamBatcher overflow: dropping oldest packets";
        }
    }

    ScopeStreamPacket packet;
    packet.channelMask = channelMask;
    packet.samples = std::move(samples);
    m_pendingBatch.append(std::move(packet));

    if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

/// @brief 清空待处理队列并停止定时器。
void ScopeStreamBatcher::clearPending() {
    m_pendingBatch.clear();
    if (m_flushTimer != nullptr) {
        m_flushTimer->stop();
    }
}

/// @brief 懒初始化 8 ms 定时器，用于批量投递。
///
/// 定时器回调逻辑：
///   1. 队列为空 → 停止定时器
///   2. 主线程正忙（m_uiBusy != 0）→ 跳过本次投递（背压）
///   3. 否则 → 一次性取走全部待处理帧，通过 batchReady 信号投递到主线程
void ScopeStreamBatcher::ensureTimer() {
    if (m_flushTimer != nullptr) {
        return;
    }
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(8);                       // ≈ 125 Hz 投递频率
    connect(m_flushTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingBatch.isEmpty()) {
            m_flushTimer->stop();
            return;
        }
        // 背压：主线程正在处理上一批，暂不投递
        if (m_uiBusy != nullptr && m_uiBusy->loadAcquire() != 0) {
            return;
        }
        // 取走全部待处理帧，一次性投递
        ScopeStreamBatch batch = std::move(m_pendingBatch);
        m_pendingBatch.clear();
        m_pendingBatch.reserve(64);
        m_overflowWarned = false;
        emit batchReady(batch);
    });
}

// =============================================================================
// ScopeService 构造
// =============================================================================

/// @brief 构造示波器服务，初始化看门狗和流数据接收管道。
///
/// 流数据管道：
///   SerialManager::streamDataReceived → ScopeStreamBatcher::enqueue（同线程直连）
///   ScopeStreamBatcher::batchReady → ScopeService::handleStreamBatchReceived（跨线程队列连接）
ScopeService::ScopeService(SerialManager *serialManager,
                           CommandDispatcher *dispatcher,
                           ScopeChannelModel *channelModel,
                           QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_dispatcher(dispatcher)
    , m_channelModel(channelModel) {
    qRegisterMetaType<ScopeStreamBatch>("ScopeStreamBatch");

    // ---------- 流看门狗：5 秒无数据则超时 ----------
    m_streamWatchdog = new QTimer(this);
    m_streamWatchdog->setSingleShot(true);
    m_streamWatchdog->setInterval(StreamWatchdogMs);
    connect(m_streamWatchdog, &QTimer::timeout, this, [this]() {
        if (!m_running) {
            return;
        }
        qCWarning(lcScope) << "Stream watchdog timeout: no data for 5 seconds";
        m_lastError = QStringLiteral("Data stream interrupted");
        emit startError(m_lastError);
        requestStop();
    });

    // ---------- 创建流批量聚合器（运行在 SerialManager 线程） ----------
    if (m_serialManager != nullptr) {
        m_streamBatcher = new ScopeStreamBatcher(&m_uiProcessing);
        m_streamBatcher->moveToThread(m_serialManager->thread());
        // 线程结束时自动清理 batcher
        connect(m_serialManager->thread(), &QThread::finished, m_streamBatcher, &QObject::deleteLater);
        // 流数据管道连接
        connect(m_serialManager, &SerialManager::streamDataReceived,
                m_streamBatcher, &ScopeStreamBatcher::enqueue);
        connect(m_streamBatcher, &ScopeStreamBatcher::batchReady,
                this, &ScopeService::handleStreamBatchReceived, Qt::QueuedConnection);
    }
}

ScopeService::~ScopeService() = default;

// =============================================================================
// 采样启动请求
// =============================================================================

/// @brief 请求启动采样：校验配置 → 发射配置信号 → 开始 4 步命令序列。
///
/// @param sampleIntervalIndex  采样间隔索引（协议定义）
/// @param displayWindowMs      显示窗口时长（毫秒）
void ScopeService::requestStart(uint8_t sampleIntervalIndex, int displayWindowMs) {
    m_sampleIntervalIndex = sampleIntervalIndex;
    m_displayWindowMs = displayWindowMs;

    // 前置条件检查
    if (m_dispatcher == nullptr || m_pendingCommand != PendingCommand::None) {
        emit runningChanged(m_running);
        return;
    }
    if (!m_channelModel->hasValidSamplingConfig()) {
        m_lastError = QStringLiteral("No valid scope channel mapping");
        setRunning(false);
        emit startError(m_lastError);
        qCWarning(lcScope).noquote() << QStringLiteral("Start blocked: %1").arg(m_lastError);
        return;
    }

    // 设置启动状态
    m_startPending = true;
    m_stopPending = false;
    m_hasReceivedStream = false;
    m_insufficientTimeArmed = true;  // 武装"采样时间不够"检测；仅采样启动尝试期间命中 0x06 才弹窗
    clearPendingStreamBatch();

    // 通知 UI 配置采集参数
    emit acquisitionConfigured(SamplingConfig::intervalUsForIndex(m_sampleIntervalIndex), m_displayWindowMs);
    emit resetViewRequested();

    // 日志记录启动快照
    logStartSnapshot();

    // 开始 4 步命令序列
    sendNextStartCommand();
}

// =============================================================================
// 采样停止请求
// =============================================================================

/// @brief 请求停止采样：发送 StopSampling 命令。
void ScopeService::requestStop() {
    if (!m_running || m_pendingCommand != PendingCommand::None || m_dispatcher == nullptr) {
        emit runningChanged(m_running);
        return;
    }

    if (m_streamWatchdog != nullptr) {
        m_streamWatchdog->stop();
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
    qCInfo(lcScope).noquote()
        << QStringLiteral("%1 TX payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStopSampling)))
               .arg(formatPayloadHex(payload));
}

// =============================================================================
// 设备主动调试信息（0x06）
// =============================================================================

/// @brief 处理设备主动上报的 0x06 调试信息。
///
/// 仅关心"采样时间不够"(tick overrun)：识别后且当前确为采样启动尝试（m_insufficientTimeArmed），
/// emit samplingTimeInsufficient 供 UI 弹窗。一次启动尝试最多上报一次。其他调试信息忽略
/// （仍由 MainWindow 记入日志）。
void ScopeService::onDeviceDebugInfo(const QString &message) {
    int estimatedUs = -1;
    int limitUs = -1;
    if (!MotorProtocol::parseTickOverrunDebug(message, &estimatedUs, &limitUs)) {
        return;  // 非"采样时间不够"调试信息
    }
    if (!m_insufficientTimeArmed) {
        return;  // 非采样启动尝试引发，或已上报（去抖）
    }
    m_insufficientTimeArmed = false;

    QString detail;
    if (estimatedUs >= 0 && limitUs >= 0) {
        detail = QStringLiteral("estimated %1us > limit %2us").arg(estimatedUs).arg(limitUs);
    }
    qCWarning(lcScope).noquote()
        << QStringLiteral("Sampling rejected: insufficient tick budget (%1)")
               .arg(detail.isEmpty() ? message : detail);
    emit samplingTimeInsufficient(detail);
}

// =============================================================================
// 调试流注入
// =============================================================================

/// @brief 注入调试模式的流数据（来自 SimulatorService）。
///
/// 调试流不经过 ScopeStreamBatcher，直接进入主线程处理。
void ScopeService::ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (!m_running || !m_debugStreamActive) {
        return;
    }
    m_lastStreamMask = channelMask;
    m_hasReceivedStream = true;
    m_perfBatchCount += 1;
    m_perfSampleCount += samples.size();
    emit samplesReceived(channelMask, samples);
}

// =============================================================================
// 内部状态管理
// =============================================================================

/// @brief 设置运行状态并发射 runningChanged 信号。
void ScopeService::setRunning(bool running) {
    if (!running && m_streamWatchdog != nullptr) {
        m_streamWatchdog->stop();
    }
    if (m_running == running) {
        emit runningChanged(running);
        return;
    }
    m_running = running;
    emit runningChanged(running);
    qCInfo(lcScope).noquote()
        << QStringLiteral("Sampling state=%1").arg(running ? QStringLiteral("Start") : QStringLiteral("Stop"));
}

// =============================================================================
// 4 步启动命令序列
// =============================================================================

/// @brief 根据当前 pendingCommand 状态发送下一步命令。
///
/// 状态机转换：
///   None → SetSampleInterval → SetSampleChannels
///        → SetChannelRegisterMap → StartSampling
///
/// 每步命令通过 sendCommand 提交后，在 handleResponse 中收到 ACK
/// 后调用 finishPendingCommand，finishPendingCommand 再调用本方法
/// 推进到下一步。
void ScopeService::sendNextStartCommand() {
    if (!m_startPending) {
        return;
    }
    switch (m_pendingCommand) {
    // --- 第 1 步：设置采样间隔 ---
    case PendingCommand::None: {
        const QByteArray payload = MotorProtocol::encodeSetSampleInterval(m_sampleIntervalIndex);
        if (!sendCommand(MotorProtocol::CmdSetSampleInterval, payload)) {
            m_startPending = false;
            setRunning(false);
            return;
        }
        m_pendingCommand = PendingCommand::SetSampleInterval;
        qCInfo(lcScope).noquote()
            << QStringLiteral("%1 TX intervalIndex=%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdSetSampleInterval)))
                   .arg(formatByte(m_sampleIntervalIndex))
                   .arg(formatPayloadHex(payload));
        return;
    }
    // --- 第 2 步：设置通道掩码 ---
    case PendingCommand::SetSampleInterval: {
        const uint8_t mask = m_channelModel->currentChannelMask();
        const QByteArray payload = MotorProtocol::encodeSetSampleChannels(mask);
        if (!sendCommand(MotorProtocol::CmdSetSampleChannels, payload)) {
            m_startPending = false;
            setRunning(false);
            return;
        }
        m_pendingCommand = PendingCommand::SetSampleChannels;
        qCInfo(lcScope).noquote()
            << QStringLiteral("%1 TX channelMask=%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdSetSampleChannels)))
                   .arg(formatByte(mask))
                   .arg(formatPayloadHex(payload));
        return;
    }
    // --- 第 3 步：设置通道寄存器映射 ---
    case PendingCommand::SetSampleChannels: {
        const QByteArray payload = MotorProtocol::encodeSetChannelRegisterMap(m_channelModel->currentRegisterMap());
        if (!sendCommand(MotorProtocol::CmdSetChannelRegisterMap, payload)) {
            m_startPending = false;
            setRunning(false);
            return;
        }
        m_pendingCommand = PendingCommand::SetChannelRegisterMap;
        qCInfo(lcScope).noquote()
            << QStringLiteral("%1 TX payload=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdSetChannelRegisterMap)))
                   .arg(formatPayloadHex(payload));
        return;
    }
    // --- 第 4 步：启动采样 ---
    case PendingCommand::SetChannelRegisterMap: {
        const QByteArray payload = MotorProtocol::encodeStartSampling();
        if (!sendCommand(MotorProtocol::CmdStartSampling, payload)) {
            m_startPending = false;
            setRunning(false);
            return;
        }
        m_pendingCommand = PendingCommand::StartSampling;
        qCInfo(lcScope).noquote()
            << QStringLiteral("%1 TX payload=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStartSampling)))
                   .arg(formatPayloadHex(payload));
        return;
    }
    case PendingCommand::StartSampling:
    case PendingCommand::StopSampling:
        return;
    }
}

/// @brief 当前命令完成后的后处理：推进序列或切换运行状态。
void ScopeService::finishPendingCommand() {
    switch (m_pendingCommand) {
    // 中间步骤完成 → 继续下一步
    case PendingCommand::SetSampleInterval:
    case PendingCommand::SetSampleChannels:
    case PendingCommand::SetChannelRegisterMap:
        sendNextStartCommand();
        return;

    // 启动命令 ACK → 采样开始运行
    case PendingCommand::StartSampling:
        m_startPending = false;
        clearPendingCommandState();
        setRunning(true);
        // 非调试模式下启动看门狗
        if (!m_debugStreamActive && m_streamWatchdog != nullptr) {
            m_streamWatchdog->start();
        }
        return;

    // 停止命令 ACK → 采样已停止
    case PendingCommand::StopSampling:
        m_stopPending = false;
        clearPendingCommandState();
        setRunning(false);
        return;

    case PendingCommand::None:
        return;
    }
}

/// @brief 重置待处理命令状态。
void ScopeService::clearPendingCommandState() {
    m_pendingCommand = PendingCommand::None;
}

// =============================================================================
// 命令响应处理
// =============================================================================

/// @brief 处理所有采样命令的响应。
///
/// - 错误响应 → 终止整个序列，重置状态
/// - 正常 ACK → 根据 pendingCommand 推进到下一步
void ScopeService::handleResponse(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);

    // ---------- 错误响应：终止所有挂起操作 ----------
    if (cmd == MotorProtocol::CmdErrorResponse) {
        m_lastError = QStringLiteral("Protocol error %1").arg(formatByte(MotorProtocol::decodeErrorCode(data)));
        m_startPending = false;
        m_stopPending = false;
        clearPendingCommandState();
        setRunning(false);
        qCWarning(lcScope).noquote()
            << QStringLiteral("%1 RX payload=%2 %3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(formatPayloadHex(data))
                   .arg(m_lastError);
        return;
    }

    // ---------- 正常 ACK：按 pendingCommand 分发 ----------
    switch (m_pendingCommand) {
    case PendingCommand::SetSampleInterval:
        if (cmd == MotorProtocol::CmdSetSampleInterval) {
            qCInfo(lcScope).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));
            finishPendingCommand();
        }
        break;
    case PendingCommand::SetSampleChannels:
        if (cmd == MotorProtocol::CmdSetSampleChannels) {
            qCInfo(lcScope).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));
            finishPendingCommand();
        }
        break;
    case PendingCommand::SetChannelRegisterMap:
        if (cmd == MotorProtocol::CmdSetChannelRegisterMap) {
            qCInfo(lcScope).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));
            finishPendingCommand();
        }
        break;
    case PendingCommand::StartSampling:
        if (cmd == MotorProtocol::CmdStartSampling) {
            qCInfo(lcScope).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));
            m_insufficientTimeArmed = false;  // 启动成功，解除"采样时间不够"检测
            finishPendingCommand();
        }
        break;
    case PendingCommand::StopSampling:
        if (cmd == MotorProtocol::CmdStopSampling) {
            qCInfo(lcScope).noquote()
                << QStringLiteral("%1 RX ACK payload=%2")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(formatPayloadHex(data));
            finishPendingCommand();
        }
        break;
    case PendingCommand::None:
        break;
    }
}

// =============================================================================
// 流数据接收
// =============================================================================

/// @brief 主线程接收批量流数据：逐帧发射 samplesReceived 信号。
///
/// 通过 m_uiProcessing 原子标志实现背压：
///   处理前设为 1 → batcher 暂停投递
///   处理后设为 0 → batcher 恢复投递
void ScopeService::handleStreamBatchReceived(const ScopeStreamBatch &batch) {
    if (!m_running || m_debugStreamActive || batch.isEmpty()) {
        return;
    }

    // 标记主线程正忙（背压）
    m_uiProcessing.storeRelease(1);

    // 重置看门狗（有数据到达说明流正常）
    if (m_streamWatchdog != nullptr) {
        m_streamWatchdog->start();
    }

    // 逐帧处理并发射信号
    m_perfBatchCount += 1;
    for (const ScopeStreamPacket &packet : batch) {
        m_lastStreamMask = packet.channelMask;
        m_hasReceivedStream = true;
        m_perfSampleCount += packet.samples.size();
        emit samplesReceived(packet.channelMask, packet.samples);
    }

    // 释放背压标志
    m_uiProcessing.storeRelease(0);
}

/// @brief 跨线程清空 batcher 中的待处理队列。
void ScopeService::clearPendingStreamBatch() {
    if (m_streamBatcher == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(m_streamBatcher, &ScopeStreamBatcher::clearPending, Qt::QueuedConnection);
}

// =============================================================================
// 日志与命令提交
// =============================================================================

/// @brief 记录采样启动时的完整参数快照到日志。
///
/// 包括：采样间隔、显示窗口、原始点数、降采样桶大小、UI 点数、
/// 通道掩码和各通道地址。
void ScopeService::logStartSnapshot() const {
    const int intervalUs = SamplingConfig::intervalUsForIndex(m_sampleIntervalIndex);
    // 计算原始窗口点数
    const int rawWindowPoints = qMax(
        1,
        qRound((static_cast<double>(m_displayWindowMs) * 1000.0) / static_cast<double>(intervalUs)));
    // 降采样桶大小：确保 UI 点数不超过 ChannelBuffer::kUiRingSize
    const int bucket = qMax(1, (rawWindowPoints + ChannelBuffer::kUiRingSize - 1) / ChannelBuffer::kUiRingSize);
    const int uiPoints = qMax(1, (rawWindowPoints + bucket - 1) / bucket);

    // 收集活跃通道信息
    QStringList activeChannels;
    for (int index = 0; index < m_channelModel->channelCount(); ++index) {
        const ScopeChannelState &channel = m_channelModel->channel(index);
        if (!channel.enabled || channel.registerAddress == 0xFFFF) {
            continue;
        }
        activeChannels.append(QStringLiteral("CH%1=%2")
                                  .arg(index + 1)
                                  .arg(channel.addressText.isEmpty()
                                           ? QStringLiteral("0xFFFF")
                                           : channel.addressText));
    }

    qCInfo(lcScope).noquote()
        << QStringLiteral("Start snapshot interval=%1us index=%2 window=%3ms raw_points=%4 bucket=%5 ui_points=%6 mask=%7 channels=%8")
               .arg(intervalUs)
               .arg(formatByte(m_sampleIntervalIndex))
               .arg(m_displayWindowMs)
               .arg(rawWindowPoints)
               .arg(bucket)
               .arg(uiPoints)
               .arg(formatByte(m_channelModel->currentChannelMask()))
               .arg(activeChannels.join(QStringLiteral(", ")));
}

/// @brief 通过 CommandDispatcher 提交命令并注册响应回调。
///
/// @return true=提交成功，false=分发器不可用
bool ScopeService::sendCommand(uint8_t cmd, const QByteArray &data) {
    if (m_dispatcher == nullptr) {
        m_lastError = QStringLiteral("Serial manager is unavailable");
        qCWarning(lcScope).noquote()
            << QStringLiteral("%1 for %2")
                   .arg(m_lastError)
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)));
        return false;
    }

    QPointer<ScopeService> guard(this);
    m_dispatcher->submitCommand(cmd, data, CommandDispatcher::High,
        [guard](uint8_t respCmd, uint8_t respSeq, const QByteArray &respData) {
            if (guard != nullptr) {
                guard->handleResponse(respCmd, respSeq, respData);
            }
        });
    return true;
}
