// =============================================================================
// @file    generatorservice.cpp
// @brief   信号发生器服务实现 — Linear/Cosine 命令编码、响应处理、超时管理
//
// GeneratorService 封装了波形发生器的三个操作：
//   - startLinear()  — 线性扫描（锯齿波），下发地址/范围/步长/间隔
//   - startCosine()  — 余弦波发生，下发振幅/偏移/频率 + 最多 3 通道（地址+相位）
//   - stop()         — 停止发生器
//
// 每个操作都通过 CommandDispatcher 异步提交命令，3 秒内无 ACK 则超时。
// 使用 PendingOp 状态机跟踪当前待确认的操作类型。
// =============================================================================
#include "services/generatorservice.h"

#include "protocol/motor_protocol.h"
#include "services/commanddispatcher.h"

#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>
#include <QtMath>

Q_LOGGING_CATEGORY(lcGenerator, "motordev.generator")

namespace {
/// @brief 将字节数组格式化为大写十六进制字符串，用于日志。
QString formatPayloadHex(const QByteArray &data) {
    return data.isEmpty() ? QStringLiteral("<empty>")
                          : QString::fromLatin1(data.toHex(' ')).toUpper();
}

/// @brief 将 16 位值格式化为 "0x0000" 形式。
QString formatWord(quint16 value) {
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}
} // namespace

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造发生器服务，初始化 3 秒 ACK 超时定时器。
GeneratorService::GeneratorService(CommandDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_dispatcher(dispatcher) {
    m_ackTimeoutTimer = new QTimer(this);
    m_ackTimeoutTimer->setSingleShot(true);
    m_ackTimeoutTimer->setInterval(3000);               // 3 秒超时
    connect(m_ackTimeoutTimer, &QTimer::timeout, this, &GeneratorService::handleTimeout);
}

GeneratorService::~GeneratorService() = default;

// =============================================================================
// 状态查询
// =============================================================================

/// @brief 返回发生器是否正在运行（模式不为 None）。
bool GeneratorService::isRunning() const {
    return m_mode != Mode::None;
}

/// @brief 返回当前模式的文本标签（"Linear" / "Cosine" / 空）。
QString GeneratorService::modeLabel() const {
    switch (m_mode) {
    case Mode::Linear:
        return QStringLiteral("Linear");
    case Mode::Cosine:
        return QStringLiteral("Cosine");
    case Mode::Sawtooth:
        return QStringLiteral("Sawtooth");
    case Mode::None:
        return QString();
    }
    return QString();
}

/// @brief 返回余弦模式下的通道数（1~3），非余弦模式返回 0。
int GeneratorService::cosineChannelCount() const {
    return m_cosineChannelCount;
}

// =============================================================================
// 启动线性扫描
// =============================================================================

/// @brief 下发线性扫描启动命令。
///
/// @param addr        目标寄存器地址
/// @param min         扫描最小值
/// @param max         扫描最大值
/// @param step        步长
/// @param intervalMs  每步间隔（毫秒），限制在 [1, 65535]
void GeneratorService::startLinear(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs) {
    if (m_dispatcher == nullptr) {
        return;
    }

    const quint16 interval = static_cast<quint16>(qBound(1, intervalMs, 65535));
    const QByteArray payload = MotorProtocol::encodeStartLinearGen(addr, min, max, step, interval);
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX addr=%2 min=%3 max=%4 step=%5 intervalMs=%6 payload=%7")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStartLinearGen)))
               .arg(formatWord(addr))
               .arg(min)
               .arg(max)
               .arg(step)
               .arg(interval)
               .arg(formatPayloadHex(payload));

    // 记录待确认操作并启动超时
    m_pendingOp = PendingOp::StartLinear;
    m_ackTimeoutTimer->start();

    // 提交命令，使用 QPointer 防止回调时对象已析构
    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStartLinearGen, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

// =============================================================================
// 启动余弦波
// =============================================================================

/// @brief 下发余弦波启动命令。
///
/// @param amplitude    振幅（int16）
/// @param offset       偏移量（int16）
/// @param frequencyHz  频率（Hz），协议传输时 ×100 转为 uint16
/// @param channels     通道列表（最多 3 个），包含地址和相位角
void GeneratorService::startCosine(qint16 amplitude, qint16 offset, double frequencyHz,
                                   const QVector<ScopeGeneratorCosineChannel> &channels) {
    if (m_dispatcher == nullptr || channels.isEmpty()) {
        return;
    }

    // 限制通道数为 1~3
    const int boundedCount = qBound(1, channels.size(), 3);
    const uint8_t channelCount = static_cast<uint8_t>(boundedCount);
    // 频率 ×100 转为整数传输，限制在 [1, 65535]
    const quint16 freqX100 = static_cast<quint16>(qBound(1.0, frequencyHz * 100.0, 65535.0));

    // 构建通道参数对：(地址, 相位×10)
    QVector<QPair<quint16, qint16>> channelPairs;
    channelPairs.reserve(boundedCount);
    for (int i = 0; i < boundedCount; ++i) {
        const ScopeGeneratorCosineChannel &channel = channels.at(i);
        // 相位角 ×10 转为整数传输，限制在 [-3200, 3200]（即 ±320°）
        const qint16 phaseX10 = static_cast<qint16>(qBound(-3200.0, channel.phaseDeg * 10.0, 3200.0));
        channelPairs.push_back(qMakePair(channel.addr, phaseX10));
    }

    m_cosineChannelCount = boundedCount;
    const QByteArray payload = MotorProtocol::encodeStartCosineGen(
        amplitude, offset, freqX100, channelCount, channelPairs);
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX amplitude=%2 offset=%3 freqX100=%4 channels=%5 payload=%6")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStartCosineGen)))
               .arg(amplitude)
               .arg(offset)
               .arg(freqX100)
               .arg(boundedCount)
               .arg(formatPayloadHex(payload));

    m_pendingOp = PendingOp::StartCosine;
    m_ackTimeoutTimer->start();
    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStartCosineGen, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

// =============================================================================
// 启动锯齿波测试
// =============================================================================

/// @brief 下发锯齿波测试发生器启动命令。
///
/// @param addr  目标寄存器地址
/// @param min   最小值
/// @param max   最大值
/// @param step  步长
void GeneratorService::startSawtooth(quint16 addr, qint16 min, qint16 max, qint16 step) {
    if (m_dispatcher == nullptr) {
        return;
    }

    const QByteArray payload = MotorProtocol::encodeStartSawtoothGen(addr, min, max, step);
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX addr=%2 min=%3 max=%4 step=%5 payload=%6")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStartSawtoothGen)))
               .arg(formatWord(addr))
               .arg(min)
               .arg(max)
               .arg(step)
               .arg(formatPayloadHex(payload));

    m_pendingOp = PendingOp::StartSawtooth;
    m_ackTimeoutTimer->start();

    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStartSawtoothGen, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

// =============================================================================
// 停止发生器
// =============================================================================

/// @brief 下发停止发生器命令。
void GeneratorService::stop() {
    if (m_dispatcher == nullptr) {
        return;
    }

    const QByteArray payload = MotorProtocol::encodeStopGenerator();
    qCInfo(lcGenerator).noquote()
        << QStringLiteral("%1 TX payload=%2")
               .arg(QString::fromLatin1(MotorProtocol::commandName(MotorProtocol::CmdStopGenerator)))
               .arg(formatPayloadHex(payload));

    m_pendingOp = PendingOp::Stop;
    m_ackTimeoutTimer->start();
    QPointer<GeneratorService> guard(this);
    m_dispatcher->submitCommand(MotorProtocol::CmdStopGenerator, payload, CommandDispatcher::High,
        [guard](uint8_t cmd, uint8_t seq, const QByteArray &data) {
            if (guard != nullptr) {
                guard->onResponse(cmd, seq, data);
            }
        });
}

// =============================================================================
// 响应处理
// =============================================================================

/// @brief 统一处理所有发生器命令的响应。
///
/// 根据响应 cmd 判断是哪个操作的 ACK：
/// - CmdStartLinearGen ACK → 进入 Linear 模式
/// - CmdStartCosineGen ACK → 进入 Cosine 模式
/// - CmdStopGenerator ACK  → 退出运行模式
/// - CmdErrorResponse      → 根据 pendingOp 发射对应失败信号
void GeneratorService::onResponse(uint8_t cmd, uint8_t /*seq*/, const QByteArray &data) {
    // ---------- 线性启动 ACK ----------
    if (cmd == MotorProtocol::CmdStartLinearGen && data.isEmpty()) {
        m_ackTimeoutTimer->stop();
        m_pendingOp = PendingOp::None;
        m_mode = Mode::Linear;
        m_cosineChannelCount = 0;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK mode=Linear")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)));
        emit runningChanged(true);
        return;
    }

    // ---------- 余弦启动 ACK ----------
    if (cmd == MotorProtocol::CmdStartCosineGen && data.isEmpty()) {
        m_ackTimeoutTimer->stop();
        m_pendingOp = PendingOp::None;
        m_mode = Mode::Cosine;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK mode=Cosine channels=%2")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(m_cosineChannelCount);
        emit runningChanged(true);
        return;
    }

    // ---------- 锯齿波启动 ACK ----------
    if (cmd == MotorProtocol::CmdStartSawtoothGen && data.isEmpty()) {
        m_ackTimeoutTimer->stop();
        m_pendingOp = PendingOp::None;
        m_mode = Mode::Sawtooth;
        m_cosineChannelCount = 0;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK mode=Sawtooth")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)));
        emit runningChanged(true);
        return;
    }

    // ---------- 停止 ACK ----------
    if (cmd == MotorProtocol::CmdStopGenerator && data.isEmpty()) {
        m_ackTimeoutTimer->stop();
        m_pendingOp = PendingOp::None;
        m_mode = Mode::None;
        m_cosineChannelCount = 0;
        qCInfo(lcGenerator).noquote()
            << QStringLiteral("%1 RX ACK generator stopped")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)));
        emit runningChanged(false);
        return;
    }

    // ---------- 错误响应 ----------
    if (cmd == MotorProtocol::CmdErrorResponse) {
        m_ackTimeoutTimer->stop();
        qCWarning(lcGenerator).noquote()
            << QStringLiteral("%1 RX errorCode=0x%2 payload=%3")
                   .arg(QString::fromLatin1(MotorProtocol::commandName(cmd)))
                   .arg(MotorProtocol::decodeErrorCode(data), 2, 16, QLatin1Char('0'))
                   .arg(formatPayloadHex(data))
                   .toUpper();
        // 根据待确认操作类型决定状态回退
        switch (m_pendingOp) {
        case PendingOp::StartLinear:
        case PendingOp::StartCosine:
        case PendingOp::StartSawtooth:
            m_pendingOp = PendingOp::None;
            emit runningChanged(false);                 // 启动失败，保持停止状态
            break;
        case PendingOp::Stop:
            m_pendingOp = PendingOp::None;
            emit runningChanged(isRunning());           // 停止失败，保持当前运行状态
            break;
        case PendingOp::None:
            break;
        }
    }
}

// =============================================================================
// 超时处理
// =============================================================================

/// @brief ACK 超时回调：3 秒未收到响应，强制重置状态。
void GeneratorService::handleTimeout() {
    qCWarning(lcGenerator) << "Generator ACK timeout, pendingOp=" << static_cast<int>(m_pendingOp);
    switch (m_pendingOp) {
    case PendingOp::StartLinear:
    case PendingOp::StartCosine:
        m_pendingOp = PendingOp::None;
        emit runningChanged(false);                     // 启动超时，恢复停止状态
        break;
    case PendingOp::Stop:
        // 停止超时：假定设备已停止，强制重置本地状态
        m_mode = Mode::None;
        m_cosineChannelCount = 0;
        m_pendingOp = PendingOp::None;
        emit runningChanged(false);
        break;
    case PendingOp::None:
        break;
    }
}
