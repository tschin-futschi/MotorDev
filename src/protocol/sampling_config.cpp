// =============================================================================
// @file    sampling_config.cpp
// @brief   示波器采样参数配置工具实现
//
// 定义采样间隔和显示窗口的 UI 文本 ↔ 数值映射关系。
// =============================================================================

#include "protocol/sampling_config.h"

namespace SamplingConfig {

/// @brief 采样间隔选项标签列表
///
/// 索引 0~6 对应协议中的 intervalIndex 0x00~0x06。
QStringList intervalLabels() {
    return {
        QStringLiteral("150 us"),  QStringLiteral("250 us"),
        QStringLiteral("300 us"),  QStringLiteral("400 us"),
        QStringLiteral("500 us"),  QStringLiteral("900 us"),
        QStringLiteral("1000 us")
    };
}

uint8_t intervalIndexForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("150 us")) return 0x00;   // 150 微秒
    if (normalized == QStringLiteral("250 us")) return 0x01;   // 250 微秒
    if (normalized == QStringLiteral("300 us")) return 0x02;   // 300 微秒
    if (normalized == QStringLiteral("400 us")) return 0x03;   // 400 微秒
    if (normalized == QStringLiteral("500 us")) return 0x04;   // 500 微秒
    if (normalized == QStringLiteral("900 us")) return 0x05;   // 900 微秒
    if (normalized == QStringLiteral("1000 us")) return 0x06;  // 1000 微秒
    return 0x03;  // 默认 400 us
}

int intervalUsForIndex(uint8_t index) {
    switch (index) {
    case 0x00: return 150;
    case 0x01: return 250;
    case 0x02: return 300;
    case 0x03: return 400;
    case 0x04: return 500;
    case 0x05: return 900;
    case 0x06: return 1000;
    default: return 400;  // 无效索引默认 400 us
    }
}

int displayWindowMsForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("50 ms")) return 50;
    if (normalized == QStringLiteral("200 ms")) return 200;
    if (normalized == QStringLiteral("500 ms")) return 500;
    if (normalized == QStringLiteral("1000 ms")) return 1000;
    if (normalized == QStringLiteral("2000 ms")) return 2000;
    if (normalized == QStringLiteral("4000 ms")) return 4000;
    return 50;  // 默认 50 ms
}

// --- 采样配置默认值（唯一来源）---
// 默认 UI 文本在此集中定义，索引/毫秒值由对应映射函数派生，确保三者始终一致。

QString defaultIntervalLabel() {
    // 与 protocol.md §4.4 固件上电默认采样间隔一致（索引 0x03 = 400us），
    // 消除 UI 默认与固件默认的语义漂移。
    return QStringLiteral("400 us");
}

uint8_t defaultIntervalIndex() {
    return intervalIndexForText(defaultIntervalLabel());
}

QString defaultDisplayWindowLabel() {
    return QStringLiteral("50 ms");
}

int defaultDisplayWindowMs() {
    return displayWindowMsForText(defaultDisplayWindowLabel());
}

}  // namespace SamplingConfig
