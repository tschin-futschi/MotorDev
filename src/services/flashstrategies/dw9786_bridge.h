// =============================================================================
// @file    dw9786_bridge.h
// @brief   DW9786 vendor 库与 MotorDev SerialManager 的桥接层（骨架，待填充）
// =============================================================================
#pragma once

#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>

class SerialManager;

namespace Dw9786Bridge {

/// @brief 注入 SerialManager + 日志回调到 vendor 全局函数指针。
/// @return true=attach 成功；false=已有其他调用方 attach 未 detach（运行期互斥，
///         Release 下亦生效），调用方应据此中止本次操作。
[[nodiscard]] bool attach(SerialManager *sm,
                          std::function<void(const QString &)> logSink,
                          int defaultTimeoutMs = 2000);

void detach();

void setProgressCallback(std::function<void(int pct)> cb);
void setCancelFlag(const std::atomic<bool> *flag);

/// @brief 本次 attach 以来 I2C 透传失败次数（超时 / 错误响应 / 短读）。
/// attach 时清零；供 OISReset 等"只调 vendor 写、不读 vendor 返回值"的序列
/// 在结束时判定整条序列是否真正成功。仅在 SerialManager 工作线程读取。
int transferErrorCount();

}  // namespace Dw9786Bridge
