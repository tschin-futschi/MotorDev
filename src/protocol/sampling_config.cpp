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
        QStringLiteral("200 us"),  QStringLiteral("300 us"),
        QStringLiteral("500 us"),  QStringLiteral("750 us"),
        QStringLiteral("1000 us"), QStringLiteral("1500 us"),
        QStringLiteral("2000 us")
    };
}

uint8_t intervalIndexForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("200 us")) return 0x00;   // 200 微秒
    if (normalized == QStringLiteral("300 us")) return 0x01;   // 300 微秒
    if (normalized == QStringLiteral("500 us")) return 0x02;   // 500 微秒
    if (normalized == QStringLiteral("750 us")) return 0x03;   // 750 微秒
    if (normalized == QStringLiteral("1000 us")) return 0x04;  // 1000 微秒
    if (normalized == QStringLiteral("1500 us")) return 0x05;  // 1500 微秒
    if (normalized == QStringLiteral("2000 us")) return 0x06;  // 2000 微秒
    return 0x04;  // 默认 1000 us
}

int intervalUsForIndex(uint8_t index) {
    switch (index) {
    case 0x00: return 200;
    case 0x01: return 300;
    case 0x02: return 500;
    case 0x03: return 750;
    case 0x04: return 1000;
    case 0x05: return 1500;
    case 0x06: return 2000;
    default: return 1000;  // 无效索引默认 1000 us
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

}  // namespace SamplingConfig
