// =============================================================================
// @file    simulatorserial.cpp
// @brief   模拟器专用串口驱动实现 — 独立线程串口管理、帧收发
//
// SimulatorSerial 在独立 QThread 中运行，拥有自己的 QSerialPort 和
// FrameParser 实例。与主线程 SerialManager 互不干扰，专用于模拟器
// 在第二个串口上与 STM32 通信。
//
// 线程模型：
//   - 构造时创建 QThread 并 moveToThread(m_thread)
//   - init() 在线程启动后由 QThread::started 信号触发
//   - openPort / closePort / sendRawFrame 可从任意线程调用
//     （因为 QSerialPort 已在 worker 线程中，需通过 invokeMethod 或直连）
//   - 析构时 BlockingQueuedConnection 关闭串口，再 quit + wait 线程
// =============================================================================
#include "services/simulatorserial.h"

#include <QIODevice>
#include <QMetaObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>

namespace {
/// @brief 拼接错误消息前缀与详情。
QString makeSerialErrorMessage(const QString &prefix, const QString &detail) {
    if (detail.isEmpty()) {
        return prefix;
    }
    return QStringLiteral("%1: %2").arg(prefix, detail);
}
} // namespace

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 创建独立线程，将自身 moveToThread 后启动。
///
/// 注意：parent 被显式忽略，因为跨线程对象不能有 QObject parent。
SimulatorSerial::SimulatorSerial(QObject *parent)
    : QObject(nullptr) {
    Q_UNUSED(parent);

    m_thread = new QThread();
    connect(m_thread, &QThread::started, this, &SimulatorSerial::init);

    moveToThread(m_thread);
    m_thread->start();
}

/// @brief 安全销毁：先关闭串口，再停止线程。
///
/// 使用 BlockingQueuedConnection 确保关闭操作在 worker 线程中完成。
SimulatorSerial::~SimulatorSerial() {
    if (m_thread == nullptr) {
        return;
    }

    if (m_thread->isRunning()) {
        // 在 worker 线程中关闭串口（阻塞等待完成）
        QMetaObject::invokeMethod(this, [this]() { closePortInternal(false); }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
    delete m_thread;
    m_thread = nullptr;
}

// =============================================================================
// 静态工具方法
// =============================================================================

/// @brief 枚举系统可用串口，返回排序后的端口名列表。
QStringList SimulatorSerial::availablePorts() {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports.append(info.portName());
    }
    ports.sort();
    return ports;
}

// =============================================================================
// 线程初始化
// =============================================================================

/// @brief 在 worker 线程中创建 QSerialPort 并连接信号。
///
/// 由 QThread::started 信号触发，确保 QSerialPort 的线程归属正确。
void SimulatorSerial::init() {
    if (m_serial != nullptr) {
        return;                                         // 防止重复初始化
    }

    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &SimulatorSerial::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        onSerialErrorOccurred(static_cast<int>(error));
    });
}

// =============================================================================
// 串口操作
// =============================================================================

/// @brief 打开指定串口，配置 8N1 无流控。
///
/// @param portName  串口名称（如 "COM3"）
/// @param baudRate  波特率
void SimulatorSerial::openPort(const QString &portName, qint32 baudRate) {
    if (m_serial == nullptr) {
        init();
    }

    // 若已打开，先关闭
    if (m_serial->isOpen()) {
        closePortInternal(true);
    }

    // ---------- 配置串口参数：8N1 无流控 ----------
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QStringLiteral("Failed to open %1: %2").arg(portName, m_serial->errorString()));
        return;
    }

    m_parser.reset();                                   // 重置帧解析器状态机
    emit connected();
}

/// @brief 关闭串口（外部调用入口）。
void SimulatorSerial::closePort() {
    closePortInternal(true);
}

/// @brief 发送原始帧数据到串口。
///
/// @param frame  已编码的完整帧字节（含帧头 + 校验）
void SimulatorSerial::sendRawFrame(const QByteArray &frame) {
    if (m_serial == nullptr || !m_serial->isOpen()) {
        emit errorOccurred(QStringLiteral("Simulator serial port is not open"));
        return;
    }

    m_serial->write(frame);
}

// =============================================================================
// 数据接收
// =============================================================================

/// @brief 串口数据到达回调：逐字节喂入 FrameParser，解析出 Control 帧后发射信号。
///
/// 注意：SimulatorSerial 只处理 Control 帧（AA 55 头），
/// 不处理 Stream 帧（BB 头），因为模拟器只接收上位机的控制命令。
void SimulatorSerial::onReadyRead() {
    if (m_serial == nullptr) {
        return;
    }

    const QByteArray bytes = m_serial->readAll();
    for (char byte : bytes) {
        const FrameParser::FrameType frameType = m_parser.feedByte(static_cast<uint8_t>(byte));
        if (frameType != FrameParser::FrameType::Control) {
            continue;                                   // 忽略非 Control 帧
        }

        // 解析完成一个完整 Control 帧，发射信号供 SimulatorService 处理
        const ControlFrame &frame = m_parser.controlFrame();
        emit frameReceived(frame.cmd, frame.seq, frame.data);
    }
}

// =============================================================================
// 错误处理
// =============================================================================

/// @brief 串口错误回调：致命错误（设备断开等）自动关闭串口。
void SimulatorSerial::onSerialErrorOccurred(int error) {
    const auto serialError = static_cast<QSerialPort::SerialPortError>(error);
    if (serialError == QSerialPort::NoError || m_serial == nullptr) {
        return;
    }

    emit errorOccurred(makeSerialErrorMessage(QStringLiteral("Simulator serial error"), m_serial->errorString()));

    // 致命错误：资源丢失、权限不足、设备不存在 → 自动关闭
    switch (serialError) {
    case QSerialPort::ResourceError:
    case QSerialPort::PermissionError:
    case QSerialPort::DeviceNotFoundError:
        closePortInternal(true);
        break;
    default:
        break;
    }
}

// =============================================================================
// 内部方法
// =============================================================================

/// @brief 关闭串口的内部实现。
///
/// @param emitDisconnected  是否发射 disconnected() 信号
///                          （析构时传 false 避免信号传递到已销毁对象）
void SimulatorSerial::closePortInternal(bool emitDisconnected) {
    m_parser.reset();                                   // 重置帧解析器

    const bool wasOpen = (m_serial != nullptr && m_serial->isOpen());
    if (wasOpen) {
        m_serial->close();
    }

    if (emitDisconnected && wasOpen) {
        emit disconnected();
    }
}
