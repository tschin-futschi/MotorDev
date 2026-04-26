// =============================================================================
// @file    sampling_config.h
// @brief   示波器采样参数配置工具
//
// 提供采样间隔、显示窗口等参数在 UI 文本和协议索引之间的映射。
// 被 OscilloscopTab 和 ScopeService 使用。
// =============================================================================

#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace SamplingConfig {

/// @brief 将采样间隔 UI 文本转换为协议索引
/// @param text  UI 显示的文本（如 "200 us"）
/// @return 协议索引（0x00~0x06）；无匹配时默认返回 0x04（1000 us）
uint8_t intervalIndexForText(const QString &text);

/// @brief 将协议索引转换为采样间隔（微秒）
/// @param index  协议索引（0x00~0x06）
/// @return 采样间隔微秒值；无效索引默认返回 1000
int intervalUsForIndex(uint8_t index);

/// @brief 将显示窗口 UI 文本转换为毫秒值
/// @param text  UI 显示的文本（如 "50 ms"）
/// @return 窗口时长毫秒值；无匹配时默认返回 50
int displayWindowMsForText(const QString &text);

/// @brief 获取所有采样间隔选项的 UI 标签列表
/// @return 用于填充下拉框的字符串列表
QStringList intervalLabels();

}  // namespace SamplingConfig
