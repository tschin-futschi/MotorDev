// =============================================================================
// @file    aw_local_isp_strategy.cpp
// @brief   AW 本地 ISP 烧录策略基类实现
// =============================================================================
#include "services/flashstrategies/aw_local_isp_strategy.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QByteArray>
#include <QDateTime>
#include <QThread>

#include <chrono>
#include <utility>

namespace {

constexpr int      kDataTimeoutMs       = 5000;          ///< 单包 DATA 等响应超时
constexpr int      kExecTimeoutMs       = 15000;         ///< EXEC 等响应超时(协议 5-10s,留余量)
constexpr int      kBeginTimeoutMs      = 2000;          ///< BEGIN 等响应超时
constexpr int      kShortTimeoutMs      = 2000;          ///< CANCEL / RESET_CHIP 收尾超时
constexpr int      kChunkSize           = 252;           ///< 单帧 DATA chunk 字节数(< 253 留 margin)
constexpr quint32  kMaxFirmwareBytes    = 64U * 1024U;   ///< STM32 端 SRAM1 缓冲上限
constexpr int      kProgressMinInterval = 16;            ///< 进度上报最小间隔 ms(~60Hz)
constexpr qint64   kProgressMinBytes    = 1024;          ///< 进度上报最小累积字节
constexpr quint32  kFlashTopExclusive   = 0x01020000U;   ///< AW_FLASH_TOP(不含)

}  // namespace

AwLocalIspStrategy::AwLocalIspStrategy(SerialManager *serialManager, LogSink logSink)
    : m_serialManager(serialManager)
    , m_logSink(std::move(logSink)) {}

// -----------------------------------------------------------------------------
// FlashStrategy::flash 实现
// -----------------------------------------------------------------------------

bool AwLocalIspStrategy::flash(const QByteArray &firmware,
                                std::function<void(qint64 sentBytes)> progress,
                                const std::atomic<bool> &cancelFlag,
                                QString *errorMessage) {
    m_progress = std::move(progress);
    m_lastReportedBytes = 0;
    m_lastReportTimeMs = QDateTime::currentMSecsSinceEpoch();

    if (m_serialManager == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("SerialManager 未注入");
        log(LogLevel::Error, *errorMessage);
        return false;
    }

    // Fast-path 线程断言:必须在 SerialManager 工作线程内调用
    Q_ASSERT_X(QThread::currentThread() == m_serialManager->thread(),
               "AwLocalIspStrategy::flash",
               "must run in SerialManager worker thread (fast-path)");

    // -------- 入口校验 --------
    const qsizetype size = firmware.size();
    if (size == 0) {
        if (errorMessage) *errorMessage = QStringLiteral("固件内容为空");
        log(LogLevel::Error, *errorMessage);
        return false;
    }
    if (size > static_cast<qsizetype>(kMaxFirmwareBytes)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("固件大小 %1 字节超过 AW 本地 ISP 上限 %2 字节(64 KB)")
                                .arg(size).arg(kMaxFirmwareBytes);
        }
        log(LogLevel::Error, *errorMessage);
        return false;
    }
    if ((size % 4) != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("固件大小 %1 字节非 4 字节对齐,无法写入 IC Flash")
                                .arg(size);
        }
        log(LogLevel::Error, *errorMessage);
        return false;
    }

    const quint32 addr = flashTargetAddr();
    if ((static_cast<quint32>(size) > kFlashTopExclusive - addr)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("固件大小超出目标 Flash 区间(addr=0x%1, 剩余 %2 字节)")
                                .arg(addr, 8, 16, QLatin1Char('0'))
                                .arg(kFlashTopExclusive - addr);
        }
        log(LogLevel::Error, *errorMessage);
        return false;
    }

    log(LogLevel::Info,
        QStringLiteral("开始烧录:addr=0x%1, totalBytes=%2 (%3 KB)")
            .arg(addr, 8, 16, QLatin1Char('0'))
            .arg(size)
            .arg(QString::number(size / 1024.0, 'f', 1)));

    if (m_progress) m_progress(0);

    auto failCleanup = [this]() {
        log(LogLevel::Warn, QStringLiteral("收尾:发 RESET_CHIP + CANCEL"));
        doResetChip();   // 让 IC 退出 UBOOT(若已进入),失败也忽略
        doCancel();      // 重置 STM32 session 状态
    };

    // -------- 0x32 BEGIN --------
    if (cancelFlag.load()) {
        if (errorMessage) *errorMessage = QStringLiteral("用户取消");
        return false;
    }
    if (!doBegin(addr, static_cast<quint32>(size), errorMessage)) {
        failCleanup();
        return false;
    }

    // -------- 0x33 DATA 循环 --------
    if (!doData(firmware, m_progress, cancelFlag, errorMessage)) {
        failCleanup();
        return false;
    }

    // -------- 0x34 EXEC --------
    if (cancelFlag.load()) {
        if (errorMessage) *errorMessage = QStringLiteral("用户取消");
        failCleanup();
        return false;
    }
    log(LogLevel::Info, QStringLiteral("阶段:烧录执行中... (5-10 秒,无中间进度)"));

    uint8_t ispStatus = 0;
    if (!doExec(ispStatus, errorMessage)) {
        failCleanup();
        return false;
    }
    if (ispStatus != static_cast<uint8_t>(MotorProtocol::AwIspStatus::Ok)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ISP 烧录失败:%1 (status=%2)")
                                .arg(QString::fromLatin1(MotorProtocol::awIspStatusName(ispStatus)))
                                .arg(ispStatus);
        }
        log(LogLevel::Error, *errorMessage);
        failCleanup();
        return false;
    }

    log(LogLevel::Ok, QStringLiteral("烧录完成,IC 已由 STM32 端 reset 至运行态"));
    return true;
}

// -----------------------------------------------------------------------------
// 子流程
// -----------------------------------------------------------------------------

bool AwLocalIspStrategy::doBegin(quint32 addr, quint32 totalBytes, QString *err) {
    const QByteArray payload = MotorProtocol::encodeFlashBegin(addr, totalBytes);
    QByteArray resp;
    log(LogLevel::Info, QStringLiteral("BEGIN: addr=0x%1 totalBytes=%2")
                            .arg(addr, 8, 16, QLatin1Char('0')).arg(totalBytes));
    if (!sendCmd(MotorProtocol::CmdFlashBegin, payload, resp,
                 kBeginTimeoutMs, "BEGIN", err)) {
        return false;
    }
    return true;
}

bool AwLocalIspStrategy::doData(const QByteArray &firmware,
                                 const std::function<void(qint64)> &progress,
                                 const std::atomic<bool> &cancelFlag,
                                 QString *err) {
    const qsizetype total = firmware.size();
    qsizetype offset = 0;
    quint16 pktSeq = 0;
    const auto t0 = std::chrono::steady_clock::now();

    while (offset < total) {
        if (cancelFlag.load()) {
            if (err) *err = QStringLiteral("用户取消");
            return false;
        }
        const int n = static_cast<int>(qMin<qsizetype>(kChunkSize, total - offset));
        const QByteArray chunk = firmware.mid(static_cast<int>(offset), n);
        const QByteArray payload = MotorProtocol::encodeFlashData(pktSeq, chunk);

        QByteArray resp;
        if (!sendCmd(MotorProtocol::CmdFlashData, payload, resp,
                     kDataTimeoutMs, "DATA", err)) {
            if (err) {
                *err = QStringLiteral("DATA pktSeq=%1 offset=%2/%3 失败:%4")
                            .arg(pktSeq).arg(offset + n).arg(total).arg(*err);
            }
            return false;
        }

        quint16 nextSeq = 0;
        if (!MotorProtocol::decodeFlashDataResponse(resp, &nextSeq)) {
            if (err) {
                *err = QStringLiteral("DATA pktSeq=%1 响应载荷长度不足(%2 B)")
                            .arg(pktSeq).arg(resp.size());
            }
            log(LogLevel::Error, *err);
            return false;
        }
        if (nextSeq != static_cast<quint16>(pktSeq + 1)) {
            if (err) {
                *err = QStringLiteral("DATA nextSeq 不匹配:预期 %1,实际 %2(pktSeq=%3)")
                            .arg(pktSeq + 1).arg(nextSeq).arg(pktSeq);
            }
            log(LogLevel::Error, *err);
            return false;
        }

        offset += n;
        pktSeq = static_cast<quint16>(pktSeq + 1);
        notifyProgress(static_cast<qint64>(offset));
        (void)progress;  // notifyProgress 已经持有 m_progress
    }

    const auto t1 = std::chrono::steady_clock::now();
    const qint64 elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    log(LogLevel::Info, QStringLiteral("DATA 阶段完成:%1 包,共 %2 字节,耗时 %3 ms")
                            .arg(pktSeq).arg(total).arg(elapsedMs));
    // DATA 末尾无条件推一次 total,绕过 notifyProgress 的节流(16ms / 1024B 双阈值)。
    // 小固件最后一包可能不满阈值导致漏推,UI 端会卡在 99.x% 直到 EXEC 完成;
    // 在两阶段进度模型下这表现为 DATA 段(0~20%)末尾不抵达 20%。强制推一次精确收口。
    if (m_progress) {
        m_progress(static_cast<qint64>(total));
        m_lastReportedBytes = static_cast<qint64>(total);
    }
    return true;
}

bool AwLocalIspStrategy::doExec(uint8_t &ispStatusOut, QString *err) {
    QByteArray resp;
    if (!sendCmd(MotorProtocol::CmdFlashExec, {}, resp,
                 kExecTimeoutMs, "EXEC", err)) {
        return false;
    }
    if (!MotorProtocol::decodeFlashExecResponse(resp, &ispStatusOut)) {
        if (err) *err = QStringLiteral("EXEC 响应载荷长度不足(%1 B)").arg(resp.size());
        log(LogLevel::Error, *err);
        return false;
    }
    log(LogLevel::Info, QStringLiteral("EXEC 返回:%1 (status=%2)")
                            .arg(QString::fromLatin1(MotorProtocol::awIspStatusName(ispStatusOut)))
                            .arg(ispStatusOut));
    return true;
}

void AwLocalIspStrategy::doResetChip() {
    QByteArray resp;
    QString err;
    if (!sendCmd(MotorProtocol::CmdFlashResetChip, {}, resp,
                 kShortTimeoutMs, "RESET_CHIP", &err)) {
        log(LogLevel::Warn, QStringLiteral("RESET_CHIP 失败(忽略):%1").arg(err));
        return;
    }
    uint8_t st = 0;
    if (MotorProtocol::decodeFlashExecResponse(resp, &st) && st != 0) {
        log(LogLevel::Warn, QStringLiteral("RESET_CHIP 返回非 OK:%1 (status=%2)")
                                .arg(QString::fromLatin1(MotorProtocol::awIspStatusName(st)))
                                .arg(st));
    }
}

void AwLocalIspStrategy::doCancel() {
    QByteArray resp;
    QString err;
    if (!sendCmd(MotorProtocol::CmdFlashCancel, {}, resp,
                 kShortTimeoutMs, "CANCEL", &err)) {
        log(LogLevel::Warn, QStringLiteral("CANCEL 失败(忽略):%1").arg(err));
    }
}

// -----------------------------------------------------------------------------
// 通用发送 + 等响应
// -----------------------------------------------------------------------------

bool AwLocalIspStrategy::sendCmd(uint8_t cmd,
                                  const QByteArray &payload,
                                  QByteArray &outData,
                                  int timeoutMs,
                                  const char *stageName,
                                  QString *err) {
    uint8_t outCmd = 0;
    uint8_t outSeq = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = m_serialManager->sendAndWaitResponse(
        cmd, payload, outCmd, outSeq, outData, timeoutMs);
    const auto t1 = std::chrono::steady_clock::now();
    const qint64 elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (!ok) {
        if (err) {
            *err = QStringLiteral("[%1] 超时或链路失败(timeout=%2 ms, elapsed=%3 ms)")
                        .arg(QLatin1String(stageName)).arg(timeoutMs).arg(elapsedMs);
        }
        log(LogLevel::Error, *err);
        return false;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        if (err) {
            *err = QStringLiteral("[%1] STM32 错误响应:0x%2")
                        .arg(QLatin1String(stageName))
                        .arg(code, 2, 16, QLatin1Char('0'));
        }
        log(LogLevel::Error, *err);
        return false;
    }
    if (outCmd != cmd) {
        if (err) {
            *err = QStringLiteral("[%1] 响应命令码不匹配:预期 0x%2,实际 0x%3")
                        .arg(QLatin1String(stageName))
                        .arg(cmd, 2, 16, QLatin1Char('0'))
                        .arg(outCmd, 2, 16, QLatin1Char('0'));
        }
        log(LogLevel::Error, *err);
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// 进度节流 / 日志
// -----------------------------------------------------------------------------

void AwLocalIspStrategy::notifyProgress(qint64 sentBytes) {
    if (!m_progress) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool intervalElapsed = (now - m_lastReportTimeMs) >= kProgressMinInterval;
    const bool bytesAccumulated = (sentBytes - m_lastReportedBytes) >= kProgressMinBytes;
    if (intervalElapsed || bytesAccumulated) {
        m_progress(sentBytes);
        m_lastReportedBytes = sentBytes;
        m_lastReportTimeMs = now;
    }
}

void AwLocalIspStrategy::log(LogLevel level, const QString &message) const {
    if (m_logSink) m_logSink(level, message);
}
