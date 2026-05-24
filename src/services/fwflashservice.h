// =============================================================================
// @file    fwflashservice.h
// @brief   固件烧录服务 — 协调前置序列、worker 线程、进度回调与状态机
//
// 职责：
// - 接收 UI 的 startFlash / cancelFlash 请求
// - 顺序触发前置停止序列（采样 → 发生器 → 循环写入），通过 3 个回调下发
//   （fire-and-forget，不强等 ACK；任一步失败仅写日志，不阻断烧录）
//   注：PMIC 不在前置序列中关闭，烧录期间 IC 必须保持正常供电
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
class SerialManager;

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
    FwFlashService(FlashStrategyRegistry *registry,
                   SerialManager *serialManager,
                   QObject *parent = nullptr);
    ~FwFlashService() override;

    /// @brief 注入"立即停止"回调；nullptr-safe，未设置时跳过该步骤（仅写日志）
    void setStopScopeCallback(StopCallback cb);
    void setStopGeneratorCallback(StopCallback cb);
    void setStopCyclicWriteCallback(StopCallback cb);

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

    /// @brief 接收 STM32 端 0x38 FLASH_EXEC_PROGRESS 事件帧。
    /// 由 MainWindow 在 unsolicitedFrameReceived 分支解码后通过 invokeMethod 入队调用。
    /// 仅在 m_state == Flashing 时生效；非 Flashing 状态收到也只是 warn 日志，不改进度。
    /// @param phase 0=ERASE / 1=WRITE（其他值忽略）
    /// @param done  当前阶段已完成单元数
    /// @param total 当前阶段总单元数
    void onFlashExecProgress(quint8 phase, quint32 done, quint32 total);

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
    /// @brief 按总进度百分比（0~100）emit progressUpdated。
    /// 将 pct 折算为等效字节数（pct * m_totalBytes / 100），交给 UI 的 progress bar。
    /// 所有阶段（DATA/ERASE/WRITE/TAIL）的进度上报都通过此函数，权重见 FwFlashProgress 命名空间。
    void emitProgressPct(int pct);

    FlashStrategyRegistry *m_registry = nullptr;
    SerialManager *m_serialManager = nullptr;
    StopCallback m_stopScope;
    StopCallback m_stopGenerator;
    StopCallback m_stopCyclic;

    State m_state = State::Idle;
    qint64 m_totalBytes = 0;
    bool m_flashInFlight = false;  ///< 烧录任务已投递到 SerialManager 工作线程且尚未回报

    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
};

// -----------------------------------------------------------------------------
// 总进度条权重映射（4 阶段，合计 100%）
//   DATA  : PC→STM32 SRAM 传输阶段（真实字节进度）
//   ERASE : STM32 调 aw_flash_block_erase_check（一次性 ~800 ms）
//   WRITE : STM32 写 Flash block 循环（占 EXEC 主体 ~6 s）
//   TAIL  : EXEC 响应到达 + reset_chip（~100 ms）
// 实测后可微调；改动只需调本常量，所有进度折算逻辑都基于它。
// 见 plan_flash_progress_0x38_*.md §"EXEC 阶段权重的物理含义"。
// -----------------------------------------------------------------------------
namespace FwFlashProgress {
inline constexpr int kPctData  = 20;
inline constexpr int kPctErase = 5;
inline constexpr int kPctWrite = 70;
inline constexpr int kPctTail  = 5;
static_assert(kPctData + kPctErase + kPctWrite + kPctTail == 100,
              "FwFlashProgress weights must sum to 100");
}  // namespace FwFlashProgress
