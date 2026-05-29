// =============================================================================
// @file    dw9786_strategy.h
// @brief   DW9786 (Dongwoon) 烧录策略
//
// 实现走真实硬件路径：通过 dw9786_bridge 桥接 vendor 库与 SerialManager，
// 按厂商序列 id_check → fw_download_with_buffer (erase + write + checksum)
// → chip_enable → fw_info。
//
// OTA 模式（eraseCalibration=false，默认）严禁调用 module_cal_erase；量产
// 模式可显式开启。
//
// 固件输入：DW9786 自定义 hex 文本（每行 1 个 32-bit hex 字符串，期望 10240
// 行，拆字规则 high16 先 low16 后，与 HL9788N 相反），由 FirmwareParser::
// parseFile 传入 Dw9786Hex 解析为 65520... 等。strategy 收到的 firmware 已是
// 40960 字节小端二进制，直接 reinterpret_cast 即可传 vendor SDK。
//
// 线程：与 AW / DW9788 一致，flash() 必须在 SerialManager 工作线程内调用。
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

class DW9786Strategy : public FlashStrategy {
public:
    using LogLevel = AwLocalIspStrategy::LogLevel;
    using LogSink  = AwLocalIspStrategy::LogSink;

    /// @brief 构造
    /// @param serialManager       SerialManager 实例（非空；外部持有）
    /// @param logSink             日志 sink；nullptr-safe
    /// @param eraseCalibration    true=量产模式（烧录后 erase 校准）；false=OTA 模式（默认，保留校准）
    /// @param slaveId8bit         I2C 从机地址 8-bit 形式；默认 0x24（vendor _SLV_OIS_ 默认值）
    DW9786Strategy(SerialManager *serialManager,
                    LogSink logSink,
                    bool eraseCalibration = false,
                    uint8_t slaveId8bit = 0x24);

    QString icModel() const override;
    QString icDescription() const override;

    bool flash(const QByteArray &firmware,
               std::function<void(qint64 sentBytes)> progress,
               const std::atomic<bool> &cancelFlag,
               QString *errorMessage) override;

    /// vendor 库一气呵成完成烧录（无 STM32 端 0x38 反馈），strategy 自己报全程 0~100% 进度。
    bool reportsFullProgress() const override { return true; }

private:
    void log(LogLevel level, const QString &message) const;

    SerialManager *m_serialManager = nullptr;
    LogSink        m_logSink;
    bool           m_eraseCalibration;
    uint8_t        m_slaveId8bit;
};
