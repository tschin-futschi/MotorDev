// =============================================================================
// @file    hl9788n_bridge.h
// @brief   HL9788N vendor 库与 MotorDev SerialManager 的桥接层
//
// vendor 库（src/services/flashstrategies/vendor/hl9788n/）通过 3 个全局函数指针
// 与外界通信：WriteI2CDev / ReadI2CDev / OutputLog。本桥接层提供：
// - attach/detach：把 SerialManager 与日志回调注入 vendor 函数指针
// - 3 个 vendor 回调实现：将 vendor I2C 调用翻译为 0x30/0x31 透传命令
// - progress/cancel hook 注入：供 vendor hl9788n_fw_update_dma_with_buffer() 调用
// - 状态翻译：把 hl9788n_rv_status() 的位掩码翻成中文
//
// 端序：零字节翻转。vendor 内部已按 IC datasheet 拼好字节流，桥接层原样透传。
//
// 线程：bridge_i2c_write/read 必须在 SerialManager 工作线程内被调用（与 AW 一致），
// 因为内部使用 SerialManager::sendAndWaitResponse 的 fast-path。
//
// 并发：vendor 全局函数指针是单例语义。同一时刻只允许 1 个 DW IC 在烧录；未
// detach 时再次 attach 会触发 Q_ASSERT。
// =============================================================================
#pragma once

#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>

class SerialManager;

namespace Hl9788nBridge {

/// @brief 注入 SerialManager + 日志回调到 vendor 全局函数指针
/// @param sm                  SerialManager 实例（必须非空）
/// @param logSink             日志接收 sink；中转 vendor OutputLog
/// @param defaultTimeoutMs    单次 I2C 透传超时（默认 2000 ms，覆盖串口往返）
/// @return true=attach 成功；false=已有其他调用方 attach 未 detach（运行期互斥，
///         Release 下亦生效），调用方应据此中止本次烧录。
[[nodiscard]] bool attach(SerialManager *sm,
                          std::function<void(const QString &)> logSink,
                          int defaultTimeoutMs = 2000);

/// @brief 解绑桥接层。烧录结束/失败时调用。重复 detach 安全（幂等）。
void detach();

/// @brief 注入进度回调（vendor hl9788n_fw_update_dma_with_buffer 内部调用）
/// @param cb 进度回调；nullptr 时禁用
void setProgressCallback(std::function<void(int pct)> cb);

/// @brief 注入取消标志地址（vendor 在关键点检查；返回非零即取消）
/// @param flag 外部 std::atomic<bool>* 的地址；nullptr 时禁用
void setCancelFlag(const std::atomic<bool> *flag);

/// @brief 把 hl9788n_rv_status() 返回值翻译为中文诊断字符串
/// @param status hl9788n_rv_status() 返回的位掩码
QString describeRvStatus(uint16_t status);

}  // namespace Hl9788nBridge
