// =============================================================================
// @file    ftdi_latency_helper.h
// @brief   FT232 Latency Timer 自动设置工具
//
// 通过运行时动态加载 ftd2xx.dll 调用 FTDI D2XX API，把指定 COM 端口
// 对应 FT232 芯片的 Latency Timer 强制设置为期望值（典型 1 ms），
// 消除 USB-Serial 默认 16 ms Latency Timer 对烧录链路 RTT 的拖累。
//
// 仅 Windows 平台生效；非 Windows 平台或非 FTDI 桥（CP210x / CH340 等）
// 将返回 false 并填充 outErrorMessage，调用方应 best-effort 继续。
// =============================================================================

#pragma once

#include <QString>
#include <QtGlobal>

namespace FtdiLatencyHelper {

// @brief 把指定 COM 端口对应 FT232 芯片的 Latency Timer 设置为 latencyMs
// @param portName       Qt COM 端口名（如 "COM5"）
// @param latencyMs      目标 Latency Timer 值，1-255 ms
// @param outSerialNumber 出参：成功时填充 FTDI 序列号，便于日志定位
// @param outErrorMessage 出参：失败时填充原因短语
// @return 成功 true / 失败 false（best-effort，调用方失败时仅打日志，不阻断打开串口）
bool setLatencyTimerForPort(const QString &portName,
                             quint8 latencyMs,
                             QString &outSerialNumber,
                             QString &outErrorMessage);

}  // namespace FtdiLatencyHelper
