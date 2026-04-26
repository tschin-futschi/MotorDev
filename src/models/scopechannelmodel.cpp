// =============================================================================
// @file    scopechannelmodel.cpp
// @brief   示波器 8 通道配置数据模型实现
//
// 初始化 8 个通道的默认配置（前 4 个启用，后 4 个禁用），
// 提供各属性的修改方法和协议参数生成逻辑。
// =============================================================================

#include "models/scopechannelmodel.h"

#include "ui/style_constants.h"

using namespace MotorDev;

ScopeChannelModel::ScopeChannelModel(QObject *parent)
    : QObject(parent)
    , m_channels({
          // 前 4 个通道默认启用，设有描述和寄存器地址
          {true, QStringLiteral("Speed"), QStringLiteral("0x0010"), 0x0010, Style::Color::ScopeWaveCh1},
          {true, QStringLiteral("Torque"), QStringLiteral("0x0012"), 0x0012, Style::Color::ScopeWaveCh2},
          {true, QStringLiteral("Temp"), QStringLiteral("0x0014"), 0x0014, Style::Color::ScopeWaveCh3},
          {true, QStringLiteral("Current"), QStringLiteral("0x0016"), 0x0016, Style::Color::ScopeWaveCh4},
          // 后 4 个通道默认禁用
          {false, QString(), QStringLiteral("0x0018"), 0x0018, Style::Color::ScopeWaveCh5},
          {false, QString(), QStringLiteral("0x001A"), 0x001A, Style::Color::ScopeWaveCh6},
          {false, QString(), QStringLiteral("0x001C"), 0x001C, Style::Color::ScopeWaveCh7},
          {false, QString(), QStringLiteral("0x001E"), 0x001E, Style::Color::ScopeWaveCh8},
      }) {
    m_defaultChannels = m_channels;  // 保存一份默认配置，用于样式重置
}

ScopeChannelModel::~ScopeChannelModel() = default;

// --- 单属性修改方法（每个都进行索引校验并发出 channelChanged 信号）---
void ScopeChannelModel::setEnabled(int index, bool enabled) { if (!isValidIndex(index)) return; m_channels[index].enabled = enabled; emit channelChanged(); }
void ScopeChannelModel::setDescription(int index, const QString &text) { if (!isValidIndex(index)) return; m_channels[index].description = text.trimmed(); emit channelChanged(); }
void ScopeChannelModel::setAddressText(int index, const QString &text) { if (!isValidIndex(index)) return; m_channels[index].addressText = text.trimmed(); m_channels[index].registerAddress = parseRegisterAddress(text); emit channelChanged(); }
void ScopeChannelModel::setColor(int index, const QColor &color) { if (!isValidIndex(index)) return; m_channels[index].color = color; emit channelChanged(); }
void ScopeChannelModel::setLineWidth(int index, int width) { if (!isValidIndex(index)) return; m_channels[index].lineWidth = static_cast<qreal>(width); emit channelChanged(); }
void ScopeChannelModel::setLineStyle(int index, Qt::PenStyle style) { if (!isValidIndex(index)) return; m_channels[index].lineStyle = style; emit channelChanged(); }
void ScopeChannelModel::setShowDataPoints(int index, bool show) { if (!isValidIndex(index)) return; m_channels[index].showDataPoints = show; emit channelChanged(); }

/// @brief 将所有通道的样式属性（颜色/线宽/线型/数据点）恢复为构造时的默认值
///
/// 仅重置样式属性，不影响 enabled/description/address 等功能配置。
void ScopeChannelModel::resetStylesToDefault() {
    const int count = qMin(m_channels.size(), m_defaultChannels.size());
    for (int i = 0; i < count; ++i) {
        m_channels[i].color = m_defaultChannels[i].color;
        m_channels[i].lineWidth = m_defaultChannels[i].lineWidth;
        m_channels[i].lineStyle = m_defaultChannels[i].lineStyle;
        m_channels[i].showDataPoints = m_defaultChannels[i].showDataPoints;
    }
    emit stylesReset();
    emit channelChanged();
}

/// @brief 生成当前通道掩码
///
/// 遍历所有通道，将"已启用且寄存器地址有效"的通道对应 bit 置位。
uint8_t ScopeChannelModel::currentChannelMask() const {
    uint8_t mask = 0;
    for (int index = 0; index < m_channels.size(); ++index) {
        if (m_channels.at(index).enabled && m_channels.at(index).registerAddress != 0xFFFF)
            mask |= static_cast<uint8_t>(1u << index);
    }
    return mask;
}

/// @brief 生成 8 通道寄存器映射表
///
/// 未启用的通道填充 0xFFFF（无效地址），协议层据此跳过未映射的通道。
QVector<quint16> ScopeChannelModel::currentRegisterMap() const {
    QVector<quint16> mapping;
    mapping.reserve(m_channels.size());
    for (const ScopeChannelState &channel : m_channels)
        mapping.append(channel.enabled ? channel.registerAddress : 0xFFFF);
    return mapping;
}

bool ScopeChannelModel::hasValidSamplingConfig() const {
    return currentChannelMask() != 0x00;  // 至少有一个有效通道
}

/// @brief 解析寄存器地址文本
///
/// 支持格式："0x0010"、"0X0010"、"0010"（均视为十六进制）。
/// 空文本或解析失败返回 0xFFFF。
quint16 ScopeChannelModel::parseRegisterAddress(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return 0xFFFF;
    bool ok = false;
    const uint value = trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
                           ? trimmed.mid(2).toUInt(&ok, 16)
                           : trimmed.toUInt(&ok, 16);
    if (!ok || value > 0xFFFF) return 0xFFFF;
    return static_cast<quint16>(value);
}
