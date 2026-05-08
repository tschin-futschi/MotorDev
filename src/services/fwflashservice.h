// =============================================================================
// @file    fwflashservice.h
// @brief   固件烧录服务 — 协调前置序列、worker 线程、进度回调与状态机
//
// 职责：
// - 接收 UI 的 startFlash / cancelFlash 请求
// - 顺序触发前置停止序列（采样 → 发生器 → 循环写入 → PMIC），通过 4 个回调下发
//   （fire-and-forget，不强等 ACK；任一步失败仅写日志，不阻断烧录）
// - 在独立 worker 线程内调用 FlashStrategy::flash()
// - 通过信号回 UI 上报状态、阶段文本、进度、日志、最终结果
// - 取消通过 std::atomic<bool> cancelFlag 协作式实现
//
// 线程：
// - 本类对象生存于主线程
// - flash() 调用在 QThread::create 创建的 worker 上执行
// - 所有信号 emit 跨线程时由 Qt 自动转 QueuedConnection
// - m_state 仅在主线程修改（worker 通过 invokeMethod 回主线程切换状态）
// =============================================================================
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

class FlashStrategy;
class FlashStrategyRegistry;
class QThread;

class FwFlashService : public QObject {
    Q_OBJECT

public:
    /// @brief 烧录任务状态机
    enum class State {
        Idle,
        Preparing,
        StoppingScope,
        StoppingGenerator,
        StoppingCyclic,
        DisablingPmic,
        Flashing,
        Completed,
        Failed,
        Cancelled,
    };
    Q_ENUM(State)

    /// @brief 日志级别（与 FwFlashLogPanel 的 4 类着色一致）
    enum class LogLevel {
        Info,
        Warn,
        Error,
        Ok,
    };
    Q_ENUM(LogLevel)

    using StopCallback = std::function<void()>;

    /// @brief 构造服务
    /// @param registry 策略注册中心；外部持有，service 不接管所有权
    /// @param parent QObject parent
    explicit FwFlashService(FlashStrategyRegistry *registry, QObject *parent = nullptr);
    ~FwFlashService() override;

    /// @brief 注入"立即停止"回调；nullptr-safe，未设置时跳过该步骤（仅写日志）
    void setStopScopeCallback(StopCallback cb);
    void setStopGeneratorCallback(StopCallback cb);
    void setStopCyclicWriteCallback(StopCallback cb);
    void setDisablePmicCallback(StopCallback cb);

    /// @brief 当前状态
    State state() const { return m_state; }

    /// @brief 是否处于"忙碌"区间（Preparing 到 Flashing 之间任意状态）
    bool isBusy() const;

public slots:
    /// @brief 启动烧录任务
    /// @param icModel    目标 IC 型号（必须已在 registry 注册）
    /// @param firmware   合并后的连续二进制（应为 FirmwareInfo::data）
    /// @param totalBytes 进度条总字节数；通常等于 firmware.size()
    void startFlash(const QString &icModel, const QByteArray &firmware, qint64 totalBytes);

    /// @brief 请求取消正在进行的烧录任务
    void cancelFlash();

signals:
    void stateChanged(FwFlashService::State newState);
    void stageMessage(const QString &message);
    void progressUpdated(qint64 sentBytes, qint64 totalBytes);
    void logMessage(FwFlashService::LogLevel level, const QString &message);
    void finished(bool success, const QString &summary);

private:
    void setState(State s);
    void emitLog(LogLevel level, const QString &message);
    void runPreflightAndFlash(FlashStrategy *strategy, const QByteArray &firmware);
    void shutdownWorker();

    FlashStrategyRegistry *m_registry = nullptr;
    StopCallback m_stopScope;
    StopCallback m_stopGenerator;
    StopCallback m_stopCyclic;
    StopCallback m_disablePmic;

    State m_state = State::Idle;
    qint64 m_totalBytes = 0;

    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    QThread *m_workerThread = nullptr;
};
