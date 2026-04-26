// =============================================================================
// @file    scopestylepanel.h
// @brief   示波器样式面板 — 8 通道的颜色、线宽、线型配置
//
// ScopeStylePanel 位于示波器页面右侧（可折叠），提供每个通道的样式编辑：
// - 颜色按钮：点击弹出 QColorDialog 选择颜色
// - 线宽 SpinBox：1~8 像素
// - 线型 ComboBox：实线/虚线/点线/点划线
// - "恢复默认"按钮：重置所有通道样式
// =============================================================================
#pragma once

#include <QColor>
#include <QWidget>

class QComboBox;
class QPushButton;
class QSpinBox;

/// @brief 示波器通道样式配置面板
///
/// 管理 8 个通道的线条样式（颜色、线宽、线型）。
/// 通过信号通知 ScopeChannelModel 更新对应属性。
class ScopeStylePanel : public QWidget {
    Q_OBJECT

public:
    explicit ScopeStylePanel(QWidget *parent = nullptr);
    ~ScopeStylePanel() override;

    /// @brief 设置指定通道的颜色（同步按钮背景色）
    void setChannelColor(int index, const QColor &color);

    /// @brief 设置指定通道的线宽
    void setChannelLineWidth(int index, int width);

    /// @brief 设置指定通道的线型
    void setChannelLineStyle(int index, Qt::PenStyle style);

    /// @brief 设置指定通道是否显示数据点
    void setChannelShowDataPoints(int index, bool show);

signals:
    void colorChanged(int index, const QColor &color);              ///< 颜色变化
    void lineWidthChanged(int index, int width);                    ///< 线宽变化
    void lineStyleChanged(int index, Qt::PenStyle style);           ///< 线型变化
    void dataPointsChanged(int index, bool showDataPoints);         ///< 数据点显示变化
    void defaultSettingsRequested();                                ///< 恢复默认请求

private:
    static constexpr int kChannelCount = 8;     ///< 通道数量

    /// @brief 单通道样式控件集
    struct ChannelRow {
        QPushButton *colorButton = nullptr;     ///< 颜色选择按钮
        QSpinBox *widthSpin = nullptr;          ///< 线宽 SpinBox
        QComboBox *styleCombo = nullptr;        ///< 线型 ComboBox
        QColor currentColor;                    ///< 当前颜色
    };

    void connectSignals();
    void setupUi();

    /// @brief 更新颜色按钮的背景色
    void updateColorButton(int index);

    QPushButton *m_defaultButton = nullptr;     ///< "恢复默认"按钮
    ChannelRow m_rows[kChannelCount];           ///< 8 通道控件行
};
