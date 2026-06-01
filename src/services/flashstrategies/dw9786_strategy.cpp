// =============================================================================
// @file    dw9786_strategy.cpp
// @brief   DW9786 烧录策略实现
// =============================================================================
#include "services/flashstrategies/dw9786_strategy.h"

#include "services/flashstrategies/dw9786_bridge.h"
#include "serialmanager.h"

#include <QByteArray>
#include <QString>
#include <QThread>

#include <cstring>
#include <utility>

// vendor 包装头放最后：与 dw9786_bridge.cpp 同理
#include "services/flashstrategies/dw9786_vendor_include.h"

DW9786Strategy::DW9786Strategy(SerialManager *serialManager,
                                LogSink logSink,
                                bool eraseCalibration,
                                uint8_t slaveId8bit)
    : m_serialManager(serialManager)
    , m_logSink(std::move(logSink))
    , m_eraseCalibration(eraseCalibration)
    , m_slaveId8bit(slaveId8bit) {}

QString DW9786Strategy::icModel() const {
    return QStringLiteral("DW9786");
}

QString DW9786Strategy::icDescription() const {
    return QStringLiteral("Dongwoon DW9786 OIS Driver");
}

void DW9786Strategy::log(LogLevel level, const QString &message) const {
    if (m_logSink) m_logSink(level, message);
}

// -----------------------------------------------------------------------------
// FlashStrategy::flash
// -----------------------------------------------------------------------------
bool DW9786Strategy::flash(const QByteArray &firmware,
                            std::function<void(qint64 sentBytes)> progress,
                            const std::atomic<bool> &cancelFlag,
                            QString *errorMessage) {
    auto setErr = [&](const QString &msg) {
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
    };

    if (m_serialManager == nullptr) {
        setErr(QStringLiteral("SerialManager 未注入"));
        return false;
    }

    // 与 AW / DW9788 一致：flash() 必须在 SerialManager 工作线程内调用，
    // 以支持 sendAndWaitResponse 的 fast-path（嵌套 event loop 路径要求同线程）
    Q_ASSERT_X(QThread::currentThread() == m_serialManager->thread(),
               "DW9786Strategy::flash",
               "must run in SerialManager worker thread (fast-path)");

    // firmware 已由 FirmwareParser::parseDw9786Hex 解析为 40960 字节小端二进制
    // (MCS_SIZE_W = 20480 uint16 = 40960 字节)。此处只做尺寸校验。
    constexpr int kFirmwareWords = static_cast<int>(MCS_SIZE_W);                       // 20480
    constexpr int kFirmwareBytes = kFirmwareWords * static_cast<int>(sizeof(uint16_t)); // 40960

    if (firmware.size() != kFirmwareBytes) {
        setErr(QStringLiteral("固件大小异常：得到 %1 字节，期望 %2 字节 (40KB)。"
                              "请确认选中的是 DW9786 hex 文件，并由 FirmwareParser "
                              "按 Dw9786Hex 格式解析为二进制。")
                   .arg(firmware.size()).arg(kFirmwareBytes));
        return false;
    }

    const qint64 progressTotal = firmware.size();  // 40KB
    // vendor 上报单位 = 千分位 (0..10000)；strategy 直接用千分位算字节，避免 1% 截断
    auto reportPctMilli = [&](int pct_milli) {
        if (!progress) return;
        const int clamped = qBound(0, pct_milli, 10000);
        progress(static_cast<qint64>(progressTotal) * clamped / 10000);
    };

    log(LogLevel::Info,
        QStringLiteral("DW9786 烧录开始 (slave=0x%1, mode=%2, firmware=%3 字节)")
            .arg(m_slaveId8bit, 2, 16, QLatin1Char('0'))
            .arg(m_eraseCalibration ? QStringLiteral("量产")
                                    : QStringLiteral("OTA (保留校准)"))
            .arg(progressTotal));
    reportPctMilli(0);

    if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); return false; }

    // 直接以 firmware buffer 作为 vendor 所需 uint16_t 数组（小端布局一致）
    const uint16_t *fwWords = reinterpret_cast<const uint16_t *>(firmware.constData());

    // -------- attach 桥接层 --------
    auto logToBridge = [this](const QString &s) {
        log(LogLevel::Info, s);
    };
    if (!Dw9786Bridge::attach(m_serialManager, logToBridge)) {
        setErr(QStringLiteral("DW9786 桥接层已被占用（OISReset 或另一次烧录进行中），本次烧录中止"));
        return false;
    }
    Dw9786Bridge::setCancelFlag(&cancelFlag);
    Dw9786Bridge::setProgressCallback(
        [&reportPctMilli](int pct_milli) { reportPctMilli(pct_milli); });

    bool success = false;
    auto cleanup = [&]() {
        Dw9786Bridge::setProgressCallback(nullptr);
        Dw9786Bridge::setCancelFlag(nullptr);
        Dw9786Bridge::detach();
    };

    do {
        // -------- vendor 序列 --------
        if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); break; }

        dw978x_ois_slvid(m_slaveId8bit);

        const int chipId = dw9786_id_check();
        if (chipId != DW9786_CHIP_ID) {
            setErr(QStringLiteral("Chip ID 不匹配 (读取=0x%1, 期望=0x9786)")
                       .arg(static_cast<uint16_t>(chipId), 4, 16, QLatin1Char('0')));
            break;
        }
        log(LogLevel::Info, QStringLiteral("Chip ID 验证通过 (0x9786)"));

        if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); break; }

        const int ret = dw9786_fw_download_with_buffer(fwWords, static_cast<uint32_t>(kFirmwareWords));
        if (ret == DW9786_FW_CANCELLED) {
            setErr(QStringLiteral("用户取消"));
            break;
        }
        if (ret == CHECKSUM_ERROR) {
            setErr(QStringLiteral("Checksum 不匹配"));
            break;
        }
        if (ret != FUNC_PASS) {
            setErr(QStringLiteral("DW9786 烧录失败 (ret=%1)").arg(ret));
            break;
        }

        // 启动 IC（vendor checksum 校验已在 _with_buffer 内部完成）
        dw9786_chip_enable(MODE_ON);
        dw9786_mcu_active(MODE_ON);

        dw9786_fw_info();   // 日志吐版本号
        dw9786_chip_info(); // 日志吐 chip 信息

        if (m_eraseCalibration) {
            log(LogLevel::Warn, QStringLiteral("量产模式：擦除模组校准数据"));
            if (dw9786_module_cal_erase() != FUNC_PASS) {
                setErr(QStringLiteral("module_cal_erase 失败"));
                break;
            }
        }

        log(LogLevel::Ok, QStringLiteral("DW9786 烧录完成"));
        reportPctMilli(10000);
        success = true;
    } while (false);

    cleanup();
    return success;
}
