// =============================================================================
// @file    dw9788_strategy.cpp
// @brief   DW9788 (HL9788N) 烧录策略实现
// =============================================================================
#include "services/flashstrategies/dw9788_strategy.h"

#include "services/flashstrategies/hl9788n_bridge.h"
#include "serialmanager.h"

#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QThread>

#include <cstring>
#include <utility>

// vendor 包装头放最后：与 hl9788n_bridge.cpp 同理
#include "services/flashstrategies/hl9788n_vendor_include.h"

DW9788Strategy::DW9788Strategy(SerialManager *serialManager,
                                LogSink logSink,
                                bool eraseCalibration,
                                uint8_t slaveId8bit)
    : m_serialManager(serialManager)
    , m_logSink(std::move(logSink))
    , m_eraseCalibration(eraseCalibration)
    , m_slaveId8bit(slaveId8bit) {}

QString DW9788Strategy::icModel() const {
    return QStringLiteral("DW9788");
}

QString DW9788Strategy::icDescription() const {
    return QStringLiteral("Dongwoon HL9788N OIS/AF Driver");
}

void DW9788Strategy::log(LogLevel level, const QString &message) const {
    if (m_logSink) m_logSink(level, message);
}

// 注：原 DW9788Strategy::parseHl9788nHex 已下沉到 FirmwareParser::parseHl9788Hex
// （src/protocol/firmware_parser.cpp）。strategy 收到的 firmware 已是 65536 字节
// 小端二进制，直接 reinterpret_cast 即可传 vendor SDK。

// -----------------------------------------------------------------------------
// FlashStrategy::flash
// -----------------------------------------------------------------------------
bool DW9788Strategy::flash(const QByteArray &firmware,
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

    // 与 AW 一致：flash() 必须在 SerialManager 工作线程内调用，以支持
    // sendAndWaitResponse 的 fast-path（嵌套 event loop 路径要求同线程）
    Q_ASSERT_X(QThread::currentThread() == m_serialManager->thread(),
               "DW9788Strategy::flash",
               "must run in SerialManager worker thread (fast-path)");

    // firmware 已由 FirmwareParser::parseHl9788Hex 解析为 65536 字节小端二进制
    // (FW_SIZE = 64KB = 32768 uint16)。此处只做尺寸校验。
    constexpr int kFirmwareWords = static_cast<int>(FW_SIZE) / 2;          // 32768
    constexpr int kFirmwareBytes = kFirmwareWords * static_cast<int>(sizeof(uint16_t));  // 65536

    if (firmware.size() != kFirmwareBytes) {
        setErr(QStringLiteral("固件大小异常：得到 %1 字节，期望 %2 字节 (64KB)。"
                              "请确认选中的是 HL9788N hex 文件，并由 FirmwareParser "
                              "按 Hl9788Hex 格式解析为二进制。")
                   .arg(firmware.size()).arg(kFirmwareBytes));
        return false;
    }

    const qint64 progressTotal = firmware.size();  // 64KB
    auto reportPct = [&](int pct) {
        if (!progress) return;
        const int clamped = qBound(0, pct, 100);
        progress(static_cast<qint64>(progressTotal) * clamped / 100);
    };

    log(LogLevel::Info,
        QStringLiteral("HL9788N 烧录开始 (slave=0x%1, mode=%2, firmware=%3 字节)")
            .arg(m_slaveId8bit, 2, 16, QLatin1Char('0'))
            .arg(m_eraseCalibration ? QStringLiteral("量产")
                                    : QStringLiteral("OTA (保留校准)"))
            .arg(progressTotal));
    reportPct(0);

    if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); return false; }

    // 直接以 firmware buffer 作为 vendor 所需 uint16_t 数组（小端布局一致）
    const uint16_t *fwWords = reinterpret_cast<const uint16_t *>(firmware.constData());

    // -------- attach 桥接层 --------
    auto logToBridge = [this](const QString &s) {
        // vendor 端日志统一按 Info 级别上报
        log(LogLevel::Info, s);
    };
    if (!Hl9788nBridge::attach(m_serialManager, logToBridge)) {
        setErr(QStringLiteral("HL9788N 桥接层已被占用（另一次 DW 烧录进行中），本次烧录中止"));
        return false;
    }
    Hl9788nBridge::setCancelFlag(&cancelFlag);
    Hl9788nBridge::setProgressCallback(
        [&reportPct](int pct) { reportPct(pct); });

    bool success = false;
    auto cleanup = [&]() {
        Hl9788nBridge::setProgressCallback(nullptr);
        Hl9788nBridge::setCancelFlag(nullptr);
        Hl9788nBridge::detach();
    };

    do {
        // -------- 步骤 B：vendor 序列 --------
        if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); break; }

        hl978x_ois_slvid(m_slaveId8bit);

        const int chipId = hl9788n_id_check();
        if (chipId != HL9788N_CHIP_ID) {
            setErr(QStringLiteral("Chip ID 不匹配 (读取=0x%1, 期望=0x9788)")
                       .arg(static_cast<uint16_t>(chipId), 4, 16, QLatin1Char('0')));
            break;
        }
        log(LogLevel::Info, QStringLiteral("Chip ID 验证通过 (0x9788)"));

        if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); break; }

        const int dmaRet = hl9788n_fw_update_dma_with_buffer(fwWords, static_cast<uint32_t>(kFirmwareWords));
        if (dmaRet == HL9788N_FW_CANCELLED) {
            setErr(QStringLiteral("用户取消"));
            break;
        }
        if (dmaRet != FUNC_PASS) {
            setErr(QStringLiteral("DMA 烧录失败 (ret=%1)").arg(dmaRet));
            break;
        }

        hl9788n_ois_reset();
        hl9788n_fw_info();  // 仅打印版本，无返回

        if (m_eraseCalibration) {
            log(LogLevel::Warn, QStringLiteral("量产模式：擦除模组校准数据"));
            if (hl9788n_module_cal_erase() != FUNC_PASS) {
                setErr(QStringLiteral("module_cal_erase 失败"));
                break;
            }
            hl9788n_ois_reset();
        }

        const uint16_t rv = hl9788n_rv_status();
        constexpr uint16_t kCsOkIns = (1 << 12);
        if (!(rv & kCsOkIns)) {
            setErr(QStringLiteral("固件校验失败: %1")
                       .arg(Hl9788nBridge::describeRvStatus(rv)));
            break;
        }
        log(LogLevel::Ok, QStringLiteral("固件校验通过: %1")
                              .arg(Hl9788nBridge::describeRvStatus(rv)));

        reportPct(100);
        success = true;
    } while (false);

    cleanup();
    return success;
}
