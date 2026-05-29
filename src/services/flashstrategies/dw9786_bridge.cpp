// =============================================================================
// @file    dw9786_bridge.cpp
// @brief   DW9786 vendor 库与 MotorDev SerialManager 的桥接层实现
//
// 与 hl9788n_bridge.cpp 完全对称。注意：HL9788N 和 DW9786 vendor 共享同名全局
// 函数指针 WriteI2CDev / ReadI2CDev / OutputLog（link 时通过
// --allow-multiple-definition 合并）。同时刻仅允许 1 个桥接层 attach，由各自
// g_attached 防重 + 调用方约束（同时刻仅 1 个 DW IC 烧录）保障。
// =============================================================================
#include "services/flashstrategies/dw9786_bridge.h"

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
#include "services/flashstrategies/dw9786_vendor_include.h"

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
            g_logSink(QStringLiteral("[dw9786_bridge] I2C write timeout (slv=0x%1, count=%2)")
                          .arg(slv7, 2, 16, QLatin1Char('0')).arg(count));
        }
        return -1;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        if (g_logSink) {
            g_logSink(QStringLiteral("[dw9786_bridge] I2C write error response 0x%1 (slv=0x%2)")
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
            g_logSink(QStringLiteral("[dw9786_bridge] I2C read timeout (slv=0x%1, read_cnt=%2)")
                          .arg(slv7, 2, 16, QLatin1Char('0')).arg(read_cnt));
        }
        return -1;
    }
    if (outCmd == MotorProtocol::CmdErrorResponse) {
        const uint8_t code = MotorProtocol::decodeErrorCode(outData);
        if (g_logSink) {
            g_logSink(QStringLiteral("[dw9786_bridge] I2C read error response 0x%1 (slv=0x%2)")
                          .arg(code, 2, 16, QLatin1Char('0'))
                          .arg(slv7, 2, 16, QLatin1Char('0')));
        }
        return -1;
    }
    if (outData.size() < read_cnt) {
        if (g_logSink) {
            g_logSink(QStringLiteral("[dw9786_bridge] I2C read short (got=%1, expected=%2)")
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
        // vendor cpp 在 MotorDev 工程里被强制以 UNICODE off 编译，TCHAR = char。
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

namespace Dw9786Bridge {

void attach(SerialManager *sm,
            std::function<void(const QString &)> logSink,
            int defaultTimeoutMs) {
    std::lock_guard<std::mutex> lock(g_mutex);

    Q_ASSERT_X(!g_attached, "Dw9786Bridge::attach",
               "vendor function pointers already attached; detach() first");
    Q_ASSERT(sm != nullptr);

    g_serial            = sm;
    g_logSink           = std::move(logSink);
    g_defaultTimeoutMs  = defaultTimeoutMs > 0 ? defaultTimeoutMs : 2000;
    g_progressCb        = nullptr;
    g_cancelFlag        = nullptr;
    g_attached          = true;

    // 注入到 vendor 函数指针。注意：HL9788N 与 DW9786 vendor 共享同名全局符号
    // （link 阶段 --allow-multiple-definition 合并），attach 此处的赋值会覆盖
    // 当前进程内的 WriteI2CDev/ReadI2CDev/OutputLog，所以同时刻仅允许 1 个桥接
    // 层 attach（调用方约束）。
    WriteI2CDev = &bridge_i2c_write;
    ReadI2CDev  = &bridge_i2c_read;
    OutputLog   = &bridge_output_log;

    // vendor _with_buffer 的 hook 入口
    dw9786_set_progress_cb(&vendor_progress_trampoline);
    dw9786_set_cancel_check(&vendor_cancel_trampoline);

    if (g_logSink) {
        g_logSink(QStringLiteral("[dw9786_bridge] attached (timeout=%1 ms)")
                      .arg(g_defaultTimeoutMs));
        g_logSink(QStringLiteral("[dw9786_bridge] note: vendor m_delay() uses "
                                  "timeBeginPeriod(2) + busy-wait; ~1 CPU core "
                                  "will be occupied during flashing"));
    }
}

void detach() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_attached) return;

    dw9786_set_progress_cb(nullptr);
    dw9786_set_cancel_check(nullptr);

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

}  // namespace Dw9786Bridge
