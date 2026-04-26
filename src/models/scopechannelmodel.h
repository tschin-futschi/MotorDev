// =============================================================================
// @file    scopechannelmodel.h
// @brief   示波器 8 通道配置数据模型
//
// 管理示波器 8 个通道的完整配置状态（启用、描述、寄存器地址、颜色、线型等）。
// 被 OscilloscopTab、ScopeChannelStrip、ScopePlotWidget、ScopeStylePanel 共用。
//
// 职责：
// - 存储和修改各通道配置
// - 生成协议所需的通道掩码和寄存器映射表
// - 样式重置功能
//
// 线程安全：仅在主线程使用。
// =============================================================================

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>

/// @brief 单个通道的配置状态
struct ScopeChannelState {
    bool enabled = false;                    ///< 是否启用该通道
    QString description;                     ///< 通道描述文字（如 "Speed"）
    QString addressText;                     ///< 寄存器地址文本（如 "0x0010"）
    quint16 registerAddress = 0xFFFF;        ///< 解析后的寄存器地址（0xFFFF 表示无效）
    QColor color;                            ///< 波形颜色
    qreal lineWidth = 4.0;                   ///< 线宽（像素）
    Qt::PenStyle lineStyle = Qt::SolidLine;  ///< 线型（实线/虚线等）
    bool showDataPoints = false;             ///< 是否在波形上显示数据点
};

/// @brief 示波器 8 通道配置数据模型
///
/// 存储所有通道的配置状态，提供修改接口和协议参数生成方法。
/// 配置变更时发出 channelChanged() 信号，样式重置时额外发出 stylesReset()。
class ScopeChannelModel : public QObject {
    Q_OBJECT

public:
    explicit ScopeChannelModel(QObject *parent = nullptr);
    ~ScopeChannelModel() override;

    /// @brief 获取通道总数（固定为 8）
    int channelCount() const { return m_channels.size(); }

    /// @brief 获取指定通道的配置（只读）
    const ScopeChannelState &channel(int index) const { return m_channels.at(index); }

    /// @brief 获取所有通道配置的只读引用
    const QVector<ScopeChannelState> &channels() const { return m_channels; }

    // --- 通道属性修改（每个方法都会发出 channelChanged 信号）---

    /// @brief 设置通道启用状态
    void setEnabled(int index, bool enabled);

    /// @brief 设置通道描述文字
    void setDescription(int index, const QString &text);

    /// @brief 设置通道寄存器地址文本（同时更新解析后的 registerAddress）
    void setAddressText(int index, const QString &text);

    /// @brief 设置通道波形颜色
    void setColor(int index, const QColor &color);

    /// @brief 设置通道线宽
    void setLineWidth(int index, int width);

    /// @brief 设置通道线型
    void setLineStyle(int index, Qt::PenStyle style);

    /// @brief 设置是否显示数据点
    void setShowDataPoints(int index, bool show);

    /// @brief 将所有通道的样式（颜色/线宽/线型/数据点）重置为默认值
    void resetStylesToDefault();

    // --- 协议参数生成 ---

    /// @brief 根据当前启用状态和有效地址生成通道掩码
    /// @return 8 位掩码，bit0~bit7 对应 CH1~CH8
    uint8_t currentChannelMask() const;

    /// @brief 获取当前 8 通道的寄存器地址映射表
    /// @return 8 个 quint16 地址（未启用的通道为 0xFFFF）
    QVector<quint16> currentRegisterMap() const;

    /// @brief 检查是否至少有一个通道配置了有效的采样参数
    bool hasValidSamplingConfig() const;

    /// @brief 解析寄存器地址文本为 quint16
    /// @param text  地址文本（支持 0x 前缀十六进制，也支持纯十六进制）
    /// @return 解析后的地址；解析失败返回 0xFFFF
    static quint16 parseRegisterAddress(const QString &text);

signals:
    /// @brief 任意通道配置发生变更
    void channelChanged();

    /// @brief 样式已重置为默认值
    void stylesReset();

private:
    /// @brief 检查索引是否合法
    bool isValidIndex(int index) const { return index >= 0 && index < m_channels.size(); }

    QVector<ScopeChannelState> m_channels;         ///< 8 通道当前配置
    QVector<ScopeChannelState> m_defaultChannels;   ///< 8 通道默认配置（用于样式重置）
};
