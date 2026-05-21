// =============================================================================
// @file    serialmanager.cpp
// @brief   串口管理器实现
//
// 实现串口通信的核心逻辑：端口管理、命令收发、帧解析分发、
// 自动重试和心跳保活机制。
// =============================================================================

#include "serialmanager.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QTimer>

Q_LOGGING_CATEGORY(lcSerialManager, "motordev.serial")

namespace {

/// @brief 拼接串口错误消息
QString makeSerialErrorMessage(const QString &prefix, const QString &detail) {
    if (detail.isEmpty()) {
        return prefix;
    }
    return QStringLiteral("%1: %2").arg(prefix, detail);
}

/// @brief 格式化字节为十六进制字符串
QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

/// @brief 格式化载荷为空格分隔的十六进制字符串
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}
} // namespace

// -----------------------------------------------------------------------------
// 构造与析构
// -----------------------------------------------------------------------------

SerialManager::SerialManager(QObject *parent)
    : QObject(nullptr) {
    Q_UNUSED(parent);

    // 注册自定义类型，使其可通过 QueuedConnection 跨线程传递
    qRegisterMetaType<QVector<int16_t>>("QVector<int16_t>");

    // 创建工作线程并将自身移入
    // 注意：QThread 不能有 parent，否则 moveToThread 会失败
    m_thread = new QThread();
    connect(m_thread, &QThread::started, this, &SerialManager::init);

    moveToThread(m_thread);
    m_thread->start();
}

SerialManager::~SerialManager() {
    if (m_thread == nullptr) {
        return;
    }

    if (m_thread->isRunning()) {
        // 在工作线程中同步关闭串口（BlockingQueuedConnection 保证执行完毕后返回）
        QMetaObject::invokeMethod(this, [this]() { closePortInternal(false); }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }

    delete m_thread;
    m_thread = nullptr;
}

// -----------------------------------------------------------------------------
// 静态方法
// -----------------------------------------------------------------------------

QStringList SerialManager::availablePorts() {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports.append(info.portName());
    }
    ports.sort();
    return ports;
}

// -----------------------------------------------------------------------------
// 初始化（在工作线程中执行）
// -----------------------------------------------------------------------------

void SerialManager::init() {
    // 创建串口对象（必须在工作线程中创建，保证线程亲和性）
    if (m_serial == nullptr) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
            onSerialErrorOccurred(static_cast<int>(error));
        });
    }

    // 创建重试定时器（单次触发）
    if (m_retryTimer == nullptr) {
        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        connect(m_retryTimer, &QTimer::timeout, this, &SerialManager::onRetryTimeout);
    }

    // 创建心跳定时器（周期触发）
    if (m_heartbeatTimer == nullptr) {
        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(HeartbeatIntervalMs);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &SerialManager::onHeartbeatTimeout);
    }
}

// -----------------------------------------------------------------------------
// 端口管理
// -----------------------------------------------------------------------------

void SerialManager::openPort(const QString &portName, qint32 baudRate) {
    if (m_serial == nullptr) {
        init();
    }

    // 如果当前已打开端口，先关闭
    if (m_serial->isOpen()) {
        closePortInternal(true);
    }

    // 配置串口参数：8 数据位、1 停止位、无校验、无流控
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    qCInfo(lcSerialManager).noquote() << QStringLiteral("Opening port %1 at %2 (8N1)...")
                              .arg(portName)
                              .arg(baudRate);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        const QString message = QStringLiteral("Failed to open %1: %2").arg(portName, m_serial->errorString());
        qCWarning(lcSerialManager).noquote() << message;
        emit errorOccurred(message);
        return;
    }

    // 重置解析器和命令状态
    m_parser.reset();
    clearPendingCommand();
    m_missedHeartbeats = 0;
    qCInfo(lcSerialManager).noquote() << QStringLiteral("Port %1 opened successfully").arg(portName);
    emit connected();
}

void SerialManager::closePort() {
    closePortInternal(true);
}

// -----------------------------------------------------------------------------
// 心跳保活
// -----------------------------------------------------------------------------

void SerialManager::startHeartbeat() {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        return;
    }

    m_missedHeartbeats = 0;
    if (m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->start();
    }
    qCInfo(lcSerialManager) << "Heartbeat started";
}

void SerialManager::stopHeartbeat() {
    stopHeartbeatTimer();
    m_missedHeartbeats = 0;
    qCInfo(lcSerialManager) << "Heartbeat stopped";
}

// -----------------------------------------------------------------------------
// 命令发送
// -----------------------------------------------------------------------------

void SerialManager::sendCommand(uint8_t cmd, const QByteArray &data) {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        emit errorOccurred(QStringLiteral("Serial port is not open"));
        return;
    }

    // 一次只能有一条挂起的命令（简单的命令队列由上层 CommandDispatcher 管理）
    if (!m_pendingFrame.isEmpty()) {
        emit errorOccurred(QStringLiteral("Another command is pending"));
        return;
    }

    // 分配序列号并编码帧
    const uint8_t seq = m_nextSeq++;
    m_pendingFrame = FrameParser::encodeControlFrame(seq, cmd, data);
    m_pendingSeq = seq;
    m_pendingCmd = cmd;
    m_retryCount = 0;

    qCDebug(lcSerialManager).noquote()
        << QStringLiteral("TX control frame: seq=%1 cmd=%2(%3) len=%4 payload=%5")
                              .arg(formatByte(seq))
                              .arg(formatByte(cmd))
                              .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                              .arg(data.size())
                              .arg(formatPayloadHex(data));

    const qint64 written = m_serial->write(m_pendingFrame);
    if (written != m_pendingFrame.size()) {
        qCWarning(lcSerialManager) << "Write failed: expected" << m_pendingFrame.size()
                                   << "bytes, wrote" << written;
        clearPendingCommand();
        emit errorOccurred(QStringLiteral("Failed to write command to serial port"));
        return;
    }
    emit commandSent(cmd, seq);

    // 启动重试定时器
    if (m_retryTimer != nullptr) {
        m_retryTimer->start(RetryTimeoutMs);
    }
}

// -----------------------------------------------------------------------------
// 同步发送 + 等待响应（烧录链路 fast-path）
// -----------------------------------------------------------------------------
//
// 在 SerialManager 工作线程内运行，**用嵌套 QEventLoop** 同步等待响应：
//   write → 设置 fastPath → loop.exec() → onReadyRead 正常分发 →
//   handleControlFrame 截获目标响应 → loop.quit()
// 整条路径不经过 dispatcher、不跨主线程，所有事件仍在 SerialManager 工作线程内
// 完成。专为消除 PC 端 Qt 跨线程 + 主线程拥堵给烧录链路带来的延迟而设计。
//
// 为什么不用 QSerialPort::waitForReadyRead：Qt 在 Windows 上的同步等待依赖
// pending overlapped read，同步阻塞模式下没有 pending read 时会**立即返回 false**，
// 实际无法等到数据到达。嵌套 event loop 让 QSerialPort 的 readyRead 信号正常
// 分发，是 Qt 推荐的同步等异步事件的标准做法。
//
// 边界处理：
// - 严格断言调用线程 = SerialManager 工作线程
// - 通过 m_fastPath 状态字段标记当前在 fast-path；handleControlFrame 据此分流
// - 心跳响应仍走原 handleControlFrame 路径（reset m_missedHeartbeats），
//   避免烧录期间因丢心跳被误判断开
// - cmd 匹配但 seq 不匹配的控制帧 / 流帧 → 直接丢弃（烧录前置已停采样/发生器）
// - 嵌套 event loop 期间会处理心跳定时器 / 其他 invokeMethod 投递；
//   dispatcher 在烧录期间无活动，不冲突
//
bool SerialManager::sendAndWaitResponse(uint8_t cmd,
                                         const QByteArray &data,
                                         uint8_t &outCmd,
                                         uint8_t &outSeq,
                                         QByteArray &outData,
                                         int timeoutMs) {
    // 诊断字段在入口立即重置，所有早退路径都会留下可读 exitReason
    m_lastDiag = FastPathDiag{};
    m_lastDiag.sameThread = (QThread::currentThread() == m_thread);
    m_lastDiag.serialOpen = (m_serial != nullptr && m_serial->isOpen());

    if (!m_lastDiag.sameThread) {
        m_lastDiag.exitReason = FastPathDiag::ExitReason::WrongThread;
        return false;
    }
    if (!m_lastDiag.serialOpen) {
        m_lastDiag.exitReason = FastPathDiag::ExitReason::NotOpen;
        return false;
    }
    // Housekeeping 命令的 in-flight pending（stopScope/stopGenerator/stopCyclic
    // 走 sendCommand 路径会占用 m_pendingFrame）必须为 fast-path 让路：
    // 烧录开始前 dispatcher 队列里的 stop 命令仍在等响应；如果在此早退，
    // I2C 帧永远写不出去。强制 clearPendingCommand() 后继续，丢失的 stop 响应
    // 由 fast-path 期间 handleControlFrame 作为 stray frame 丢弃。
    // 已知短板：dispatcher 端 m_waitingResponse 不会被通知，需要断连重连恢复。
    if (!m_pendingFrame.isEmpty()) {
        m_lastDiag.pendingCleared = true;
        clearPendingCommand();
    }
    if (m_fastPath.active) {
        m_lastDiag.exitReason = FastPathDiag::ExitReason::AlreadyActive;
        return false;
    }

    const uint8_t seq = m_nextSeq++;
    const QByteArray frame = FrameParser::encodeControlFrame(seq, cmd, data);

    // 1. 写串口 + 强制 flush（QSerialPort::write 仅 append 到 internal buffer，需要
    //    event loop 跑起来才异步 flush；嵌套 event loop 启动前必须先 flush 一次，
    //    否则数据滞留在 QSerialPort 缓冲区，STM32 端永远收不到帧）
    const qint64 written = m_serial->write(frame);
    m_lastDiag.wroteBytes = written;
    if (written != frame.size()) {
        m_lastDiag.exitReason = FastPathDiag::ExitReason::WriteFailed;
        return false;
    }
    m_lastDiag.flushOk = m_serial->flush();
    m_lastDiag.bytesToWriteAfterFlush = m_serial->bytesToWrite();

    // 2. 启动 fast-path 状态
    QEventLoop loop;
    m_fastPath.active = true;
    m_fastPath.expectedSeq = seq;
    m_fastPath.outCmd = 0;
    m_fastPath.outSeq = 0;
    m_fastPath.outData.clear();
    m_fastPath.gotMatch = false;
    m_fastPath.loop = &loop;

    // 3. 超时定时器 + 1s 探针 timer 验证 loop 在跑
    bool timerFired = false;
    QTimer::singleShot(timeoutMs, &loop, [&loop, &timerFired]() {
        timerFired = true;
        loop.quit();
    });
    QTimer::singleShot(1000, &loop, [](){
        qCInfo(lcSerialManager).noquote() << "[FP-DIAG] 1000ms probe timer fired (loop is alive)";
    });

    // 4. 嵌套 event loop（记录 loop.exec 真实耗时）
    QElapsedTimer execTimer;
    execTimer.start();
    loop.exec();
    m_lastDiag.execMs = execTimer.elapsed();
    m_lastDiag.timerFired = timerFired;
    m_lastDiag.gotMatch = m_fastPath.gotMatch;
    m_lastDiag.bytesAvailableAfter = m_serial ? m_serial->bytesAvailable() : -1;
    m_lastDiag.bytesToWriteAfter = m_serial ? m_serial->bytesToWrite() : -1;

    // 5. 收尾
    const bool matched = m_fastPath.gotMatch;
    if (matched) {
        outCmd = m_fastPath.outCmd;
        outSeq = m_fastPath.outSeq;
        outData = m_fastPath.outData;
        m_lastDiag.exitReason = FastPathDiag::ExitReason::CompletedMatch;
    } else {
        m_lastDiag.exitReason = FastPathDiag::ExitReason::CompletedTimeout;
    }
    m_fastPath.active = false;
    m_fastPath.loop = nullptr;
    m_fastPath.outData.clear();
    return matched;
}

// -----------------------------------------------------------------------------
// 数据接收与解析
// -----------------------------------------------------------------------------

void SerialManager::onReadyRead() {
    if (m_serial == nullptr) {
        return;
    }

    const QByteArray bytes = m_serial->readAll();
    // 逐字节喂入状态机解析器
    for (char byte : bytes) {
        const FrameParser::FrameType frameType = m_parser.feedByte(static_cast<uint8_t>(byte));
        if (frameType == FrameParser::FrameType::Control) {
            handleControlFrame(m_parser.controlFrame());
        } else if (frameType == FrameParser::FrameType::Stream) {
            // 流帧直接转发给上层（示波器数据）
            const StreamFrame &frame = m_parser.streamFrame();
            emit streamDataReceived(frame.channelMask, frame.samples);
        }
    }
}

// -----------------------------------------------------------------------------
// 重试机制
// -----------------------------------------------------------------------------

void SerialManager::onRetryTimeout() {
    if (m_serial == nullptr || !m_serial->isOpen() || m_pendingFrame.isEmpty()) {
        return;
    }

    // 超过最大重试次数，放弃该命令
    if (m_retryCount >= MaxRetries) {
        qCWarning(lcSerialManager).noquote()
            << QStringLiteral("Command timeout: seq=%1 cmd=%2(%3) after %4 attempts")
                                    .arg(formatByte(m_pendingSeq))
                                    .arg(formatByte(m_pendingCmd))
                                    .arg(QString::fromLatin1(MotorProtocol::commandName(m_pendingCmd)))
                                    .arg(MaxRetries + 1);
        clearPendingCommand();
        emit errorOccurred(QStringLiteral("Command timeout"));
        return;
    }

    // 重发命令
    ++m_retryCount;
    qCWarning(lcSerialManager).noquote()
        << QStringLiteral("Retry #%1 for seq=%2 cmd=%3(%4)")
                                .arg(m_retryCount)
                                .arg(formatByte(m_pendingSeq))
                                .arg(formatByte(m_pendingCmd))
                                .arg(QString::fromLatin1(MotorProtocol::commandName(m_pendingCmd)));
    m_serial->write(m_pendingFrame);
    if (m_retryTimer != nullptr) {
        m_retryTimer->start(RetryTimeoutMs);
    }
}

// -----------------------------------------------------------------------------
// 心跳超时处理
// -----------------------------------------------------------------------------

void SerialManager::onHeartbeatTimeout() {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        return;
    }

    // 心跳独立于命令队列发送，不占用 pendingCommand 槽位。
    // handleControlFrame() 会按 cmd==0x00 匹配心跳响应，不会与普通命令响应冲突。
    const QByteArray heartbeatFrame = FrameParser::encodeControlFrame(m_nextSeq++, MotorProtocol::CmdHeartbeat, {});
    m_serial->write(heartbeatFrame);

    ++m_missedHeartbeats;
    qCDebug(lcSerialManager).noquote() << QStringLiteral("Heartbeat sent (missed: %1)").arg(m_missedHeartbeats);

    // 连续丢失心跳超过阈值，判定连接断开
    if (m_missedHeartbeats >= MaxMissedHeartbeats) {
        qCWarning(lcSerialManager).noquote() << QStringLiteral("Heartbeat lost: %1 missed, disconnecting")
                                    .arg(m_missedHeartbeats);
        closePortInternal(true);
    }
}

// -----------------------------------------------------------------------------
// 串口错误处理
// -----------------------------------------------------------------------------

void SerialManager::onSerialErrorOccurred(int error) {
    const auto serialError = static_cast<QSerialPort::SerialPortError>(error);
    if (serialError == QSerialPort::NoError || m_serial == nullptr) {
        return;
    }

    const QString message = makeSerialErrorMessage(QStringLiteral("Serial port error"), m_serial->errorString());
    qCWarning(lcSerialManager).noquote() << message;
    emit errorOccurred(message);

    // 致命错误自动断开连接
    switch (serialError) {
    case QSerialPort::ResourceError:       // 设备被拔出
    case QSerialPort::PermissionError:     // 权限丢失
    case QSerialPort::DeviceNotFoundError: // 设备不存在
        closePortInternal(true);
        break;
    default:
        break;
    }
}

// -----------------------------------------------------------------------------
// 内部辅助方法
// -----------------------------------------------------------------------------

void SerialManager::clearPendingCommand() {
    stopRetryTimer();
    m_pendingFrame.clear();
    m_pendingSeq = 0;
    m_pendingCmd = 0;
    m_retryCount = 0;
}

void SerialManager::stopRetryTimer() {
    if (m_retryTimer != nullptr) {
        m_retryTimer->stop();
    }
}

void SerialManager::stopHeartbeatTimer() {
    if (m_heartbeatTimer != nullptr) {
        m_heartbeatTimer->stop();
    }
}

/// @brief 处理解析完成的控制帧
///
/// 分发逻辑：
/// 1. 心跳响应（cmd==0x00）→ 重置心跳计数器
/// 2. 错误响应（cmd==0xFF）→ 清除挂起命令并转发
/// 3. 无挂起命令 → 作为设备主动上报帧转发
/// 4. seq 匹配 → 清除挂起命令并转发响应
/// 5. seq 不匹配 → 仍然转发（可能是设备主动上报帧，如调试信息）
void SerialManager::handleControlFrame(const ControlFrame &frame) {
    // 心跳响应：无论是否在 fast-path 都正常处理（重置丢失计数器，避免烧录期间被误判断开）
    if (frame.cmd == MotorProtocol::CmdHeartbeat) {
        qCDebug(lcSerialManager) << "Heartbeat response received";
        m_missedHeartbeats = 0;
        return;
    }

    // 烧录链路 fast-path：sendAndWaitResponse 正在嵌套 event loop 中等响应。
    // 错误响应 seq=0xFF（CRC 失败）无条件归属当前调用；其他响应按 seq 匹配。
    // 不匹配的控制帧直接丢弃（烧录期间无其他业务命令在飞）。
    if (m_fastPath.active) {
        const bool match = (frame.cmd == MotorProtocol::CmdErrorResponse) ||
                           (frame.seq == m_fastPath.expectedSeq);
        if (match) {
            m_fastPath.outCmd = frame.cmd;
            m_fastPath.outSeq = frame.seq;
            m_fastPath.outData = frame.data;
            m_fastPath.gotMatch = true;
            if (m_fastPath.loop != nullptr) {
                m_fastPath.loop->quit();
            }
        }
        return;
    }

    qCDebug(lcSerialManager).noquote()
        << QStringLiteral("RX control frame: seq=%1 cmd=%2(%3) len=%4 payload=%5")
                              .arg(formatByte(frame.seq))
                              .arg(formatByte(frame.cmd))
                              .arg(QString::fromLatin1(MotorProtocol::commandName(frame.cmd)))
                              .arg(frame.data.size())
                              .arg(formatPayloadHex(frame.data));

    // 错误响应：不论 seq 是否匹配都清除挂起状态
    if (frame.cmd == MotorProtocol::CmdErrorResponse) {
        clearPendingCommand();
        emit frameReceived(frame.cmd, frame.seq, frame.data);
        return;
    }

    // 无挂起命令：视为设备主动上报帧
    if (m_pendingFrame.isEmpty()) {
        emit frameReceived(frame.cmd, frame.seq, frame.data);
        return;
    }

    // seq 匹配：命令响应成功
    if (frame.seq == m_pendingSeq) {
        clearPendingCommand();
        emit frameReceived(frame.cmd, frame.seq, frame.data);
    } else {
        // seq 不匹配：可能是设备主动上报帧（如 0x06 调试信息），不丢弃
        emit frameReceived(frame.cmd, frame.seq, frame.data);
    }
}

/// @brief 内部关闭串口
///
/// 停止所有定时器、清除挂起命令、重置解析器。
/// 仅在 wasOpen 且 emitDisconnectedSignal 为 true 时发出 disconnected 信号。
void SerialManager::closePortInternal(bool emitDisconnectedSignal) {
    stopHeartbeatTimer();
    clearPendingCommand();
    m_parser.reset();
    m_missedHeartbeats = 0;

    const bool wasOpen = (m_serial != nullptr && m_serial->isOpen());
    if (wasOpen) {
        m_serial->close();
        qCInfo(lcSerialManager) << "Port closed";
    }

    if (emitDisconnectedSignal && wasOpen) {
        emit disconnected();
    }
}
