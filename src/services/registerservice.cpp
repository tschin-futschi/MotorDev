// =============================================================================
// @file    registerservice.cpp
// @brief   寄存器读写服务实现 — 队列调度、命令提交、响应处理、超时管理
//
// 工作流程：
//   1. readSingleRow / writeSingleRow → 若当前无待处理请求，直接提交；否则入队
//   2. readBatch / writeBatch → 清空旧队列，批量入队后开始处理
//   3. submitCurrentRequest → 编码 payload，通过 CommandDispatcher 提交
//   4. handleResponse → 解析 ACK / 错误响应，发射结果信号
//   5. processNextInQueue → 从队列取下一条请求，循环直至队列清空
//   6. onTimeout → 500 ms 无响应后超时，发射错误信号并继续处理队列
//
// 会话 ID（m_sessionId）用于防止旧请求的延迟响应干扰新请求。
// =============================================================================
#include "services/registerservice.h"

#include "protocol/motor_protocol.h"
#include "services/commanddispatcher.h"

#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>

Q_LOGGING_CATEGORY(lcRegister, "motordev.register")

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

/// @brief 构造寄存器服务，初始化超时定时器并监听命令分发事件。
///
/// @param dispatcher  命令分发器指针，用于提交读写命令
/// @param parent      父对象
RegisterService::RegisterService(CommandDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_dispatcher(dispatcher) {
    // ---------- 超时定时器：500 ms 无响应则判定超时 ----------
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RegisterService::onTimeout);

    // 监听命令实际发出事件，在命令被分发时才启动超时计时
    if (m_dispatcher != nullptr) {
        connect(m_dispatcher, &CommandDispatcher::commandDispatched,
                this, &RegisterService::onCommandDispatched);
    }
}

RegisterService::~RegisterService() = default;

// =============================================================================
// 单行读写接口
// =============================================================================

/// @brief 读取单个寄存器。若当前有待处理请求，则入队等待。
///
/// @param globalRow  全局行号（用于 UI 反馈定位）
/// @param addr       16 位寄存器地址
void RegisterService::readSingleRow(int globalRow, quint16 addr) {
    RowRequest req;
    req.globalRow = globalRow;
    req.addr = addr;
    req.isWrite = false;

    if (m_hasPending) {
        m_pendingQueue.prepend(req);                    // 插入队首，优先处理
        emit rowPending(globalRow);
        return;
    }

    // 无待处理请求，直接提交
    m_isWriteOp = false;
    m_priority = CommandDispatcher::Normal;
    m_currentRequest = req;
    m_hasPending = true;
    emit rowPending(globalRow);
    submitCurrentRequest();
}

/// @brief 写入单个寄存器。若当前有待处理请求，则入队等待。
///
/// @param globalRow  全局行号
/// @param addr       16 位寄存器地址
/// @param value      16 位写入值
/// @param priority   命令优先级（循环写入时使用 Low）
void RegisterService::writeSingleRow(int globalRow, quint16 addr, qint16 value,
                                     CommandDispatcher::Priority priority) {
    RowRequest req;
    req.globalRow = globalRow;
    req.addr = addr;
    req.value = value;
    req.isWrite = true;

    if (m_hasPending) {
        m_pendingQueue.prepend(req);
        return;
    }

    m_isWriteOp = true;
    m_priority = priority;
    m_currentRequest = req;
    m_hasPending = true;
    submitCurrentRequest();
}

// =============================================================================
// 批量读写接口
// =============================================================================

/// @brief 批量读取寄存器，清空旧队列后按顺序入队处理。
void RegisterService::readBatch(const QVector<RowRequest> &rows) {
    enqueueAndStart(rows, false, CommandDispatcher::Normal);
}

/// @brief 批量写入寄存器，清空旧队列后按顺序入队处理。
void RegisterService::writeBatch(const QVector<RowRequest> &rows) {
    enqueueAndStart(rows, true, CommandDispatcher::Normal);
}

/// @brief 批量入队的内部实现：清空旧队列 → 入队 → 启动首条处理。
void RegisterService::enqueueAndStart(const QVector<RowRequest> &rows, bool isWrite,
                                      CommandDispatcher::Priority priority) {
    clearQueue();                                       // 清空旧的请求和计时器
    m_priority = priority;
    for (RowRequest r : rows) {
        r.isWrite = isWrite;
        m_pendingQueue.enqueue(r);
    }
    processNextInQueue();
}

// =============================================================================
// 命令提交
// =============================================================================

/// @brief 将当前请求编码为协议帧并通过 CommandDispatcher 提交。
///
/// 使用 QPointer + sessionId 双重保护：
/// - QPointer 防止对象被析构后回调崩溃
/// - sessionId 防止旧响应干扰新请求
void RegisterService::submitCurrentRequest() {
    if (m_dispatcher == nullptr) {
        // 无分发器，直接报错
        if (m_isWriteOp) {
            emit rowWriteError(m_currentRequest.globalRow);
        } else {
            emit rowReadError(m_currentRequest.globalRow);
        }
        processNextInQueue();
        return;
    }

    // ---------- 编码协议帧 ----------
    const uint8_t cmd = m_isWriteOp ? MotorProtocol::CmdWriteRegister : MotorProtocol::CmdReadRegister;
    const QByteArray payload = m_isWriteOp
        ? MotorProtocol::encodeWriteRegister(m_currentRequest.addr, m_currentRequest.value)
        : MotorProtocol::encodeReadRegister(m_currentRequest.addr);

    // ---------- 日志记录 ----------
    if (m_isWriteOp) {
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 TX row=%2 addr=0x%3 value=0x%4 payload=%5")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(m_currentRequest.globalRow)
                   .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
                   .arg(static_cast<quint16>(m_currentRequest.value), 4, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(payload))
                   .toUpper();
    } else {
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 TX row=%2 addr=0x%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(m_currentRequest.globalRow)
                   .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(payload))
                   .toUpper();
    }

    // ---------- 提交命令，捕获 sessionId 用于响应校验 ----------
    const uint32_t sessionId = m_sessionId;
    QPointer<RegisterService> guard(this);
    m_activeTicket = m_dispatcher->submitCommand(
        cmd, payload, m_priority,
        [guard, sessionId](uint8_t responseCmd, uint8_t responseSeq, const QByteArray &responseData) {
            if (guard == nullptr) {
                return;
            }
            guard->handleResponse(responseCmd, responseSeq, responseData, sessionId);
        });
}

// =============================================================================
// 响应处理
// =============================================================================

/// @brief 处理命令响应：解析 ACK、读取值或错误码。
///
/// @param cmd        响应命令字
/// @param seq        序列号（未使用）
/// @param data       响应 payload
/// @param sessionId  发送时的会话 ID，用于过滤过期响应
void RegisterService::handleResponse(uint8_t cmd, uint8_t seq, const QByteArray &data, uint32_t sessionId) {
    Q_UNUSED(seq);
    // 过滤过期响应：sessionId 不匹配说明已被 clearQueue 或超时重置
    if (!m_hasPending || sessionId != m_sessionId) {
        return;
    }

    m_timeoutTimer->stop();
    m_activeTicket = 0;

    const int row = m_currentRequest.globalRow;

    // ---------- 读取响应 ----------
    if (cmd == MotorProtocol::CmdReadRegister && !m_isWriteOp) {
        qint16 value = 0;
        if (MotorProtocol::decodeReadRegisterResponse(data, &value)) {
            qCInfo(lcRegister).noquote()
                << QStringLiteral("%1 RX ACK row=%2 value=0x%3 payload=%4")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(row)
                       .arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0'))
                       .arg(formatPayloadHex(data))
                       .toUpper();
            emit rowReadOk(row, value);
        } else {
            qCWarning(lcRegister).noquote()
                << QStringLiteral("%1 RX decode failed row=%2 payload=%3")
                       .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                       .arg(row)
                       .arg(formatPayloadHex(data));
            emit rowReadError(row);
        }
        processNextInQueue();
        return;
    }

    // ---------- 写入 ACK ----------
    if (cmd == MotorProtocol::CmdWriteRegister && m_isWriteOp) {
        qCInfo(lcRegister).noquote()
            << QStringLiteral("%1 RX ACK row=%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(row)
                   .arg(formatPayloadHex(data));
        emit rowWriteOk(row);
        processNextInQueue();
        return;
    }

    // ---------- 错误响应 ----------
    if (cmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
        qCWarning(lcRegister).noquote()
            << QStringLiteral("%1 RX row=%2 errorCode=0x%3 payload=%4")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(row)
                   .arg(errorCode, 2, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(data))
                   .toUpper();
        if (m_isWriteOp) {
            emit rowWriteError(row);
        } else {
            emit rowReadError(row);
        }
        processNextInQueue();
    }
}

// =============================================================================
// 队列管理
// =============================================================================

/// @brief 从队列中取出下一条请求并提交；队列为空时发射 queueFinished 信号。
void RegisterService::processNextInQueue() {
    if (m_dispatcher == nullptr) {
        clearQueue();
        return;
    }
    if (m_pendingQueue.isEmpty()) {
        m_timeoutTimer->stop();
        qCDebug(lcRegister).noquote()
            << QStringLiteral("Queue done (%1)")
                   .arg(m_isWriteOp ? QStringLiteral("write") : QStringLiteral("read"));
        m_hasPending = false;
        emit queueFinished(m_isWriteOp);
        return;
    }

    // 出队下一条请求
    m_currentRequest = m_pendingQueue.dequeue();
    m_hasPending = true;
    m_isWriteOp = m_currentRequest.isWrite;
    if (!m_isWriteOp) {
        emit rowPending(m_currentRequest.globalRow);    // 通知 UI 显示"读取中"状态
    }
    submitCurrentRequest();
}

// =============================================================================
// 超时与分发事件
// =============================================================================

/// @brief 命令被实际分发时启动超时计时器（500 ms）。
///
/// 仅当分发的 ticket 与当前活跃 ticket 匹配时才启动，
/// 避免其他服务的命令误触发计时。
void RegisterService::onCommandDispatched(uint32_t ticket, uint8_t cmd, uint8_t seq) {
    Q_UNUSED(cmd);
    Q_UNUSED(seq);
    if (ticket == m_activeTicket) {
        m_timeoutTimer->start(500);
    }
}

/// @brief 超时回调：报告当前请求失败，递增 sessionId 使旧响应失效，继续处理队列。
void RegisterService::onTimeout() {
    if (!m_hasPending) {
        return;
    }

    const int row = m_currentRequest.globalRow;
    qCWarning(lcRegister).noquote()
        << QStringLiteral("Timeout waiting for %1 row=%2 addr=0x%3")
               .arg(QString::fromLatin1(MotorProtocol::commandName(
                   m_isWriteOp ? MotorProtocol::CmdWriteRegister : MotorProtocol::CmdReadRegister)))
               .arg(row)
               .arg(m_currentRequest.addr, 4, 16, QLatin1Char('0'))
               .toUpper();
    if (m_isWriteOp) {
        emit rowWriteError(row);
    } else {
        emit rowReadError(row);
    }

    ++m_sessionId;                                      // 使旧响应回调失效
    m_activeTicket = 0;
    processNextInQueue();
}

/// @brief 清空队列和所有运行时状态，递增 sessionId 使旧回调全部失效。
void RegisterService::clearQueue() {
    m_timeoutTimer->stop();
    ++m_sessionId;
    m_activeTicket = 0;
    m_hasPending = false;
    m_pendingQueue.clear();
}
