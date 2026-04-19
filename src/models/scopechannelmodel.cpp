// ScopeChannelModel — 示波器 8 通道配置数据模型
#include "models/scopechannelmodel.h"

#include "ui/style_constants.h"

using namespace MotorDev;

ScopeChannelModel::ScopeChannelModel(QObject *parent)
    : QObject(parent)
    , m_channels({
          {true, QStringLiteral("Speed"), QStringLiteral("0x0010"), 0x0010, Style::Color::ScopeWaveCh1},
          {true, QStringLiteral("Torque"), QStringLiteral("0x0012"), 0x0012, Style::Color::ScopeWaveCh2},
          {true, QStringLiteral("Temp"), QStringLiteral("0x0014"), 0x0014, Style::Color::ScopeWaveCh3},
          {true, QStringLiteral("Current"), QStringLiteral("0x0016"), 0x0016, Style::Color::ScopeWaveCh4},
          {false, QString(), QStringLiteral("0x0018"), 0x0018, Style::Color::ScopeWaveCh5},
          {false, QString(), QStringLiteral("0x001A"), 0x001A, Style::Color::ScopeWaveCh6},
          {false, QString(), QStringLiteral("0x001C"), 0x001C, Style::Color::ScopeWaveCh7},
          {false, QString(), QStringLiteral("0x001E"), 0x001E, Style::Color::ScopeWaveCh8},
      }) {
    m_defaultChannels = m_channels;
}

ScopeChannelModel::~ScopeChannelModel() = default;

void ScopeChannelModel::setEnabled(int index, bool enabled) { if (!isValidIndex(index)) return; m_channels[index].enabled = enabled; emit channelChanged(); }
void ScopeChannelModel::setDescription(int index, const QString &text) { if (!isValidIndex(index)) return; m_channels[index].description = text.trimmed(); emit channelChanged(); }
void ScopeChannelModel::setAddressText(int index, const QString &text) { if (!isValidIndex(index)) return; m_channels[index].addressText = text.trimmed(); m_channels[index].registerAddress = parseRegisterAddress(text); emit channelChanged(); }
void ScopeChannelModel::setColor(int index, const QColor &color) { if (!isValidIndex(index)) return; m_channels[index].color = color; emit channelChanged(); }
void ScopeChannelModel::setLineWidth(int index, int width) { if (!isValidIndex(index)) return; m_channels[index].lineWidth = static_cast<qreal>(width); emit channelChanged(); }
void ScopeChannelModel::setLineStyle(int index, Qt::PenStyle style) { if (!isValidIndex(index)) return; m_channels[index].lineStyle = style; emit channelChanged(); }
void ScopeChannelModel::setShowDataPoints(int index, bool show) { if (!isValidIndex(index)) return; m_channels[index].showDataPoints = show; emit channelChanged(); }

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

uint8_t ScopeChannelModel::currentChannelMask() const {
    uint8_t mask = 0;
    for (int index = 0; index < m_channels.size(); ++index) {
        if (m_channels.at(index).enabled && m_channels.at(index).registerAddress != 0xFFFF)
            mask |= static_cast<uint8_t>(1u << index);
    }
    return mask;
}

QVector<quint16> ScopeChannelModel::currentRegisterMap() const {
    QVector<quint16> mapping;
    mapping.reserve(m_channels.size());
    for (const ScopeChannelState &channel : m_channels)
        mapping.append(channel.enabled ? channel.registerAddress : 0xFFFF);
    return mapping;
}

bool ScopeChannelModel::hasValidSamplingConfig() const {
    return currentChannelMask() != 0x00;
}

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
