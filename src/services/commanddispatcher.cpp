// =============================================================================
// @file    commanddispatcher.cpp
// @brief   命令分发器实现
//
// 实现优先级命令队列、响应匹配、错误处理和命令生命周期管理。
// =============================================================================

#include "services/commanddispatcher.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QMetaObject>

namespace {
constexpr uint8_t kInvalidSeq = 0xFF;  ///< 无效序列号标记
}

// -----------------------------------------------------------------------------
// 构造
// -----------------------------------------------------------------------------

CommandDispatcher::CommandDispatcher(SerialManager *serialManager, QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager) {
    if (m_serialManager == nullptr) {
        return;
    }

    // 连接 SerialManager 的信号（跨线程时 Qt 自动选择 QueuedConnection）
    connect(m_serialManager, &SerialManager::frameReceived,
            this, &CommandDispatcher::onFrameReceived);
    connect(m_serialManager, &SerialManager::commandSent,
            this, &CommandDispatcher::onCommandSent);
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &CommandDispatcher::onSerialError);
    connect(m_serialManager, &SerialManager::disconnected,
            this, &CommandDispatcher::onSerialDisconnected);
}

// -----------------------------------------------------------------------------
// 命令提交与取消
// -----------------------------------------------------------------------------

/// @brief 提交命令到优先级队列
///
/// 按优先级插入队列（优先级数值越小排越前）。
/// 如果当前无活跃命令，立即尝试发送队头命令。
CommandDispatcher::Ticket CommandDispatcher::submitCommand(uint8_t cmd,
                                                           const QByteArray &data,
                                                           Priority priority,
                                                           ResponseCallback onResponse) {
    // 无串口管理器时立即以本地错误回调
    if (m_serialManager == nullptr) {
        if (onResponse) {
            onResponse(MotorProtocol::CmdErrorResponse, 0, localErrorPayload());
        }
        return InvalidTicket;
    }

    PendingEntry entry;
    entry.ticket = m_nextTicket++;
    entry.cmd = cmd;
    entry.data = data;
    entry.priority = priority;
    entry.onResponse = std::move(onResponse);

    // 按优先级寻找插入位置
    int insertIndex = m_queue.size();
    for (int i = 0; i < m_queue.size(); ++i) {
        if (priority < m_queue.at(i).priority) {
            insertIndex = i;
            break;
        }
    }
    m_queue.insert(insertIndex, entry);
    trySendNext();
    return entry.ticket;
}

/// @brief 取消队列中尚未发送的命令
///
/// 已经在发送中（活跃命令）的无法取消。
bool CommandDispatcher::cancelCommand(Ticket ticket) {
    if (ticket == InvalidTicket || ticket == m_activeTicket) {
        return false;  // 无效票据或正在执行中的命令不可取消
    }

    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).ticket != ticket) {
            continue;
        }
        m_queue.removeAt(i);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// 响应处理
// -----------------------------------------------------------------------------

/// @brief 处理收到的控制帧
///
/// 匹配策略：
/// 1. 无活跃命令 → 视为设备主动上报帧
/// 2. cmd == ErrorResponse → 无条件匹配活跃命令
/// 3. seq 匹配 → 确认响应
/// 4. seq 未知但 cmd 匹配 → 兼容 commandSent 信号延迟到达的场景
/// 5. 以上都不匹配 → 视为设备主动上报帧
void CommandDispatcher::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (!m_waitingResponse) {
        emit unsolicitedFrameReceived(cmd, seq, data);
        return;
    }

    // 错误响应：无论 seq 是否匹配，都归属于当前活跃命令
    if (cmd == MotorProtocol::CmdErrorResponse) {
        const ResponseCallback callback = m_activeCallback;
        clearActive();
        if (callback) {
            callback(cmd, seq, data);
        }
        trySendNext();
        return;
    }

    // 尝试按 seq 或 cmd 匹配
    const bool matchesBySeq = (m_activeSeq != kInvalidSeq && seq == m_activeSeq);
    const bool matchesBeforeSeqKnown = (m_activeSeq == kInvalidSeq && cmd == m_activeCmd);
    if (!matchesBySeq && !matchesBeforeSeqKnown) {
        emit unsolicitedFrameReceived(cmd, seq, data);
        return;
    }

    // 匹配成功：回调并继续队列
    const ResponseCallback callback = m_activeCallback;
    clearActive();
    if (callback) {
        callback(cmd, seq, data);
    }
    trySendNext();
}

/// @brief 记录活跃命令的 seq（由 SerialManager 在实际写入串口后回调）
void CommandDispatcher::onCommandSent(uint8_t cmd, uint8_t seq) {
    if (!m_waitingResponse || cmd != m_activeCmd || m_activeTicket == InvalidTicket) {
        return;
    }

    m_activeSeq = seq;
    emit commandDispatched(m_activeTicket, cmd, seq);
}

// -----------------------------------------------------------------------------
// 错误处理
// -----------------------------------------------------------------------------

/// @brief 串口错误处理
///
/// 致命错误（端口未打开、打开失败等）→ 失败活跃命令 + 清空队列。
/// 非致命错误（超时等）→ 仅失败活跃命令，继续发送队列中的下一条。
void CommandDispatcher::onSerialError(const QString &message) {
    if (!m_waitingResponse && m_queue.isEmpty()) {
        return;
    }

    if (isSerialLevelError(message)) {
        failActive(message);
        failQueued(message);
        return;
    }

    failActive(message);
    trySendNext();
}

/// @brief 串口断开：失败所有命令
void CommandDispatcher::onSerialDisconnected() {
    failActive(QStringLiteral("Serial port disconnected"));
    failQueued(QStringLiteral("Serial port disconnected"));
}

// -----------------------------------------------------------------------------
// 内部辅助方法
// -----------------------------------------------------------------------------

QByteArray CommandDispatcher::localErrorPayload() {
    return QByteArray(1, static_cast<char>(LocalErrorCode));
}

/// @brief 判断是否为串口级别致命错误（需要清空整个队列）
bool CommandDispatcher::isSerialLevelError(const QString &message) {
    return message == QStringLiteral("Serial port is not open")
        || message.startsWith(QStringLiteral("Failed to open"))
        || message.startsWith(QStringLiteral("Serial port error"));
}

/// @brief 从队列取出下一条命令并通过 QueuedConnection 发送到 SerialManager
void CommandDispatcher::trySendNext() {
    if (m_waitingResponse || m_queue.isEmpty() || m_serialManager == nullptr) {
        return;
    }

    const PendingEntry entry = m_queue.takeFirst();
    m_waitingResponse = true;
    m_activeTicket = entry.ticket;
    m_activeSeq = kInvalidSeq;  // seq 尚未分配，等待 commandSent 回调
    m_activeCmd = entry.cmd;
    m_activeCallback = entry.onResponse;

    // 通过 QueuedConnection 调用工作线程中的 SerialManager::sendCommand
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection,
        Q_ARG(uint8_t, entry.cmd),
        Q_ARG(QByteArray, entry.data));
}

void CommandDispatcher::clearActive() {
    m_waitingResponse = false;
    m_activeTicket = InvalidTicket;
    m_activeSeq = kInvalidSeq;
    m_activeCmd = 0;
    m_activeCallback = ResponseCallback();
}

/// @brief 以错误回调通知活跃命令失败
void CommandDispatcher::failActive(const QString &message) {
    Q_UNUSED(message);
    if (!m_waitingResponse) {
        return;
    }

    const ResponseCallback callback = m_activeCallback;
    clearActive();
    if (callback) {
        callback(MotorProtocol::CmdErrorResponse, 0, localErrorPayload());
    }
}

/// @brief 以错误回调通知队列中所有命令失败并清空队列
void CommandDispatcher::failQueued(const QString &message) {
    Q_UNUSED(message);
    const QVector<PendingEntry> queued = m_queue;
    m_queue.clear();
    for (const PendingEntry &entry : queued) {
        if (entry.onResponse) {
            entry.onResponse(MotorProtocol::CmdErrorResponse, 0, localErrorPayload());
        }
    }
}
