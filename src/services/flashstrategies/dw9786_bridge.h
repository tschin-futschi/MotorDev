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

void attach(SerialManager *sm,
            std::function<void(const QString &)> logSink,
            int defaultTimeoutMs = 2000);

void detach();

void setProgressCallback(std::function<void(int pct)> cb);
void setCancelFlag(const std::atomic<bool> *flag);

}  // namespace Dw9786Bridge
