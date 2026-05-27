// =============================================================================
// @file    dw9788_strategy.h
// @brief   DW9788 (Dongwoon HL9788N) 烧录策略
//
// 实现走真实硬件路径：通过 hl9788n_bridge 桥接 vendor 库与 SerialManager，
// 按厂商 PDF 序列 id_check → fw_update_dma → ois_reset → fw_info → verify。
//
// OTA 模式（eraseCalibration=false，默认）严禁调用 module_cal_erase；量产
// 模式可显式开启。
//
// 固件输入：DW 自定义 hex 文本（每行一个 32-bit hex 字符串），strategy 内部
// 解析为 uint16_t[FW_SIZE]；不依赖 FirmwareParser。
//
// 线程：与 AW 一致，flash() 必须在 SerialManager 工作线程内调用。
// =============================================================================
#pragma once

#include "services/flashstrategies/aw_local_isp_strategy.h"  // for LogLevel/LogSink alias
#include "services/flashstrategy.h"

#include <QByteArray>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>

class SerialManager;

class DW9788Strategy : public FlashStrategy {
public:
    using LogLevel = AwLocalIspStrategy::LogLevel;
    using LogSink  = AwLocalIspStrategy::LogSink;

    /// @brief 构造
    /// @param serialManager       SerialManager 实例（非空；外部持有）
    /// @param logSink             日志 sink；nullptr-safe
    /// @param eraseCalibration    true=量产模式（烧录后 erase 校准）；false=OTA 模式（默认，保留校准）
    /// @param slaveId8bit         I2C 从机地址 8-bit 形式；默认 0x48（厂商 PDF 示例值）
    DW9788Strategy(SerialManager *serialManager,
                    LogSink logSink,
                    bool eraseCalibration = false,
                    uint8_t slaveId8bit = 0x48);

    QString icModel() const override;
    QString icDescription() const override;

    bool flash(const QByteArray &firmware,
               std::function<void(qint64 sentBytes)> progress,
               const std::atomic<bool> &cancelFlag,
               QString *errorMessage) override;

    /// vendor 库一气呵成完成烧录（无 STM32 端 0x38 反馈），strategy 自己报全程 0~100% 进度。
    bool reportsFullProgress() const override { return true; }

private:
    /// @brief 把 DW 自定义 hex 文本（每行 32-bit hex）解析为 vendor 内存布局的 uint16_t 数组
    /// @param firmware  原始字节流（hex 文本）
    /// @param outWords  输出 uint16_t 数组（FW_SIZE = 0x10000 个元素）
    /// @param errOut    解析失败原因（含行号）
    /// @return true=解析成功；false=失败
    bool parseHl9788nHex(const QByteArray &firmware,
                          uint16_t *outWords,
                          QString *errOut) const;

    void log(LogLevel level, const QString &message) const;

    SerialManager *m_serialManager = nullptr;
    LogSink        m_logSink;
    bool           m_eraseCalibration;
    uint8_t        m_slaveId8bit;
};
