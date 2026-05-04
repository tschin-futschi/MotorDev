// =============================================================================
// @file    scopeplotwidget.h
// @brief   示波器波形绘制控件 — OpenGL 加速的实时多通道波形显示
//
// ScopePlotWidget 是示波器的核心绘制控件，基于 QOpenGLWidget 实现：
// - 最多 8 通道实时波形绘制
// - 两种视图模式：Overlay（叠加）和 Stacked（堆叠）
// - 支持鼠标拖拽缩放（水平/垂直）、双击全屏、右键菜单重置
// - 自动/手动 Y 轴范围
// - 采样按钮嵌入在绘图区内
// - 性能统计：平均帧时、FPS
//
// 数据流：
// ScopeService → appendSamples() → ChannelBuffer 环形缓冲 → paintEvent() 绘制
//
// 渲染优化：
// - 使用 paint snapshot 机制避免绘制时锁竞争
// - 30fps 定时器驱动刷新，非每帧都触发重绘
// - cosmetic pen + FlatCap + BevelJoin（Windows 性能关键）
// =============================================================================
#pragma once

#include "models/channelbuffer.h"
#include "ui/scopeviewmode.h"

#include <cstdint>
#include <vector>

#include <QColor>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QResizeEvent>
#include <QString>
#include <QTimer>
#include <QVector>

/// @brief 示波器波形绘制控件（OpenGL 加速）
///
/// 继承 QOpenGLWidget，通过 QPainter 在 OpenGL surface 上绘制波形。
/// 内置 ChannelBuffer（双环形缓冲）作为每通道的数据存储。
class ScopePlotWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    /// @brief 通道绘制参数（由 OscilloscopTab 设置）
    struct PlotChannelData {
        int index = -1;                             ///< 通道索引（0~7）
        QString name;                               ///< 通道名称（图例显示）
        QColor color;                               ///< 线条颜色
        bool enabled = false;                       ///< 是否启用
        qreal lineWidth = 4.0;                      ///< 线条宽度
        Qt::PenStyle lineStyle = Qt::SolidLine;     ///< 线条样式
        bool showDataPoints = false;                ///< 是否显示数据点
    };

    explicit ScopePlotWidget(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;

    /// @brief 获取平均帧绘制耗时（毫秒）
    double paintAverageMs() const { return m_avgFrameMs; }

    /// @brief 获取绘制帧率（FPS）
    double paintFps() const { return m_avgFrameMs > 0.0 ? 1000.0 / m_avgFrameMs : 0.0; }

public slots:
    /// @brief 设置采样运行状态（控制刷新定时器和采样按钮）
    void setRunning(bool running);

    /// @brief 设置视图模式（Overlay/Stacked）
    void setViewMode(ScopeViewMode mode);

    /// @brief 设置 Y 轴为自动范围模式
    void setAutoYAxisRange();

    /// @brief 设置 Y 轴为手动范围
    void setManualYAxisRange(double minValue, double maxValue);

    /// @brief 更新通道配置（名称、颜色、线型等）
    void setChannelData(const QVector<PlotChannelData> &channels);

    /// @brief 配置采集参数（采样间隔和显示窗口）
    /// @param intervalUs 采样间隔（微秒）
    /// @param windowMs 显示窗口（毫秒）
    void configureAcquisition(int intervalUs, int windowMs);

    /// @brief 追加采样数据到环形缓冲
    /// @param channelMask 通道掩码（bit0~bit7 对应 CH1~CH8）
    /// @param samples 各通道交错排列的采样数据
    void appendSamples(uint8_t channelMask, const QVector<int16_t> &samples);

    /// @brief 清空所有通道数据
    void clearData();

    /// @brief 重置视图（清空数据 + 重置缩放）
    void resetView();

signals:
    /// @brief 全屏切换请求（双击触发）
    void fullscreenToggleRequested();

    /// @brief 采样启停请求
    /// @param running true=启动，false=停止
    void samplingToggleRequested(bool running);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    /// @brief 拖拽缩放方向
    enum class DragZoomMode {
        None,           ///< 无拖拽
        Horizontal,     ///< 水平缩放（时间轴）
        Vertical        ///< 垂直缩放（Y 轴）
    };

    static constexpr int kMaxChannels = 8;              ///< 最大通道数
    static constexpr qint64 kAutoYMinIntervalMs = 100;  ///< 自动 Y 轴更新最小间隔

    /// @brief 内部通道数据（名称 + 颜色 + 缓冲区 + 样式）
    struct ChannelData {
        QString name;                       ///< 通道名称
        QColor color;                       ///< 线条颜色
        ChannelBuffer buffer;               ///< 双环形缓冲区
        bool enabled = false;               ///< 是否启用
        qreal lineWidth = 4.0;              ///< 线条宽度
        Qt::PenStyle lineStyle = Qt::SolidLine; ///< 线条样式
        bool showDataPoints = false;        ///< 是否显示数据点
    };

    // --- 绘制方法 ---
    void paintGrid(QPainter *painter, const QRect &rect);
    void paintOverlay(QPainter *painter, const QRect &rect, double yMin, double yMax);
    void paintStacked(QPainter *painter, const QRect &rect, double yMin, double yMax);
    void paintTimeAxis(QPainter *painter, const QRect &rect);
    void paintYAxis(QPainter *painter, const QRect &rect);
    void paintLegend(QPainter *painter, const QRect &rect);
    void paintSelection(QPainter *painter, const QRect &rect);
    void paintFrameTimeReadout(QPainter *painter, const QRect &rect);
    void paintCrosshair(QPainter *painter, const QRect &plotRect, double yMin, double yMax);
    void paintStaticFrame(QPainter *painter, const QRect &frameRect, const QRect &plotRect);

    // --- 数据快照（避免绘制时锁竞争）---
    void buildPaintSnapshot(int channelIndex);
    void computeAutoYFromSnapshot(int startIndex, int endIndex,
                                  double &minValue, double &maxValue) const;

    /// @brief 绘制单通道波形到指定矩形区域
    /// @return 实际绘制的点数
    int drawWaveformLane(QPainter *painter, const QRect &laneRect,
                         int channelIndex, double yMin, double yMax,
                         int startIndex, int endIndex);

    /// @brief 绘制数据点圆点标记
    void drawDataPointDots(QPainter *painter, const QColor &color, int pointCount);

    // --- 视图控制 ---
    void resetZoom();
    void markAutoYDirty();
    QRect currentPlotRect() const;
    int visibleSampleStart() const;
    int visibleSampleEnd() const;
    void updateSamplingButton();
    void updateSamplingButtonGeometry();

    // --- 成员变量 ---
    QVector<ChannelData> m_channels;                ///< 通道数据数组
    QPushButton *m_samplingButton = nullptr;        ///< 采样启停按钮
    ScopeViewMode m_viewMode = ScopeViewMode::Overlay; ///< 视图模式
    bool m_running = false;                         ///< 采样运行状态
    bool m_autoYRange = true;                       ///< 当前是否自动 Y 轴
    bool m_yRangeSettingAuto = true;                ///< Y 轴设置（非临时缩放状态）
    bool m_dragSelecting = false;                   ///< 是否正在拖拽选择
    QPoint m_dragStart;                             ///< 拖拽起点
    QPoint m_dragCurrent;                           ///< 拖拽当前点
    double m_viewStartRatio = 0.0;                  ///< X 轴视图起始比例
    double m_viewEndRatio = 1.0;                    ///< X 轴视图结束比例
    double m_yViewMin = -1.0;                       ///< Y 轴视图最小值
    double m_yViewMax = 1.0;                        ///< Y 轴视图最大值
    double m_manualYMin = -1.0;                     ///< 手动 Y 轴最小值
    double m_manualYMax = 1.0;                      ///< 手动 Y 轴最大值
    int m_sampleIntervalUs = 1000;                  ///< 采样间隔（微秒）
    int m_displayWindowMs = 50;                     ///< 显示窗口（毫秒）
    int m_rawWindowSampleCount = 50;                ///< 原始窗口采样点数
    int m_bucketWidth = 1;                          ///< 降采样桶宽度
    int m_displaySampleCount = 50;                  ///< 显示用采样点数（降采样后）
    DragZoomMode m_dragMode = DragZoomMode::None;   ///< 当前拖拽模式
    bool m_autoYDirty = true;                       ///< 自动 Y 轴是否需要重算
    double m_cachedAutoYMin = -1.0;                 ///< 缓存的自动 Y 最小值
    double m_cachedAutoYMax = 1.0;                  ///< 缓存的自动 Y 最大值
    bool m_staticCacheDirty = true;                 ///< 静态帧缓存是否脏

    /// @brief 绘制快照数组（每通道 kUiRingSize 个 float）
    mutable std::array<std::array<float, ChannelBuffer::kUiRingSize>, kMaxChannels> m_paintSnapshot {};
    mutable std::array<int, kMaxChannels> m_paintSnapshotCount {};   ///< 快照有效点数
    mutable std::array<int, kMaxChannels> m_paintSnapshotOffset {};  ///< 快照起始偏移
    mutable std::vector<QPointF> m_pointBuffer;     ///< drawPolyline 共享点缓冲

    // --- 十字准线 ---
    QPoint m_cursorPos;                             ///< 鼠标当前位置
    bool m_cursorInPlot = false;                    ///< 鼠标是否在绘图区内

    // --- 帧率统计 ---
    QTimer *m_renderTimer = nullptr;                ///< 渲染刷新定时器
    mutable QElapsedTimer m_frameClock;             ///< 帧时计时器
    mutable double m_avgFrameMs = 0.0;              ///< 平均帧时（指数移动平均）
    mutable int m_frameSamples = 0;                 ///< 帧采样计数
    mutable QElapsedTimer m_autoYClock;             ///< 自动 Y 轴更新节流计时器
};
