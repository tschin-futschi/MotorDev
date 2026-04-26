// =============================================================================
// @file    commanddispatcher.h
// @brief   命令分发器（服务层）
//
// 在 SerialManager 之上提供优先级命令队列和回调机制。
//
// 核心功能：
// - 命令排队：按优先级（Urgent/High/Normal/Low）插入队列
// - 单命令串行发送：同一时刻只有一条命令在等待响应
// - 响应匹配：通过 seq 或 cmd 匹配响应帧，触发提交时注册的回调
// - 错误处理：串口错误或断开时，自动以 CmdErrorResponse 失败所有挂起命令
// - 未匹配帧转发：不匹配任何挂起命令的帧作为设备主动上报帧转发
//
// 使用方式：上层 Service（ConfigService、RegisterService、ScopeService 等）
// 通过 submitCommand() 提交命令并注册回调，无需直接操作 SerialManager。
//
// 线程安全：CommandDispatcher 在主线程运行，通过 QueuedConnection 调用
// SerialManager（工作线程）的 sendCommand。
// =============================================================================

#pragma once

#include <QByteArray>
#include <QObject>
#include <QVector>

#include <cstdint>
#include <functional>

class SerialManager;

/// @brief 命令分发器
///
/// 管理命令队列，按优先级调度命令发送，并在收到响应时回调通知上层。
class CommandDispatcher : public QObject {
    Q_OBJECT

public:
    /// @brief 命令优先级（数值越小优先级越高）
    enum Priority {
        Urgent = 0,   ///< 紧急（如心跳、错误恢复）
        High = 1,     ///< 高优先级
        Normal = 2,   ///< 普通（默认）
        Low = 3,      ///< 低优先级
    };
    Q_ENUM(Priority)

    /// @brief 响应回调函数类型
    /// @param cmd   响应命令字节
    /// @param seq   序列号
    /// @param data  响应载荷
    using ResponseCallback = std::function<void(uint8_t cmd, uint8_t seq, const QByteArray &data)>;

    /// @brief 命令票据类型，用于取消命令
    using Ticket = uint32_t;

    static constexpr Ticket InvalidTicket = 0;    ///< 无效票据
    static constexpr uint8_t LocalErrorCode = 0xFF;  ///< 本地错误码（非设备返回的错误）

    /// @brief 构造命令分发器
    /// @param serialManager  串口管理器实例（工作线程中运行）
    /// @param parent         父对象
    explicit CommandDispatcher(SerialManager *serialManager, QObject *parent = nullptr);

    /// @brief 提交一条命令到队列
    /// @param cmd         命令字节
    /// @param data        载荷数据
    /// @param priority    优先级
    /// @param onResponse  收到响应时的回调（包括错误响应和超时）
    /// @return 命令票据；可用于后续取消
    Ticket submitCommand(uint8_t cmd,
                         const QByteArray &data,
                         Priority priority,
                         ResponseCallback onResponse);

    /// @brief 取消队列中尚未发送的命令
    /// @param ticket  命令票据
    /// @return 取消成功返回 true；已发送或不存在返回 false
    bool cancelCommand(Ticket ticket);

signals:
    /// @brief 命令已发送到串口（含分配到的 seq 号）
    void commandDispatched(uint32_t ticket, uint8_t cmd, uint8_t seq);

    /// @brief 收到未匹配任何挂起命令的帧（设备主动上报）
    void unsolicitedFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);

private slots:
    /// @brief 处理 SerialManager 返回的控制帧
    void onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data);

    /// @brief 记录已发送命令的 seq（用于后续响应匹配）
    void onCommandSent(uint8_t cmd, uint8_t seq);

    /// @brief 处理串口错误
    void onSerialError(const QString &message);

    /// @brief 处理串口断开
    void onSerialDisconnected();

private:
    /// @brief 队列中的待发送命令条目
    struct PendingEntry {
        Ticket ticket = InvalidTicket;     ///< 命令票据
        uint8_t cmd = 0;                   ///< 命令字节
        QByteArray data;                   ///< 载荷数据
        Priority priority = Normal;        ///< 优先级
        ResponseCallback onResponse;       ///< 响应回调
    };

    /// @brief 生成本地错误的载荷（1 字节 0xFF）
    static QByteArray localErrorPayload();

    /// @brief 判断错误消息是否属于串口级别致命错误
    static bool isSerialLevelError(const QString &message);

    /// @brief 尝试从队列取出下一条命令并发送
    void trySendNext();

    /// @brief 清除当前活跃命令状态
    void clearActive();

    /// @brief 以错误回调通知当前活跃命令失败
    void failActive(const QString &message);

    /// @brief 以错误回调通知队列中所有命令失败并清空队列
    void failQueued(const QString &message);

    SerialManager *m_serialManager = nullptr;  ///< 串口管理器引用
    QVector<PendingEntry> m_queue;             ///< 优先级命令队列
    Ticket m_nextTicket = 1;                   ///< 下一个可分配的票据号

    // --- 当前活跃命令状态 ---
    bool m_waitingResponse = false;            ///< 是否正在等待响应
    Ticket m_activeTicket = InvalidTicket;     ///< 活跃命令的票据
    uint8_t m_activeSeq = 0xFF;                ///< 活跃命令的 seq（0xFF 表示尚未收到 commandSent）
    uint8_t m_activeCmd = 0;                   ///< 活跃命令的 cmd
    ResponseCallback m_activeCallback;         ///< 活跃命令的回调
};
