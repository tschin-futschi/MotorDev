// =============================================================================
// @file    aw_sdk_strategy.cpp
// @brief   Awinic SDK 烧录策略共用基类实现
// =============================================================================
#include "services/flashstrategies/aw_sdk_strategy.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QThread>

#include <chrono>
#include <cstring>
#include <memory>
#include <utility>

namespace {

constexpr int kI2cTimeoutMs = 5000;          ///< 单次 I2C 透传等待超时
constexpr int kProgressMinIntervalMs = 16;   ///< 进度上报最小间隔（节流到 ~60Hz）
constexpr qint64 kProgressMinBytes = 1024;   ///< 进度上报最小字节增量（防止超低速时不触发）
constexpr int kBootcontrolTimes = 20;        ///< AwBootcontrol 第 1 参（厂家给定默认）
constexpr int kBootcontrolDelay = 7;         ///< AwBootcontrol 第 2 参（厂家给定默认）
constexpr int kDLNumType_0 = 0;              ///< AwMoveBinDownload 第 3 参（16 word per pack，用户决议）

}  // namespace

thread_local AwSdkStrategy *AwSdkStrategy::s_currentInstance = nullptr;

// -----------------------------------------------------------------------------
// 构造 / 析构
// -----------------------------------------------------------------------------

AwSdkStrategy::AwSdkStrategy(SerialManager *serialManager, LogSink logSink, AddrProvider addrProvider)
    : m_serialManager(serialManager)
    , m_logSink(std::move(logSink))
    , m_addrProvider(std::move(addrProvider)) {
}

AwSdkStrategy::~AwSdkStrategy() {
    if (m_library.isLoaded()) m_library.unload();
}

// -----------------------------------------------------------------------------
// FlashStrategy::flash 实现 — 5 步流程编排
// -----------------------------------------------------------------------------

bool AwSdkStrategy::flash(const QByteArray &firmware,
                          std::function<void(qint64 sentBytes)> progress,
                          const std::atomic<bool> &cancelFlag,
                          QString *errorMessage) {
    // 状态复位
    m_cancelFlag = &cancelFlag;
    m_progress = std::move(progress);
    m_sentBytes = 0;
    m_lastReportedBytes = 0;
    m_lastReportTimeMs = QDateTime::currentMSecsSinceEpoch();
    m_inDownload = false;
    m_initCalled = false;
    m_bootEntered = false;

    s_currentInstance = this;

    auto bail = [&](bool ok) -> bool {
        if (!ok) emergencyCleanup();
        s_currentInstance = nullptr;
        m_cancelFlag = nullptr;
        m_progress = nullptr;
        return ok;
    };

    auto checkCancel = [&]() -> bool {
        if (m_cancelFlag != nullptr && m_cancelFlag->load()) {
            if (errorMessage) *errorMessage = QStringLiteral("用户取消");
            return true;
        }
        return false;
    };

    // 0. 加载 DLL
    if (!ensureDllLoaded(errorMessage)) return bail(false);

    // 1. AwOIS_ExtFuncInit
    if (checkCancel()) return bail(false);
    if (!doInit(errorMessage)) return bail(false);

    // 1.5 把用户 configtab 配置的 IC 7-bit 地址同步给 DLL（必须在 Init 之后、Bootcontrol 之前）
    if (!applySlaveAddress(errorMessage)) return bail(false);

    // 2. AwBootcontrol
    if (checkCancel()) return bail(false);
    if (!doBootcontrol(errorMessage)) return bail(false);

    // 3. AwMoveBinDownload
    if (checkCancel()) return bail(false);
    if (!doDownload(firmware, errorMessage)) return bail(false);

    // 4. AwResetChip
    if (!doResetChip(errorMessage)) return bail(false);

    // 5. AW_OIS_ExtFuncDeInit
    doDeInit();

    s_currentInstance = nullptr;
    m_cancelFlag = nullptr;
    m_progress = nullptr;
    return true;
}

void AwSdkStrategy::emergencyCleanup() {
    // 失败 / 取消时收尾：强制 AwResetChip 让 IC 退出 boot 模式，再 DeInit 释放 DLL 资源；
    // 收尾本身的失败不覆盖原始 errorMessage，仅追加日志。
    if (m_bootEntered && m_fnResetChip != nullptr) {
        log(LogLevel::Info, QStringLiteral("收尾：调用 AwResetChip()"));
        const int ret = m_fnResetChip();
        if (ret != 0) {
            log(LogLevel::Warn, QStringLiteral("收尾 AwResetChip 返回 %1（忽略）").arg(ret));
        }
        m_bootEntered = false;
    }
    if (m_initCalled) {
        log(LogLevel::Info, QStringLiteral("收尾：调用 AW_OIS_ExtFuncDeInit()"));
        doDeInit();
    }
}

// -----------------------------------------------------------------------------
// DLL 加载与符号 resolve
// -----------------------------------------------------------------------------

bool AwSdkStrategy::ensureDllLoaded(QString *errorMessage) {
    // dllPath() 仅返回文件名（如 "AW86100.dll"）。在此与 exe 所在目录拼成绝对路径，
    // 让 LoadLibrary 不依赖 CWD/系统默认搜索顺序，避免从 IDE/快捷方式启动时找不到 DLL。
    const QString name = dllPath();
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString absPath = QDir(exeDir).absoluteFilePath(name);
    m_library.setFileName(absPath);
    if (!m_library.load()) {
        const QString reason = m_library.errorString();
        const bool fileExists = QFileInfo::exists(absPath);
        const QString detail = fileExists
            ? QStringLiteral("文件存在但加载失败（疑似依赖缺失/架构不匹配/损坏）")
            : QStringLiteral("文件不存在于该路径");
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to load %1: %2 [path=%3, exists=%4]")
                                .arg(name, reason, absPath,
                                     fileExists ? QStringLiteral("true") : QStringLiteral("false"));
        }
        log(LogLevel::Error,
            QStringLiteral("加载 DLL 失败：%1（%2）").arg(name, reason));
        log(LogLevel::Error,
            QStringLiteral("  完整路径：%1").arg(absPath));
        log(LogLevel::Error,
            QStringLiteral("  exe 目录：%1").arg(exeDir));
        log(LogLevel::Error,
            QStringLiteral("  诊断：%1").arg(detail));
        return false;
    }
    log(LogLevel::Info, QStringLiteral("已加载 DLL：%1").arg(absPath));

    auto resolveOrFail = [&](const char *fn, void **out) -> bool {
        QFunctionPointer p = m_library.resolve(fn);
        if (p == nullptr) {
            if (errorMessage) *errorMessage = QStringLiteral("DLL 缺少导出符号：%1").arg(fn);
            log(LogLevel::Error, QStringLiteral("DLL 缺少导出符号：%1").arg(fn));
            return false;
        }
        *out = reinterpret_cast<void *>(p);
        return true;
    };

    void *p = nullptr;
    if (!resolveOrFail("AwOIS_ExtFuncInit", &p)) return false;
    m_fnInit = reinterpret_cast<FnExtFuncInit>(p);

    if (!resolveOrFail("AW_OIS_ExtFuncDeInit", &p)) return false;
    m_fnDeInit = reinterpret_cast<FnExtFuncDeInit>(p);

    if (!resolveOrFail("AwBootcontrol", &p)) return false;
    m_fnBootcontrol = reinterpret_cast<FnBootcontrol>(p);

    if (!resolveOrFail("AwMoveBinDownload", &p)) return false;
    m_fnMoveBinDownload = reinterpret_cast<FnMoveBinDownload>(p);

    if (!resolveOrFail("AwResetChip", &p)) return false;
    m_fnResetChip = reinterpret_cast<FnResetChip>(p);

    if (!resolveOrFail("AwSet7bitI2CSlaveAddr", &p)) return false;
    m_fnSetSlaveAddr = reinterpret_cast<FnSetSlaveAddr>(p);

    return true;
}

// -----------------------------------------------------------------------------
// 5 步流程
// -----------------------------------------------------------------------------

bool AwSdkStrategy::doInit(QString *errorMessage) {
    DllExtFuncList list = {
        &AwSdkStrategy::extReadI2cThunk,
        &AwSdkStrategy::extWriteI2cThunk,
        &AwSdkStrategy::extOutputLogThunk,
    };
    log(LogLevel::Info, QStringLiteral("步骤 1/5：AwOIS_ExtFuncInit"));
    const int ret = m_fnInit(&list);
    if (ret != 0) {
        const QString msg = QStringLiteral("AwOIS_ExtFuncInit failed (return %1)").arg(ret);
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
        return false;
    }
    m_initCalled = true;
    return true;
}

bool AwSdkStrategy::doBootcontrol(QString *errorMessage) {
    log(LogLevel::Info, QStringLiteral("步骤 2/5：AwBootcontrol(%1, %2)")
                            .arg(kBootcontrolTimes).arg(kBootcontrolDelay));
    const int ret = m_fnBootcontrol(kBootcontrolTimes, kBootcontrolDelay);
    if (ret != 0) {
        const QString msg = QStringLiteral("AwBootcontrol failed (return %1)").arg(ret);
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
        return false;
    }
    m_bootEntered = true;
    return true;
}

bool AwSdkStrategy::doDownload(QByteArray firmware, QString *errorMessage) {
    log(LogLevel::Info, QStringLiteral("步骤 3/5：AwMoveBinDownload (%1 bytes, DL_NumType_0)")
                            .arg(firmware.size()));
    m_inDownload = true;
    if (m_progress) m_progress(0);

    DllByte *buf = reinterpret_cast<DllByte *>(firmware.data());
    const DllUInt32 len = static_cast<DllUInt32>(firmware.size());
    const int ret = m_fnMoveBinDownload(buf, len, static_cast<DllInt>(kDLNumType_0));
    m_inDownload = false;

    // 取消优先：DLL 即便返回失败，也归类为用户取消
    if (m_cancelFlag != nullptr && m_cancelFlag->load()) {
        if (errorMessage) *errorMessage = QStringLiteral("用户取消");
        return false;
    }
    if (ret != 0) {
        const QString msg = QStringLiteral("AwMoveBinDownload failed (return %1)").arg(ret);
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
        return false;
    }
    return true;
}

bool AwSdkStrategy::doResetChip(QString *errorMessage) {
    log(LogLevel::Info, QStringLiteral("步骤 4/5：AwResetChip"));
    const int ret = m_fnResetChip();
    if (ret != 0) {
        const QString msg = QStringLiteral("AwResetChip failed (return %1)").arg(ret);
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
        return false;
    }
    m_bootEntered = false;
    return true;
}

bool AwSdkStrategy::applySlaveAddress(QString *errorMessage) {
    const uint8_t addr = m_addrProvider ? m_addrProvider() : uint8_t(0);
    if (addr == 0 || addr > 0x7F) {
        const QString msg = QStringLiteral(
            "IC 7-bit I2C 从机地址未配置或非法（0x%1），请先在配置 Tab 选择 IC 类型并写入 SlaveID")
            .arg(addr, 2, 16, QLatin1Char('0'));
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
        return false;
    }
    log(LogLevel::Info, QStringLiteral("设置 DLL I2C 7-bit 从机地址：0x%1")
                            .arg(addr, 2, 16, QLatin1Char('0')));
    const int ret = m_fnSetSlaveAddr(static_cast<DllByte>(addr));
    if (ret != 0) {
        const QString msg = QStringLiteral("AwSet7bitI2CSlaveAddr failed (return %1)").arg(ret);
        if (errorMessage) *errorMessage = msg;
        log(LogLevel::Error, msg);
        return false;
    }
    return true;
}

void AwSdkStrategy::doDeInit() {
    if (m_fnDeInit == nullptr) return;
    log(LogLevel::Info, QStringLiteral("步骤 5/5：AW_OIS_ExtFuncDeInit"));
    const int ret = m_fnDeInit();
    if (ret != 0) {
        log(LogLevel::Warn, QStringLiteral("AW_OIS_ExtFuncDeInit 返回 %1（忽略）").arg(ret));
    }
    m_initCalled = false;
}

// -----------------------------------------------------------------------------
// C-style thunk
// -----------------------------------------------------------------------------

int AwSdkStrategy::extReadI2cThunk(DllByte devId, DllByte addrSize, DllByte *pAddr,
                                    DllByte rdSize, DllByte *pRdBuf) {
    AwSdkStrategy *inst = s_currentInstance;
    if (inst == nullptr) return -1;
    return inst->onReadI2c(devId, addrSize, pAddr, rdSize, pRdBuf);
}

int AwSdkStrategy::extWriteI2cThunk(DllByte devId, DllByte wrSize, DllByte *wrData) {
    AwSdkStrategy *inst = s_currentInstance;
    if (inst == nullptr) return -1;
    return inst->onWriteI2c(devId, wrSize, wrData);
}

int AwSdkStrategy::extOutputLogThunk(const char *str) {
    AwSdkStrategy *inst = s_currentInstance;
    if (inst == nullptr) return 0;
    return inst->onOutputLog(str);
}

// -----------------------------------------------------------------------------
// thunk 路由后的实例方法
// -----------------------------------------------------------------------------

int AwSdkStrategy::onReadI2c(DllByte devId, DllByte addrSize, DllByte *pAddr,
                              DllByte rdSize, DllByte *pRdBuf) {
    if (m_cancelFlag != nullptr && m_cancelFlag->load()) return -1;

    // 进入日志带 DLL 传入参数 + 起始时间戳；download 阶段量大不打，避免淹没日志/拖慢 GUI
    const bool verbose = !m_inDownload;
    const auto t0 = std::chrono::steady_clock::now();
    if (verbose) {
        const QByteArray addrBa = (addrSize > 0 && pAddr != nullptr)
            ? QByteArray(reinterpret_cast<const char *>(pAddr), addrSize)
            : QByteArray();
        log(LogLevel::Info,
            QStringLiteral("[I2C-R ENTER] devId=0x%1 addrSize=%2 addr=%3 rdSize=%4")
                .arg(devId, 2, 16, QLatin1Char('0'))
                .arg(addrSize)
                .arg(addrBa.isEmpty() ? QStringLiteral("<empty>")
                                       : QString::fromLatin1(addrBa.toHex(' ')).toUpper())
                .arg(rdSize));
    }

    const int rc = syncI2cRead(devId, addrSize, pAddr, rdSize, pRdBuf, kI2cTimeoutMs);

    if (verbose) {
        const auto t1 = std::chrono::steady_clock::now();
        const qint64 elapsedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        log(LogLevel::Info,
            QStringLiteral("[I2C-R EXIT ] devId=0x%1 rc=%2 elapsed=%3us")
                .arg(devId, 2, 16, QLatin1Char('0'))
                .arg(rc)
                .arg(elapsedUs));
    }
    return rc;
}

int AwSdkStrategy::onWriteI2c(DllByte devId, DllByte wrSize, DllByte *wrData) {
    if (m_cancelFlag != nullptr && m_cancelFlag->load()) return -1;

    // 进入日志带 DLL 传入参数 + 起始时间戳；download 阶段量大不打，避免淹没日志/拖慢 GUI
    const bool verbose = !m_inDownload;
    const auto t0 = std::chrono::steady_clock::now();
    if (verbose) {
        const int dumpLen = qMin<int>(wrSize, 16);
        const QByteArray dataBa = (wrSize > 0 && wrData != nullptr)
            ? QByteArray(reinterpret_cast<const char *>(wrData), dumpLen)
            : QByteArray();
        log(LogLevel::Info,
            QStringLiteral("[I2C-W ENTER] devId=0x%1 wrSize=%2 data=%3%4")
                .arg(devId, 2, 16, QLatin1Char('0'))
                .arg(wrSize)
                .arg(dataBa.isEmpty() ? QStringLiteral("<empty>")
                                       : QString::fromLatin1(dataBa.toHex(' ')).toUpper())
                .arg(wrSize > 16 ? QStringLiteral("...(+%1 bytes)").arg(wrSize - 16) : QString()));
    }

    // I2C 写时寄存器地址与数据由 DLL 拼成单个 wrData 一并交给我们；
    // 透传协议 AddrSize 恒为 0，STM32 端在软件位拼 I2C 总线上以单次 transaction
    // 发送（START → addr+W → wrData 整段 → STOP），不解析或重组。
    const int rc = syncI2cWrite(devId, /*addrSize=*/0, /*addr=*/nullptr,
                                 wrSize, wrData, kI2cTimeoutMs);
    if (rc == 0 && m_inDownload) {
        notifyProgress(static_cast<qint64>(wrSize));
    }

    if (verbose) {
        const auto t1 = std::chrono::steady_clock::now();
        const qint64 elapsedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        log(LogLevel::Info,
            QStringLiteral("[I2C-W EXIT ] devId=0x%1 rc=%2 elapsed=%3us")
                .arg(devId, 2, 16, QLatin1Char('0'))
                .arg(rc)
                .arg(elapsedUs));
    }
    return rc;
}

int AwSdkStrategy::onOutputLog(const char *str) {
    if (str == nullptr) return 0;
    log(LogLevel::Info, QStringLiteral("[DLL] %1").arg(QString::fromUtf8(str)));
    return 0;
}

// -----------------------------------------------------------------------------
// 同步 I2C 透传（fast-path：strategy 在 SerialManager 工作线程内执行，
// 直接调 SerialManager::sendAndWaitResponse，全程同步、零跨线程、零跨主线程）
// -----------------------------------------------------------------------------

int AwSdkStrategy::syncI2cWrite(uint8_t devId, uint8_t addrSize, const uint8_t *addr,
                                 uint8_t dataLen, const uint8_t *data, int timeoutMs) {
    Q_ASSERT_X(devId < 0x80, "AwSdkStrategy::syncI2cWrite",
               "devId must be 7-bit (0x00-0x7F), R/W bit not allowed");
    if (m_serialManager == nullptr) {
        log(LogLevel::Error, QStringLiteral("SerialManager 未注入，无法发送 I2C 透传"));
        return -1;
    }
    Q_ASSERT_X(QThread::currentThread() == m_serialManager->thread(),
               "AwSdkStrategy::syncI2cWrite",
               "must run in SerialManager worker thread (fast-path)");

    const bool verbose = !m_inDownload;
    const auto t0 = std::chrono::steady_clock::now();

    const QByteArray payload =
        MotorProtocol::encodeI2cTransferWrite(devId, addr, addrSize, data, dataLen);

    uint8_t outCmd = 0, outSeq = 0;
    QByteArray outData;
    const bool ok = m_serialManager->sendAndWaitResponse(
        MotorProtocol::CmdI2cTransferWrite, payload, outCmd, outSeq, outData, timeoutMs);

    if (verbose) {
        const auto t1 = std::chrono::steady_clock::now();
        const qint64 elapsedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        log(LogLevel::Info,
            QStringLiteral("[I2C-W TIMING] total=%1us  (single-thread fast-path)").arg(elapsedUs));
    }

    if (!ok) {
        log(LogLevel::Error, QStringLiteral("I2C 透传写超时或链路失败（%1 ms）").arg(timeoutMs));
        return -1;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        log(LogLevel::Error, QStringLiteral("I2C 透传写失败：STM32 error 0x%1")
                                .arg(code, 2, 16, QLatin1Char('0')));
        return -1;
    }
    return 0;
}

int AwSdkStrategy::syncI2cRead(uint8_t devId, uint8_t addrSize, const uint8_t *addr,
                                uint8_t readLen, uint8_t *outBuf, int timeoutMs) {
    Q_ASSERT_X(devId < 0x80, "AwSdkStrategy::syncI2cRead",
               "devId must be 7-bit (0x00-0x7F), R/W bit not allowed");
    if (m_serialManager == nullptr) {
        log(LogLevel::Error, QStringLiteral("SerialManager 未注入，无法发送 I2C 透传"));
        return -1;
    }
    Q_ASSERT_X(QThread::currentThread() == m_serialManager->thread(),
               "AwSdkStrategy::syncI2cRead",
               "must run in SerialManager worker thread (fast-path)");

    const bool verbose = !m_inDownload;
    const auto t0 = std::chrono::steady_clock::now();

    const QByteArray payload =
        MotorProtocol::encodeI2cTransferRead(devId, addr, addrSize, readLen);

    uint8_t outCmd = 0, outSeq = 0;
    QByteArray outData;
    const bool ok = m_serialManager->sendAndWaitResponse(
        MotorProtocol::CmdI2cTransferRead, payload, outCmd, outSeq, outData, timeoutMs);

    if (verbose) {
        const auto t1 = std::chrono::steady_clock::now();
        const qint64 elapsedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        log(LogLevel::Info,
            QStringLiteral("[I2C-R TIMING] total=%1us  (single-thread fast-path)").arg(elapsedUs));
    }

    if (!ok) {
        log(LogLevel::Error, QStringLiteral("I2C 透传读超时或链路失败（%1 ms）").arg(timeoutMs));
        return -1;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        log(LogLevel::Error, QStringLiteral("I2C 透传读失败：STM32 error 0x%1")
                                .arg(code, 2, 16, QLatin1Char('0')));
        return -1;
    }
    if (outData.size() < readLen) {
        log(LogLevel::Error, QStringLiteral("I2C 透传读返回长度不足：%1 < %2")
                                .arg(outData.size()).arg(readLen));
        return -1;
    }
    if (outBuf != nullptr && readLen > 0) {
        std::memcpy(outBuf, outData.constData(), readLen);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// 进度上报与日志
// -----------------------------------------------------------------------------

void AwSdkStrategy::notifyProgress(qint64 sentDelta) {
    m_sentBytes += sentDelta;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool intervalElapsed = now - m_lastReportTimeMs >= kProgressMinIntervalMs;
    const bool bytesAccumulated = m_sentBytes - m_lastReportedBytes >= kProgressMinBytes;
    if (sentDelta == 0 || intervalElapsed || bytesAccumulated) {
        if (m_progress) m_progress(m_sentBytes);
        m_lastReportedBytes = m_sentBytes;
        m_lastReportTimeMs = now;
    }
}

void AwSdkStrategy::log(LogLevel level, const QString &message) const {
    if (m_logSink) m_logSink(level, message);
}
