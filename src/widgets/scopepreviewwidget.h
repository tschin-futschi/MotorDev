// =============================================================================
// @file    scopepreviewwidget.h
// @brief   示波器预览控件 — 独立的示波器 UI 预览/演示窗口
//
// ScopePreviewWidget 提供一个独立的示波器预览环境，用于 UI 开发和演示：
// - 包含完整的 TopBar + PlotWidget + BottomPanel + StylePanel
// - 内置正弦波数据生成器，不依赖真实串口连接
// - 支持通道配置、采样控制、视图模式切换等完整交互
// =============================================================================
#pragma once

#include <QColor>
#include <QWidget>
#include <QVector>

class ScopeBottomPanel;
class ScopePlotWidget;
class ScopeStylePanel;
class QTimer;
class QSplitter;
class TopBar;
class QWidget;

/// @brief 示波器独立预览控件 — 自带模拟数据源的完整示波器 UI
///
/// 不依赖 SerialManager 或 CommandDispatcher，通过定时器生成正弦波数据
/// 驱动 PlotWidget 绘制，用于 UI 开发和功能验证。
class ScopePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScopePreviewWidget(QWidget *parent = nullptr);
    ~ScopePreviewWidget() override;

private:
    /// @brief 通道状态（预览控件内部维护，不使用 ScopeChannelModel）
    struct ChannelState {
        bool enabled = false;           ///< 通道启用
        QString description;            ///< 通道描述
        QString addressText;            ///< 通道地址文本
        QColor color;                   ///< 线条颜色
        qreal lineWidth = 4.0;          ///< 线条宽度
        Qt::PenStyle lineStyle = Qt::SolidLine; ///< 线条样式
        bool showDataPoints = false;    ///< 是否显示数据点
    };

    void setupUi();
    void connectSignals();

    /// @brief 从 m_channels 同步到 PlotWidget
    void refreshPlotData();

    /// @brief 设置采样运行状态
    void setRunning(bool running);

    /// @brief 生成并注入一帧预览数据（正弦波）
    void appendPreviewFrame();

    /// @brief 重置通道样式为默认
    void resetStyleDefaults();

    /// @brief 将采样间隔文本转为微秒数
    int sampleIntervalUsForText(const QString &text) const;

    /// @brief 将显示窗口文本转为毫秒数
    int displayWindowMsForText(const QString &text) const;

    /// @brief 计算当前启用通道的掩码
    uint8_t currentChannelMask() const;

    // --- UI 控件 ---
    TopBar *m_topBar = nullptr;                 ///< 顶部栏
    QSplitter *m_splitter = nullptr;            ///< 水平分割器
    QWidget *m_plotArea = nullptr;              ///< 波形区容器
    ScopePlotWidget *m_plotWidget = nullptr;    ///< 波形绘制控件
    QWidget *m_bottomPanelHost = nullptr;       ///< 底部面板宿主
    ScopeStylePanel *m_stylePanel = nullptr;    ///< 样式面板
    ScopeBottomPanel *m_bottomPanel = nullptr;  ///< 底部控制面板

    // --- 预览状态 ---
    QTimer *m_previewTimer = nullptr;           ///< 数据生成定时器
    QVector<ChannelState> m_channels;           ///< 当前通道状态
    QVector<ChannelState> m_defaultChannels;    ///< 默认通道状态（用于重置）
    int m_sampleIntervalUs = 1000;              ///< 采样间隔（微秒）
    int m_displayWindowMs = 50;                 ///< 显示窗口（毫秒）
    int m_phase = 0;                            ///< 正弦波相位累加器
    bool m_running = false;                     ///< 采样运行状态
};
