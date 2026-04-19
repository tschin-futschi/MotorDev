// ScopeChannelModel — 示波器 8 通道配置数据模型
#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>

struct ScopeChannelState {
    bool enabled = false;
    QString description;
    QString addressText;
    quint16 registerAddress = 0xFFFF;
    QColor color;
    qreal lineWidth = 4.0;
    Qt::PenStyle lineStyle = Qt::SolidLine;
    bool showDataPoints = false;
};

class ScopeChannelModel : public QObject {
    Q_OBJECT

public:
    explicit ScopeChannelModel(QObject *parent = nullptr);
    ~ScopeChannelModel() override;

    int channelCount() const { return m_channels.size(); }
    const ScopeChannelState &channel(int index) const { return m_channels.at(index); }
    const QVector<ScopeChannelState> &channels() const { return m_channels; }

    void setEnabled(int index, bool enabled);
    void setDescription(int index, const QString &text);
    void setAddressText(int index, const QString &text);
    void setColor(int index, const QColor &color);
    void setLineWidth(int index, int width);
    void setLineStyle(int index, Qt::PenStyle style);
    void setShowDataPoints(int index, bool show);
    void resetStylesToDefault();

    uint8_t currentChannelMask() const;
    QVector<quint16> currentRegisterMap() const;
    bool hasValidSamplingConfig() const;

    static quint16 parseRegisterAddress(const QString &text);

signals:
    void channelChanged();
    void stylesReset();

private:
    bool isValidIndex(int index) const { return index >= 0 && index < m_channels.size(); }

    QVector<ScopeChannelState> m_channels;
    QVector<ScopeChannelState> m_defaultChannels;
};
