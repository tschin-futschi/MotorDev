// =============================================================================
// @file    flashstoreservice.h
// @brief   STM32 自身 FLASH 文件存储服务（上位机端，协议 v2.10 / 0x39~0x3E）
//
// 与 FwFlashService 模式对称：
// - 接收 UI 的 startWrite / startRead / refreshInfo / cancel 请求
// - 通过 invokeMethod(QueuedConnection) 把任务投递到 SerialManager 工作线程
// - 工作线程内 fast-path：同步调 SerialManager::sendAndWaitResponse
// - 任务前 stopHeartbeat、任务后 startHeartbeat（WRITE_BEGIN 期间 STM32 阻塞 3-7s）
// - 协作式取消（std::atomic<bool> cancelFlag）
//
// 不带前置序列：与 FwFlashService 不同，STM32 端 WRITE_BEGIN handler 已内置
// 与采样 / 发生器 / 循环写入的互斥检查。若用户在并发场景下点烧录，STM32 会
// 返回 ErrorResponse，本类如实把错误上报给 UI 提示用户。
//
// 线程：
// - 本类对象生存于主线程
// - flash() 调用在 SerialManager 工作线程内同步执行（不创建新线程）
// - 所有信号 emit 跨线程时由 Qt 自动转 QueuedConnection
// - m_state 仅在主线程修改
// =============================================================================
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

class SerialManager;

class FlashStoreService : public QObject {
    Q_OBJECT

public:
    /// @brief 任务状态机
    enum class State {
        Idle,
        WritePreparing,    ///< 读文件 + 算 CRC32（主线程瞬时）
        WriteBeginning,    ///< 发 0x39 等响应（含 3-7s 整区擦）
        WriteSending,      ///< 0x3A 循环发包
        WriteEnding,       ///< 发 0x3B CRC 校验 + 提交元数据
        ReadBeginning,     ///< 发 0x3C 取 size + crc32
        ReadFetching,      ///< 0x3D 循环收包 + 落盘
        QueryingInfo,      ///< 发 0x3E 容量查询
        Completed,
        Failed,
        Cancelled,
    };
    Q_ENUM(State)

    /// @brief 日志级别（与 FwFlashLogPanel 4 级着色一致）
    enum class LogLevel {
        Info,
        Warn,
        Error,
        Ok,
    };
    Q_ENUM(LogLevel)

    explicit FlashStoreService(SerialManager *serialManager, QObject *parent = nullptr);
    ~FlashStoreService() override;

    State state() const { return m_state; }

    /// @brief 是否处于忙碌区间（Preparing 到 Querying 之间任意状态）
    bool isBusy() const;

public slots:
    /// @brief 上传本地文件到 STM32 Flash 文件存储区
    /// @param srcFilePath  本地文件绝对路径；> 917488 字节会被拒绝（UI 应预先校验）
    void startWrite(const QString &srcFilePath);

    /// @brief 下载 STM32 Flash 中的文件到 PC 本地
    /// @param dstDirPath  目录路径；文件落盘为 <dir>/FLASH_HHMMSS.txt
    ///                    （HHMMSS 取 PC 本地时间；同秒冲突时追加 _N）
    void startRead(const QString &dstDirPath);

    /// @brief 查询容量（0x3E INFO 单帧），不读文件内容
    void refreshInfo();

    /// @brief 清空 STM32 Flash 文件存储区（0x3F WIPE，整区擦回 0xFF）
    /// 阻塞 3-7s 与上传相同。完成后 service 会自动 emit infoUpdated(917488, 0)
    /// 让 UI 容量行立即刷新到"已用 0 / 剩余 896 KB"。
    void startWipe();

    /// @brief 协作式取消（翻 cancelFlag；strategy 在下一个安全点退出）
    void cancel();

signals:
    void stateChanged(FlashStoreService::State newState);
    void stageMessage(const QString &message);
    void progressUpdated(qint64 done, qint64 total);
    void logMessage(FlashStoreService::LogLevel level, const QString &message);
    void finished(bool success, const QString &summary);

    /// @brief 0x3E INFO 响应回来或 WRITE_END / READ_END 隐式触发刷新时 emit
    void infoUpdated(quint32 totalCapacity, quint32 usedSize);

    /// @brief 下载成功时附最终落盘路径（UI 可弹"已保存到 XX"）
    void readCompleted(const QString &savedFilePath);

private:
    void setState(State s);
    void emitLog(LogLevel level, const QString &message);

    /// @brief 申请进入 fast-path 前的统一前置：状态检查 + 心跳暂停
    /// @return true 表示可以进入；false 表示当前忙（已写日志）
    bool armBusy();
    /// @brief fast-path 完成后的统一收尾：状态切回 Idle + 心跳恢复 + emit finished
    void releaseBusy(bool success, bool wasCancelled, const QString &summary);

    /// @brief 把烧录任务投递到 SerialManager 工作线程同步执行
    /// （内部决定具体路径：write / read / info / wipe）
    enum class Op { Write, Read, Info, Wipe };
    void dispatchOp(Op op,
                    const QByteArray &writeData = {},
                    quint32 writeCrc = 0,
                    const QString &readDir = QString());

    SerialManager *m_serialManager = nullptr;

    State m_state = State::Idle;
    bool  m_flashInFlight = false;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
};
