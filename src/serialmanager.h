// =============================================================================
// @file    serialmanager.h
// @brief   串口管理器（通信层）
//
// 封装 QSerialPort，在独立工作线程中运行，提供串口打开/关闭、命令发送、
// 帧解析、自动重试和心跳检测功能。
//
// 线程模型：
// - SerialManager 在构造时自动 moveToThread 到独立线程
// - 所有 public slots 必须通过 QMetaObject::invokeMethod + QueuedConnection 调用
// - 信号可安全跨线程连接（Qt 自动选择 QueuedConnection）
//
// 命令发送流程：
// sendCommand() → 编码帧 → 写入串口 → 启动重试定时器
// → 收到响应匹配 seq → clearPendingCommand + emit frameReceived
// → 超时未响应 → 自动重试（最多 MaxRetries 次）→ 超时报错
// =============================================================================

#pragma once

#include "frameparser.h"
#include "protocol/motor_protocol.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <cstdint>

class QEventLoop;
class QSerialPort;
class QThread;
class QTimer;

/// @brief 串口管理器
///
/// 在独立线程中管理串口通信，提供帧级别的命令收发接口。
/// 包含自动重试机制和心跳保活检测。
///
/// @note 该对象在独立线程运行，外部调用必须通过 invokeMethod + QueuedConnection。
class SerialManager : public QObject {
    Q_OBJECT

public:
    /// @brief 构造串口管理器并启动工作线程
    /// @param parent  逻辑父对象（注意：实际 QObject parent 为 nullptr，
    ///                因为 moveToThread 不允许有父对象）
    explicit SerialManager(QObject *parent = nullptr);

    /// @brief 析构函数，安全关闭串口并停止工作线程
    ~SerialManager() override;

    /// @brief 获取系统可用的串口列表（静态方法，可在任意线程调用）
    /// @return 已排序的串口名称列表
    static QStringList availablePorts();

public slots:
    /// @brief 初始化串口对象和定时器（在工作线程中调用）
    void init();

    /// @brief 打开指定串口
    /// @param portName  串口名称（如 "COM3"）
    /// @param baudRate  波特率
    void openPort(const QString &portName, qint32 baudRate);

    /// @brief 关闭当前打开的串口（会发出 disconnected 信号）
    void closePort();

    /// @brief 启动心跳检测定时器
    void startHeartbeat();

    /// @brief 停止心跳检测定时器
    void stopHeartbeat();

    /// @brief 发送一条命令
    /// @param cmd   命令字节（定义见 MotorProtocol）
    /// @param data  载荷数据（默认为空）
    void sendCommand(uint8_t cmd, const QByteArray &data = {});

    /// @brief 同步发送一条控制帧并等待匹配响应（**必须在 SerialManager 工作线程内调用**）
    ///
    /// 烧录链路专用 fast-path：worker QObject moveToThread(serialManager->thread()) 之后，
    /// 在工作线程内同步执行整个 send + wait + parse，全程不跨线程、不经过 dispatcher，
    /// 用 `QSerialPort::waitForBytesWritten` / `waitForReadyRead` 等同步 I/O。
    /// 调用期间不接收任何采样流帧或其它服务的命令响应（其它信号也不会被分发）。
    ///
    /// 注意：调用前应先 `stopHeartbeat()`，结束后再 `startHeartbeat()`，避免心跳定时器在
    /// 这条同步路径之外被错误阻塞。
    ///
    /// @param cmd       要发送的命令字节
    /// @param data      命令载荷
    /// @param outCmd    [out] 收到的响应命令字节
    /// @param outSeq    [out] 收到的响应序列号
    /// @param outData   [out] 收到的响应载荷
    /// @param timeoutMs 整体超时（包含写完成与等响应两段）
    /// @return true=收到响应（含 CmdErrorResponse 错误响应也算收到）；false=超时/写失败/未连接
    bool sendAndWaitResponse(uint8_t cmd,
                              const QByteArray &data,
                              uint8_t &outCmd,
                              uint8_t &outSeq,
                              QByteArray &outData,
                              int timeoutMs);

public:
    /// @brief 最近一次 sendAndWaitResponse 的诊断字段（供烧录链路定位延迟来源）
    struct FastPathDiag {
        qint64 wroteBytes = 0;
        bool   flushOk = false;
        qint64 bytesToWriteAfterFlush = 0;
        qint64 execMs = 0;           ///< loop.exec() 阻塞耗时
        bool   timerFired = false;   ///< 是否由 5s 超时 timer 触发退出
        bool   gotMatch = false;     ///< 是否收到匹配响应
        qint64 bytesAvailableAfter = 0;
        qint64 bytesToWriteAfter = 0;
    };

    /// @brief 读最近一次 sendAndWaitResponse 的诊断快照（必须在同线程紧接调用）
    FastPathDiag lastFastPathDiag() const { return m_lastDiag; }

signals:
    /// @brief 串口连接成功
    void connected();

    /// @brief 串口已断开
    void disconnected();

    /// @brief 命令已发送到串口（含 seq 号，用于上层跟踪）
    void commandSent(uint8_t cmd, uint8_t seq);

    /// @brief 收到控制帧响应
    /// @param cmd   响应命令字节
    /// @param seq   序列号（与请求配对）
    /// @param data  响应载荷数据
    void frameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);

    /// @brief 收到流帧数据（示波器采样）
    /// @param channelMask  通道掩码
    /// @param samples      各通道采样值
    void streamDataReceived(uint8_t channelMask, const QVector<int16_t> &samples);

    /// @brief 发生错误
    /// @param message  错误描述信息
    void errorOccurred(const QString &message);

private slots:
    /// @brief 串口数据可读回调，逐字节喂入 FrameParser
    void onReadyRead();

    /// @brief 重试定时器超时回调
    void onRetryTimeout();

    /// @brief 心跳定时器超时回调
    void onHeartbeatTimeout();

    /// @brief 串口硬件错误回调
    void onSerialErrorOccurred(int error);

private:
    /// @brief 清除当前挂起的命令状态
    void clearPendingCommand();

    /// @brief 停止重试定时器
    void stopRetryTimer();

    /// @brief 停止心跳定时器
    void stopHeartbeatTimer();

    /// @brief 处理解析完成的控制帧
    void handleControlFrame(const ControlFrame &frame);

    /// @brief 内部关闭串口
    /// @param emitDisconnectedSignal  是否发出 disconnected 信号
    void closePortInternal(bool emitDisconnectedSignal);

    QThread *m_thread = nullptr;          ///< 工作线程（手动管理生命周期）
    QSerialPort *m_serial = nullptr;      ///< 串口对象（在工作线程创建）
    FrameParser m_parser;                 ///< 帧解析器实例
    uint8_t m_nextSeq = 0;               ///< 下一个可用的序列号（递增分配）

    // --- 命令重试机制 ---
    QTimer *m_retryTimer = nullptr;       ///< 重试超时定时器（单次触发）
    QByteArray m_pendingFrame;            ///< 当前挂起的完整帧数据（用于重发）
    uint8_t m_pendingSeq = 0;             ///< 当前挂起命令的序列号
    uint8_t m_pendingCmd = 0;             ///< 当前挂起命令的命令字节
    int m_retryCount = 0;                 ///< 已重试次数

    // --- 心跳保活 ---
    QTimer *m_heartbeatTimer = nullptr;   ///< 心跳定时器（周期触发）
    int m_missedHeartbeats = 0;           ///< 连续未收到心跳响应的次数

    // --- 烧录链路 fast-path 状态（仅在 sendAndWaitResponse 嵌套 event loop 期间有效） ---
    struct FastPathState {
        bool active = false;
        uint8_t expectedSeq = 0;
        uint8_t outCmd = 0;
        uint8_t outSeq = 0;
        QByteArray outData;
        bool gotMatch = false;
        QEventLoop *loop = nullptr;
    };
    FastPathState m_fastPath;
    FastPathDiag  m_lastDiag;     ///< 最近一次 sendAndWaitResponse 的诊断快照

    // --- 常量 ---
    static constexpr int MaxRetries = 2;             ///< 最大重试次数
    static constexpr int RetryTimeoutMs = 100;       ///< 重试超时时间（毫秒）
    static constexpr int HeartbeatIntervalMs = 1000; ///< 心跳间隔（毫秒）
    static constexpr int MaxMissedHeartbeats = 3;    ///< 最大允许的心跳丢失次数（超过则断开）
};
