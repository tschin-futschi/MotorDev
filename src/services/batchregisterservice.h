// =============================================================================
// @file    batchregisterservice.h
// @brief   寄存器批量读写服务（服务层）— 文件解析 + 顺序读写 + 进度上报
//
// 职责：
// - 接收 UI 的 startWrite(slotIndex, filePath) / startRead(slotIndex, filePath) 请求
// - 调用 BatchRegisterFile::parseFile 解析配置文件（严格全或无）
// - 按文件顺序逐条调用内部 RegisterService 单读 / 单写
// - 遇错即停（决策 #6 / #7）；读出中止时原文件完全不动
// - 全部成功时调 BatchRegisterFile::writeBackValues 回写文件（仅读出）
// - 通过信号上报状态文字 / 日志 / 完成结果
//
// 本服务封装了批量读写的所有业务逻辑，让 UI 层（RegisterRwTab）只负责
// 控件交互和信号渲染，业务与 UI 完全解耦。
//
// 互斥：本 service 一次只处理 1 个任务；外部互斥（与 Sidebar 全部读写
// 之间的协调）由 RegisterRwTab 维护。
//
// 线程：本服务对象生存于主线程；内部 RegisterService 通过 CommandDispatcher
// 与 SerialManager 工作线程通信，跨线程信号槽由 Qt 自动处理。
// =============================================================================
#pragma once

#include "protocol/batch_register_file.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class CommandDispatcher;
class RegisterService;

class BatchRegisterService : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Parsing,
        Writing,
        Reading,
        Completed,
        Failed,
    };
    Q_ENUM(State)

    enum class LogLevel {
        Info,
        Warn,
        Error,
        Ok,
    };
    Q_ENUM(LogLevel)

    /// @brief 构造批量读写服务
    /// @param dispatcher  CommandDispatcher（用于内部 RegisterService）
    /// @param parent      QObject parent
    explicit BatchRegisterService(CommandDispatcher *dispatcher, QObject *parent = nullptr);
    ~BatchRegisterService() override;

    State state() const { return m_state; }
    bool isBusy() const;

public slots:
    /// @brief 启动批量写入（文件 → 芯片）
    /// @param slotIndex   UI 槽编号 0~3（仅用于状态文字标识 "#N"；service 自身不解读）
    /// @param filePath    配置文件路径
    void startWrite(int slotIndex, const QString &filePath);

    /// @brief 启动批量读出（芯片 → 文件）
    /// @param slotIndex   UI 槽编号 0~3
    /// @param filePath    配置文件路径
    void startRead(int slotIndex, const QString &filePath);

signals:
    void stateChanged(BatchRegisterService::State newState);
    /// @brief 状态文字（UI 状态标签直接渲染）
    void stageMessage(const QString &message);
    /// @brief 详细日志（写入 LogPanel）
    void logMessage(BatchRegisterService::LogLevel level, const QString &message);
    /// @brief 任务完成（成功 / 失败）
    /// @param success  true=全部成功；false=失败 / 中止 / 解析错误
    /// @param summary  完成时的总结文字（与最后一条 stageMessage 一致）
    void finished(bool success, const QString &summary);

private slots:
    void onInnerReadOk(int batchIndex, qint16 value);
    void onInnerReadError(int batchIndex);
    void onInnerWriteOk(int batchIndex);
    void onInnerWriteError(int batchIndex);

private:
    void start(int slotIndex, const QString &filePath, bool isWrite);
    void submitNextEntry();
    void finish(bool success, const QString &finalStatusText);
    void setState(State s);
    QString opName() const;

    RegisterService *m_innerService = nullptr;  ///< 内部专用 RegisterService（与 RegisterTable 的 service 隔离，避免 globalRow 串扰）

    State m_state = State::Idle;
    bool m_isWrite = false;
    int m_activeSlotIndex = -1;
    QString m_activeFilePath;
    QStringList m_activeRawLines;
    QVector<BatchRegisterFile::Entry> m_activeEntries;
    int m_activeIndex = 0;
};
