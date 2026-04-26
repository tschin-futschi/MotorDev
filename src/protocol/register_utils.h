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

}  // namespace RegisterUtils
