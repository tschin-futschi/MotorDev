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

// -----------------------------------------------------------------------------
// DW 自定义 hex 文本解析（每行一个 32-bit hex 字符串）
//
// 行格式：8 个十六进制字符（可带前导/末尾空白），换行分隔。
// 拆分规则（与 vendor hl9788n_api_ref.cpp:752-753 保持一致）：
//   FW[2i]   = val32 & 0xFFFF          // low 16
//   FW[2i+1] = (val32 >> 16) & 0xFFFF  // high 16
//
// 容量说明：
//   vendor FW_SIZE = 0x10000 是固件"字节"总数（注释 "64KB"），不是 word 数。
//   vendor flash 主体把 2 段 × FW_HALF_SIZE 字节 = FW_SIZE 字节 烧到 IC。
//   uint16_t 元素数 = FW_SIZE / 2 = 32768 words；hex 文件行数 = words / 2 = 16384 行。
//   （vendor 静态缓冲 FW_DATA_HL9788N[FW_SIZE] 实际为 128KB，是 vendor 内部冗余；
//   实际烧到 IC 的只用前一半。）
//
// 空行忽略；非空白字符不全为 hex 时返回错误（含行号 + 内容片段）。
// -----------------------------------------------------------------------------
bool DW9788Strategy::parseHl9788nHex(const QByteArray &firmware,
                                      uint16_t *outWords,
                                      QString *errOut) const {
    constexpr int kExpectedWords = static_cast<int>(FW_SIZE) / 2;   // 32768 words = 64KB
    constexpr int kExpectedLines = kExpectedWords / 2;              // 16384 lines (each line = 2 words)

    if (firmware.isEmpty()) {
        if (errOut) *errOut = QStringLiteral("固件内容为空");
        return false;
    }

    int wordIdx = 0;
    int lineNo  = 0;          // 1-based 行号（含空行计数）
    int dataLineCount = 0;    // 非空数据行计数

    int pos = 0;
    const int total = firmware.size();
    while (pos < total) {
        // 找下一个换行
        int nl = firmware.indexOf('\n', pos);
        const int end = (nl < 0) ? total : nl;
        QByteArray line = firmware.mid(pos, end - pos);
        pos = (nl < 0) ? total : (nl + 1);
        ++lineNo;

        // 去掉 \r、首尾空白
        line = line.trimmed();
        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty()) continue;

        // 校验：所有字符必须是十六进制
        for (int i = 0; i < line.size(); ++i) {
            const char c = line.at(i);
            const bool hexOk = (c >= '0' && c <= '9') ||
                                (c >= 'A' && c <= 'F') ||
                                (c >= 'a' && c <= 'f');
            if (!hexOk) {
                if (errOut) {
                    const QString snippet = QString::fromLatin1(line.left(32));
                    *errOut = QStringLiteral("第 %1 行非法 hex 字符 '%2' (内容: \"%3\")")
                                  .arg(lineNo).arg(QChar(c)).arg(snippet);
                }
                return false;
            }
        }

        // 写两个 uint16_t
        if (wordIdx + 1 >= kExpectedWords) {
            if (errOut) {
                *errOut = QStringLiteral("第 %1 行：固件超出预期长度 (已读 %2 words，上限 %3)")
                              .arg(lineNo).arg(wordIdx).arg(kExpectedWords);
            }
            return false;
        }

        bool convOk = false;
        const quint32 val32 = static_cast<quint32>(line.toUInt(&convOk, 16));
        if (!convOk) {
            if (errOut) {
                const QString snippet = QString::fromLatin1(line.left(32));
                *errOut = QStringLiteral("第 %1 行 hex 转换失败 (内容: \"%2\")")
                              .arg(lineNo).arg(snippet);
            }
            return false;
        }

        outWords[wordIdx++] = static_cast<uint16_t>(val32 & 0xFFFFu);
        outWords[wordIdx++] = static_cast<uint16_t>((val32 >> 16) & 0xFFFFu);
        ++dataLineCount;
    }

    if (wordIdx != kExpectedWords) {
        if (errOut) {
            *errOut = QStringLiteral("数据行数不足：得到 %1 行 (%2 words)，预期 %3 行 (%4 words)")
                          .arg(dataLineCount).arg(wordIdx)
                          .arg(kExpectedLines).arg(kExpectedWords);
        }
        return false;
    }
    return true;
}

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

    if (firmware.isEmpty()) {
        setErr(QStringLiteral("固件内容为空"));
        return false;
    }

    const qint64 progressTotal = firmware.size();
    auto reportPct = [&](int pct) {
        if (!progress) return;
        const int clamped = qBound(0, pct, 100);
        progress(static_cast<qint64>(progressTotal) * clamped / 100);
    };

    log(LogLevel::Info,
        QStringLiteral("HL9788N 烧录开始 (slave=0x%1, mode=%2, hexBytes=%3)")
            .arg(m_slaveId8bit, 2, 16, QLatin1Char('0'))
            .arg(m_eraseCalibration ? QStringLiteral("量产")
                                    : QStringLiteral("OTA (保留校准)"))
            .arg(progressTotal));
    reportPct(0);

    // -------- 步骤 A：解析 hex --------
    if (cancelFlag.load()) { setErr(QStringLiteral("用户取消")); return false; }

    // 实际固件数据 = FW_SIZE / 2 = 32768 words = 64KB；分配 64KB 堆缓冲
    constexpr int kFirmwareWords = static_cast<int>(FW_SIZE) / 2;
    QByteArray buf(kFirmwareWords * static_cast<int>(sizeof(uint16_t)), 0);
    uint16_t *fwWords = reinterpret_cast<uint16_t *>(buf.data());

    QString parseErr;
    if (!parseHl9788nHex(firmware, fwWords, &parseErr)) {
        setErr(QStringLiteral("固件解析失败: %1").arg(parseErr));
        return false;
    }
    log(LogLevel::Info, QStringLiteral("固件解析完成 (%1 words = %2 KB)")
                            .arg(kFirmwareWords)
                            .arg(kFirmwareWords * 2 / 1024));

    // -------- attach 桥接层 --------
    auto logToBridge = [this](const QString &s) {
        // vendor 端日志统一按 Info 级别上报
        log(LogLevel::Info, s);
    };
    Hl9788nBridge::attach(m_serialManager, logToBridge);
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
