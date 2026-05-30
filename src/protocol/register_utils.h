// =============================================================================
// @file    register_utils.h
// @brief   寄存器地址/值输入解析工具
//
// 提供将用户输入的文本（支持十进制和 0x 前缀十六进制）解析为 quint16 数值的工具函数。
// 被 RegisterTable、示波器通道配置等模块使用。
// =============================================================================

#pragma once

#include <QString>
#include <QtGlobal>

namespace RegisterUtils {

/// @brief 解析用户输入的数字文本为 quint16
///
/// 支持的格式：
/// - 十进制：如 "1234"
/// - 十六进制：如 "0x04D2" 或 "0X04D2"
///
/// @param text  用户输入的文本
/// @param out   输出：解析结果（调用者提供，不可为 nullptr）
/// @return 解析成功返回 true，失败（空字符串、格式错误、溢出）返回 false
bool parseNumber(const QString &text, quint16 *out);

/// @brief 解析寄存器数值文本为有符号 qint16（值解析唯一来源）
///
/// 支持的格式：
/// - 十六进制：如 "0xFFFF"（按 16 位裸值取，0xFFFF → -1）
/// - 十进制：有符号，如 "-1"、"1234"（范围 -32768 ~ 32767）
///
/// 进制由 `0x`/`0X` 前缀决定，与显示模式无关。
///
/// @param text   用户输入的文本
/// @param value  输出：解析结果（可为 nullptr 仅做校验）
/// @return 解析成功返回 true，失败（空、格式错误、越界）返回 false
bool parseSignedValue(const QString &text, qint16 *value);

/// @brief 将有符号 qint16 寄存器值格式化为显示文本（显示格式唯一来源）
///
/// @param value  寄存器值
/// @param hex    true=十六进制（"0x" + 4 位大写）；false=有符号十进制
/// @return 格式化文本
QString formatValue(qint16 value, bool hex);

}  // namespace RegisterUtils
