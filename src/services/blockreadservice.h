// =============================================================================
// @file    blockreadservice.h
// @brief   寄存器块读取服务（服务层）— 连续地址段一次性 dump 到 CSV 文件
//
// 职责：
// - 接收 UI 的 start(startAddr, count, targetDir) 请求
// - 按地址 +2 步进逐个发起 RegisterService 单读
// - 失败即停（决策 D5）：首条失败立即停止，已读到的成功条目仍写入 CSV
// - 协作式取消（决策 D6）：cancelFlag + 命令间检查点，已读条目仍写入 CSV
// - 完成时按标准 CSV 格式写文件：首行表头 `addr,value`；数据行 `XXXX,XXXX`
//   4 位大写 hex 不带 `0x`；逗号后无空格；行尾 LF
// - 通过信号上报状态文字 / 进度 / 日志 / 完成结果
//
// 输出文件命名：`Bulkread_HHMMSS.csv`（PC 本地时间），同秒冲突自动追加 `_N` 后缀
//
// 互斥：本 service 与 BatchRegisterService / RegisterTable 完全独立（独立通道
// 决策 D7），通过独立 RegisterService 实例隔离命令流。底层 SerialManager 命令
// 队列共享，物理上仍串行执行。
//
// 线程：本服务对象生存于主线程；内部 RegisterService 通过 CommandDispatcher
// 与 SerialManager 工作线程通信，跨线程信号槽由 Qt 自动处理。
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QPair>

#include <cstdint>

class CommandDispatcher;
class RegisterService;

class BlockReadService : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Reading,
        WritingFile,
        Completed,
        Failed,
        Cancelled,
    };
    Q_ENUM(State)

    enum class LogLevel {
        Info,
        Warn,
        Error,
        Ok,
    };
    Q_ENUM(LogLevel)

    /// @brief 构造块读取服务
    /// @param dispatcher  CommandDispatcher（用于内部 RegisterService）
    /// @param parent      QObject parent
    explicit BlockReadService(CommandDispatcher *dispatcher, QObject *parent = nullptr);
    ~BlockReadService() override;

    State state() const { return m_state; }
    bool isBusy() const;

public slots:
    /// @brief 启动块读取（连续地址段 → CSV 文件）
    /// @param startAddr 起始地址（UI 层已校验越界）
    /// @param count     寄存器个数（≥ 1）
    /// @param targetDir 保存目录（必须是已存在的目录）
    void start(quint16 startAddr, int count, const QString &targetDir);

    /// @brief 协作式取消：置 cancelFlag；当前 in-flight 命令响应回来后立即收口
    void cancel();

signals:
    void stateChanged(BlockReadService::State newState);
    /// @brief 进度（done = 已成功读取条目数，total = 总条目数）
    void progress(int done, int total);
    /// @brief 状态文字（UI 状态标签直接渲染）
    void stageMessage(const QString &message);
    /// @brief 详细日志（写入 LogPanel）
    void logMessage(BlockReadService::LogLevel level, const QString &message);
    /// @brief 任务完成（成功 / 失败 / 取消）
    /// @param success    true=全部成功；false=失败或取消
    /// @param summary    完成时的总结文字（与最后一条 stageMessage 一致）
    /// @param savedPath  实际写入的 CSV 文件路径；写入失败或无数据时为空
    void finished(bool success, const QString &summary, const QString &savedPath);

private slots:
    void onInnerReadOk(int batchIndex, qint16 value);
    void onInnerReadError(int batchIndex);

private:
    void submitNextRead();
    /// @brief 收尾：写 CSV + emit finished + 清理上下文
    /// @param success     true=全部条目成功；false=失败 / 取消（保留 m_results 已读条目）
    /// @param finalText   状态文字（用于 stageMessage + summary）
    /// @param failedAddr  失败时的地址（仅 success=false 时有意义）
    void finishAndWriteCsv(bool success, const QString &finalText);
    /// @brief 生成输出文件路径 `Bulkread_HHMMSS.csv`，同秒冲突追加 `_N`
    /// @return 完整路径；若 999 次冲突仍无可用名返回空字符串
    QString resolveOutputPath() const;
    void setState(State s);

    RegisterService *m_innerService = nullptr;  ///< 独立 RegisterService 实例

    State m_state = State::Idle;
    quint16 m_startAddr = 0;
    int m_count = 0;
    QString m_targetDir;
    QVector<QPair<quint16, qint16>> m_results;  ///< 累积的已读条目（addr, value）
    int m_currentIndex = 0;
    bool m_cancelFlag = false;
};
