// =============================================================================
// @file    aw_local_isp_strategy.h
// @brief   AW 本地 ISP 烧录策略基类(AW86008 / AW86100 共用,协议 0x32~0x37)
//
// 替代旧 AwSdkStrategy:不再依赖 PC 端 DLL,烧录逻辑下沉到 STM32 端的 ISP 驱动
// (`Flash/AW/AW86008_86100/aw_flash.c` / `aw_uboot_isp.c`)。
// 上位机仅做:0x32 BEGIN → 多次 0x33 DATA → 0x34 EXEC,失败时 0x37 RESET_CHIP +
// 0x36 CANCEL 收尾。`AW_86008_86100_Flash_Run` 内部已经在成功路径上完成 reset,
// 因此正常成功流程下上位机不发 0x37。
//
// 线程模型(fast-path):
// - 实例存活期间由 FwFlashService 通过 invokeMethod 投递到 SerialManager 工作线程
// - flash() 在工作线程内同步执行,期间该线程 event loop 被 strategy 占用
// - 每条命令通过 `SerialManager::sendAndWaitResponse` 在嵌套 event loop 中同步收发
// - 心跳暂停由 FwFlashService 在调 flash() 前后管理,strategy 不感知
//
// 子类只需 override icModel() / icDescription();目标 Flash 地址默认
// `AW_FLASH_BASE = 0x01000000`,如不同 IC 需调整可 override flashTargetAddr()
// =============================================================================
#pragma once

#include "services/flashstrategy.h"

#include <QByteArray>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>

class SerialManager;

class AwLocalIspStrategy : public FlashStrategy {
public:
    /// @brief 日志级别(与 FwFlashLogPanel 4 级着色对齐)
    enum class LogLevel { Info, Warn, Error, Ok };

    /// @brief 日志接收 sink;实现方需自行 marshal 到 GUI 线程
    using LogSink = std::function<void(LogLevel level, const QString &message)>;

    AwLocalIspStrategy(SerialManager *serialManager, LogSink logSink);
    ~AwLocalIspStrategy() override = default;

    bool flash(const QByteArray &firmware,
               std::function<void(qint64 sentBytes)> progress,
               const std::atomic<bool> &cancelFlag,
               QString *errorMessage) override;

protected:
    /// @brief 烧录目标 Flash 起始地址。默认 `AW_FLASH_BASE = 0x01000000`,
    /// AW86008 / AW86100 共用 ISP 驱动,故子类无需 override。
    virtual quint32 flashTargetAddr() const { return 0x01000000U; }

private:
    bool doBegin(quint32 addr, quint32 totalBytes, QString *err);
    bool doData(const QByteArray &firmware,
                const std::function<void(qint64)> &progress,
                const std::atomic<bool> &cancelFlag,
                QString *err);
    bool doExec(uint8_t &ispStatusOut, QString *err);
    void doResetChip();   ///< 失败收尾用,忽略返回(尽力而为)
    void doCancel();      ///< 失败收尾用,忽略返回

    /// @brief 通用同步发命令 + 等响应。错误响应帧已在此层翻译为 errorMessage。
    /// @return true=收到正常响应(非错误响应);false=超时/写失败/错误响应
    bool sendCmd(uint8_t cmd,
                 const QByteArray &payload,
                 QByteArray &outData,
                 int timeoutMs,
                 const char *stageName,
                 QString *err);

    /// @brief 进度上报(节流)
    void notifyProgress(qint64 sentBytes);

    void log(LogLevel level, const QString &message) const;

    SerialManager *m_serialManager = nullptr;
    LogSink m_logSink;

    // flash() 调用期间使用
    std::function<void(qint64)> m_progress;
    qint64 m_lastReportedBytes = 0;
    qint64 m_lastReportTimeMs = 0;
};
