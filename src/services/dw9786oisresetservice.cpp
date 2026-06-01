// =============================================================================
// @file    dw9786oisresetservice.cpp
// @brief   DW9786 上电 OISReset 服务实现
// =============================================================================
#include "services/dw9786oisresetservice.h"

#include "serialmanager.h"
#include "services/flashstrategies/dw9786_bridge.h"

#include <QThread>

// vendor 包装头放最后：与 dw9786_strategy.cpp / dw9786_bridge.cpp 同理——先让
// Qt/std 头展开，再引入 vendor 并 #undef 污染宏（Wait / round），同时统一
// vendor 符号 rename（RamWriteA / dw978x_ois_slvid 等）。
#include "services/flashstrategies/dw9786_vendor_include.h"

namespace {

// -----------------------------------------------------------------------------
// OISReset 寄存器序列（2026-05-31 用户口述权威序列）
//
// 注意：严禁与 vendor dw9786_api_ref.cpp 自带的 dw9786_reset() 混淆——那条序列
// 更短且缺少本序列的 0xE2FC / 0xE164 步骤。DW9786 上电默认 Sleep，本序列负责把
// IC 切到工作态，否则寄存器全读 0xFFFF / 不自举。
//
// 完整 9 步：
//   1. delay 10ms
//   2. 写 0xE000 = 0x0000
//   3. delay 10ms
//   4. 写 0xE000 = 0x0001
//   5. delay 5ms
//   6. 写 0xE2FC = 0xAC1E, 0xE164 = 0x0008, 0xE2FC = 0x0000
//   7. delay 1ms
//   8. 写 0xE004 = 0x0001
//   9. delay 100ms
// -----------------------------------------------------------------------------
namespace OisSeq {

// 寄存器地址
constexpr uint16_t kRegMode      = 0xE000;  // 模式寄存器（shutdown / standby）
constexpr uint16_t kRegPtKey     = 0xE2FC;  // protection key（解锁 / 上锁）
constexpr uint16_t kRegCfg       = 0xE164;  // 配置寄存器
constexpr uint16_t kRegMcuActive = 0xE004;  // MCU active

// 写入值
constexpr uint16_t kModeShutdown = 0x0000;
constexpr uint16_t kModeStandby  = 0x0001;
constexpr uint16_t kPtKeyUnlock  = 0xAC1E;
constexpr uint16_t kCfgValue     = 0x0008;
constexpr uint16_t kPtKeyLock    = 0x0000;
constexpr uint16_t kMcuActive    = 0x0001;

// 延时（毫秒）
constexpr unsigned long kDelayPreMs       = 10;   // 步骤 1
constexpr unsigned long kDelayAfterShutMs = 10;   // 步骤 3
constexpr unsigned long kDelayAfterStbyMs = 5;    // 步骤 5
constexpr unsigned long kDelayAfterCfgMs  = 1;    // 步骤 7
constexpr unsigned long kDelayFinalMs     = 100;  // 步骤 9

}  // namespace OisSeq

// I2C 透传单帧超时：单次寄存器写响应很快；此处收紧到 1000ms，限制单步失败时
// 工作线程被阻塞的最坏时长（默认 2000ms × 6 步过长）。
constexpr int kI2cTimeoutMs = 1000;

}  // namespace

Dw9786OisResetService::Dw9786OisResetService(SerialManager *serialManager,
                                             uint8_t slaveId8bit,
                                             QObject *parent)
    : QObject(parent)
    , m_serialManager(serialManager)
    , m_slaveId8bit(slaveId8bit) {}

void Dw9786OisResetService::requestOisReset() {
    if (m_serialManager == nullptr) {
        emit finished(false, QStringLiteral("SerialManager 未注入，OISReset 跳过"));
        return;
    }

    // 投递到 SerialManager 工作线程执行（与烧录 fast-path 相同的线程契约）：
    // RamWriteA → Dw9786Bridge → SerialManager::sendAndWaitResponse 必须在该线程
    // 内同步执行。fire-and-forget，不阻塞调用方（主线程）。
    QMetaObject::invokeMethod(
        m_serialManager, [this]() { runOnSerialThread(); }, Qt::QueuedConnection);
}

void Dw9786OisResetService::runOnSerialThread() {
    Q_ASSERT_X(QThread::currentThread() == m_serialManager->thread(),
               "Dw9786OisResetService::runOnSerialThread",
               "must run in SerialManager worker thread");

    // 日志封送：本函数在工作线程执行，emit 经队列连接回主线程显示。
    // 同时作为 Dw9786Bridge 的 logSink，桥接层的 I2C 超时 / 错误也会经此上报。
    auto logLine = [this](const QString &s) { emit logMessage(s); };

    logLine(QStringLiteral("[OISReset] 开始（DW9786 上电唤醒序列，slave=0x%1）")
                .arg(m_slaveId8bit, 2, 16, QLatin1Char('0')));

    // 注入 vendor I2C 透传函数指针（复用已验证的烧录写通道），并设置 OIS 从机地址。
    // attach 失败（桥接层已被烧录占用）时直接中止，不可与烧录并发覆盖 vendor 指针。
    if (!Dw9786Bridge::attach(m_serialManager, logLine, kI2cTimeoutMs)) {
        emit finished(false, QStringLiteral("OISReset 失败：DW9786 桥接层已被占用（可能正在烧录）"));
        return;
    }
    dw978x_ois_slvid(m_slaveId8bit);

    // ---- 严格按用户口述 9 步执行（顺序 / 数值 / 延时不得改动）----
    // 延时用 QThread::msleep（释放 CPU），不用 vendor m_delay() 的 busy-wait。
    using namespace OisSeq;
    QThread::msleep(kDelayPreMs);            // 步骤 1
    RamWriteA(kRegMode, kModeShutdown);      // 步骤 2: 0xE000 = 0x0000
    QThread::msleep(kDelayAfterShutMs);      // 步骤 3
    RamWriteA(kRegMode, kModeStandby);       // 步骤 4: 0xE000 = 0x0001
    QThread::msleep(kDelayAfterStbyMs);      // 步骤 5
    RamWriteA(kRegPtKey, kPtKeyUnlock);      // 步骤 6a: 0xE2FC = 0xAC1E
    RamWriteA(kRegCfg, kCfgValue);           // 步骤 6b: 0xE164 = 0x0008
    RamWriteA(kRegPtKey, kPtKeyLock);        // 步骤 6c: 0xE2FC = 0x0000
    QThread::msleep(kDelayAfterCfgMs);       // 步骤 7
    RamWriteA(kRegMcuActive, kMcuActive);    // 步骤 8: 0xE004 = 0x0001
    QThread::msleep(kDelayFinalMs);          // 步骤 9

    // 整条序列只调 vendor 写、不读 vendor 返回值；用 bridge 透传失败计数判定真实结果，
    // 避免任一步 I2C 超时/错误时仍误报"完成"，让用户误判 IC 已唤醒。
    const int transferErrors = Dw9786Bridge::transferErrorCount();
    Dw9786Bridge::detach();

    if (transferErrors > 0) {
        logLine(QStringLiteral("[OISReset] 失败：%1 次 I2C 透传未成功，IC 可能仍处 Sleep")
                    .arg(transferErrors));
        emit finished(false,
                      QStringLiteral("OISReset 失败：%1 次 I2C 透传未成功").arg(transferErrors));
        return;
    }

    logLine(QStringLiteral("[OISReset] 完成"));
    emit finished(true, QStringLiteral("OISReset 完成"));
}
