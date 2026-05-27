// =============================================================================
// @file    hl9788n_bridge.cpp
// @brief   HL9788N vendor 库与 MotorDev SerialManager 的桥接层实现
// =============================================================================
#include "services/flashstrategies/hl9788n_bridge.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"

#include <QByteArray>
#include <QDebug>
#include <QStringList>
#include <QThread>

#include <cstdint>
#include <cstring>
#include <mutex>

// vendor 包装头放最后：先让 Qt/std 头展开（含 cmath、COM 等），再引入 vendor
// 并 #undef 污染宏（Wait、round）
#include "services/flashstrategies/hl9788n_vendor_include.h"

// -----------------------------------------------------------------------------
// vendor 链接占位：vendor hl9788n_api_ref.cpp 引用 extern modulePath / moduleFileName
// （原为 DLL 主程序提供）。MotorDev 是 exe，提供空占位避免链接失败。
// vendor 仅在 hl978x_extfuncinit() 内通过 GetFileVersionInfo(moduleFileName, ...)
// 读 DLL 版本号；MotorDev 不在意此 log，filename 空字符串时 GetFileVersionInfo
// 失败仅 log 一行，不影响主流程。
// -----------------------------------------------------------------------------
TCHAR modulePath[_MAX_DIR] = {0};
TCHAR moduleFileName[_MAX_PATH] = {0};

namespace {

// -----------------------------------------------------------------------------
// 桥接层内部状态（单例）
// -----------------------------------------------------------------------------
std::mutex g_mutex;
bool       g_attached = false;

SerialManager                       *g_serial = nullptr;
std::function<void(const QString &)> g_logSink;
int                                   g_defaultTimeoutMs = 2000;

std::function<void(int)>             g_progressCb;
const std::atomic<bool>              *g_cancelFlag = nullptr;

// -----------------------------------------------------------------------------
// vendor 回调实现（必须在 SerialManager 工作线程内被调用，由 strategy 保证）
// -----------------------------------------------------------------------------

int32_t bridge_i2c_write(uint8_t slv7, uint8_t count, uint8_t *data) {
    if (g_serial == nullptr || data == nullptr || count == 0) return -1;

    // vendor 已把 [AddrH][AddrL][D...] 拼好；addrSize=0 整段透传，端序无关
    const QByteArray payload = MotorProtocol::encodeI2cTransferWrite(
        slv7, nullptr, 0, data, count);

    uint8_t  outCmd  = 0;
    uint8_t  outSeq  = 0;
    QByteArray outData;
    const bool ok = g_serial->sendAndWaitResponse(
        MotorProtocol::CmdI2cTransferWrite, payload,
        outCmd, outSeq, outData, g_defaultTimeoutMs);

    if (!ok) {
        if (g_logSink) {
            g_logSink(QStringLiteral("[hl9788n_bridge] I2C write timeout (slv=0x%1, count=%2)")
                          .arg(slv7, 2, 16, QLatin1Char('0')).arg(count));
        }
        return -1;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        if (g_logSink) {
            g_logSink(QStringLiteral("[hl9788n_bridge] I2C write error response 0x%1 (slv=0x%2)")
                          .arg(code, 2, 16, QLatin1Char('0'))
                          .arg(slv7, 2, 16, QLatin1Char('0')));
        }
        return -1;
    }
    return 0;
}

int32_t bridge_i2c_read(uint8_t slv7,
                        uint8_t addr_cnt, uint8_t *addr,
                        uint8_t read_cnt, uint8_t *buf) {
    if (g_serial == nullptr || buf == nullptr || read_cnt == 0) return -1;

    const QByteArray payload = MotorProtocol::encodeI2cTransferRead(
        slv7, addr, addr_cnt, read_cnt);

    uint8_t  outCmd  = 0;
    uint8_t  outSeq  = 0;
    QByteArray outData;
    const bool ok = g_serial->sendAndWaitResponse(
        MotorProtocol::CmdI2cTransferRead, payload,
        outCmd, outSeq, outData, g_defaultTimeoutMs);

    if (!ok) {
        if (g_logSink) {
            g_logSink(QStringLiteral("[hl9788n_bridge] I2C read timeout (slv=0x%1, read_cnt=%2)")
                          .arg(slv7, 2, 16, QLatin1Char('0')).arg(read_cnt));
        }
        return -1;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        if (g_logSink) {
            g_logSink(QStringLiteral("[hl9788n_bridge] I2C read error response 0x%1 (slv=0x%2)")
                          .arg(code, 2, 16, QLatin1Char('0'))
                          .arg(slv7, 2, 16, QLatin1Char('0')));
        }
        return -1;
    }
    if (outData.size() < read_cnt) {
        if (g_logSink) {
            g_logSink(QStringLiteral("[hl9788n_bridge] I2C read short (got=%1, expected=%2)")
                          .arg(outData.size()).arg(read_cnt));
        }
        return -1;
    }
    std::memcpy(buf, outData.constData(), read_cnt);
    return 0;
}

int32_t bridge_output_log(const char *str) {
    if (str == nullptr) return 0;
    if (g_logSink) {
        // vendor cpp（Func.cpp / hl9788n_api_ref.cpp）在 MotorDev 工程里被强制以
        // UNICODE off 编译（见 CMakeLists 的 set_source_files_properties -UUNICODE），
        // 故 TCHAR = char，OutputLog 传入的 str 是 ANSI/UTF-8 字节流。
        // 不能基于本 TU 的 #ifdef UNICODE 决定解码方式（本 TU 在 Qt 全局 UNICODE
        // 下编译，但 vendor 不是），否则把 char* 当 wchar_t* 解码会乱码。
        g_logSink(QString::fromLocal8Bit(str));
    }
    return 0;
}

// -----------------------------------------------------------------------------
// vendor hook 适配函数（C 风格签名，转发到 std::function）
// -----------------------------------------------------------------------------

void vendor_progress_trampoline(int pct) {
    if (g_progressCb) g_progressCb(pct);
}

int vendor_cancel_trampoline(void) {
    return (g_cancelFlag && g_cancelFlag->load()) ? 1 : 0;
}

}  // namespace

namespace Hl9788nBridge {

void attach(SerialManager *sm,
            std::function<void(const QString &)> logSink,
            int defaultTimeoutMs) {
    std::lock_guard<std::mutex> lock(g_mutex);

    Q_ASSERT_X(!g_attached, "Hl9788nBridge::attach",
               "vendor function pointers already attached; detach() first");
    Q_ASSERT(sm != nullptr);

    g_serial            = sm;
    g_logSink           = std::move(logSink);
    g_defaultTimeoutMs  = defaultTimeoutMs > 0 ? defaultTimeoutMs : 2000;
    g_progressCb        = nullptr;
    g_cancelFlag        = nullptr;
    g_attached          = true;

    // 注入到 vendor 函数指针（Func.cpp 内的 extern 全局）
    WriteI2CDev = &bridge_i2c_write;
    ReadI2CDev  = &bridge_i2c_read;
    OutputLog   = &bridge_output_log;

    // vendor _with_buffer 的 hook 入口
    hl9788n_set_progress_cb(&vendor_progress_trampoline);
    hl9788n_set_cancel_check(&vendor_cancel_trampoline);

    if (g_logSink) {
        g_logSink(QStringLiteral("[hl9788n_bridge] attached (timeout=%1 ms)")
                      .arg(g_defaultTimeoutMs));
        g_logSink(QStringLiteral("[hl9788n_bridge] note: vendor m_delay() uses "
                                  "timeBeginPeriod(2) + busy-wait; ~1 CPU core "
                                  "will be occupied during flashing"));
    }
}

void detach() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_attached) return;

    hl9788n_set_progress_cb(nullptr);
    hl9788n_set_cancel_check(nullptr);

    WriteI2CDev = nullptr;
    ReadI2CDev  = nullptr;
    OutputLog   = nullptr;

    g_serial      = nullptr;
    g_logSink     = nullptr;
    g_progressCb  = nullptr;
    g_cancelFlag  = nullptr;
    g_attached    = false;
}

void setProgressCallback(std::function<void(int)> cb) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_progressCb = std::move(cb);
}

void setCancelFlag(const std::atomic<bool> *flag) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cancelFlag = flag;
}

QString describeRvStatus(uint16_t status) {
    QStringList parts;
    parts.reserve(8);

    // 位掩码定义见 hl9788n_api_ref.h（#define CS_OK_TM (1<<15) 等），直接用宏
    if (!(status & CS_OK_INS))     parts << QStringLiteral("INS checksum NG");
    if (!(status & CS_OK_LUT))     parts << QStringLiteral("LUT checksum NG");
    if (!(status & CS_OK_UM))      parts << QStringLiteral("UM checksum NG");
    if (!(status & CS_OK_TM))      parts << QStringLiteral("TM checksum NG");
    if (status & RV_ERR_INS)       parts << QStringLiteral("INS verify error");
    if (status & RV_ERR_LUT)       parts << QStringLiteral("LUT verify error");
    if (status & RV_ERR_UM)        parts << QStringLiteral("UM verify error");
    if (status & RV_ERR_TM)        parts << QStringLiteral("TM verify error");
    if (!(status & AUTOLOAD_DONE)) parts << QStringLiteral("autoload not done");
    if (status & FMC_BUSY)         parts << QStringLiteral("FMC busy");

    const QString base = QStringLiteral("status=0x%1")
                             .arg(status, 4, 16, QLatin1Char('0')).toUpper();
    if (parts.isEmpty()) {
        return base + QStringLiteral(" (all OK)");
    }
    return base + QStringLiteral(" [") + parts.join(QStringLiteral(", ")) + QStringLiteral("]");
}

}  // namespace Hl9788nBridge
