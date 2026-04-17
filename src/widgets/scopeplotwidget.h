#pragma once

#include "ui/scopeviewmode.h"

#include <array>
#include <cstdint>
#include <vector>

#include <QColor>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVector>

class ScopePlotWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    struct PlotChannelData {
        int index = -1;
        QString name;
        QColor color;
        bool enabled = false;
        qreal lineWidth = 4.0;
        Qt::PenStyle lineStyle = Qt::SolidLine;
        bool showDataPoints = false;
    };

    explicit ScopePlotWidget(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;

    // Performance counters exposed for the OscilloscopTab 1 Hz log.
    double paintAverageMs() const { return m_avgFrameMs; }
    double paintFps() const { return m_avgFrameMs > 0.0 ? 1000.0 / m_avgFrameMs : 0.0; }

public slots:
    void setRunning(bool running);
    void setViewMode(ScopeViewMode mode);
    void setAutoYAxisRange();
    void setManualYAxisRange(double minValue, double maxValue);
    void setChannelData(const QVector<PlotChannelData> &channels);
    void configureAcquisition(int intervalUs, int windowMs);
    void appendSamples(uint8_t channelMask, const QVector<int16_t> &samples);
    void clearData();
    void resetView();

signals:
    void fullscreenToggleRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class DragZoomMode {
        None,
        Horizontal,
        Vertical
    };

    static constexpr int kUiRingSize = 3000;
    static constexpr int kMaxChannels = 8;
    static constexpr qint64 kAutoYMinIntervalMs = 100;

    struct ChannelData {
        QString name;
        QColor color;
        QVector<double> rawRing;
        int rawHead = 0;
        int rawCount = 0;
        std::array<float, kUiRingSize> uiRing {};
        int uiHead = 0;
        int uiCount = 0;
        std::vector<double> bucketBuffer;
        int bucketFill = 0;
        bool enabled = false;
        qreal lineWidth = 4.0;
        Qt::PenStyle lineStyle = Qt::SolidLine;
        bool showDataPoints = false;
    };

    void paintGrid(QPainter *painter, const QRect &rect);
    void paintOverlay(QPainter *painter, const QRect &rect, double yMin, double yMax);
    void paintStacked(QPainter *painter, const QRect &rect, double yMin, double yMax);
    void paintTimeAxis(QPainter *painter, const QRect &rect);
    void paintYAxis(QPainter *painter, const QRect &rect);
    void paintLegend(QPainter *painter, const QRect &rect);
    void paintSelection(QPainter *painter, const QRect &rect);
    void paintFrameTimeReadout(QPainter *painter, const QRect &rect);
    void paintStaticFrame(QPainter *painter, const QRect &frameRect, const QRect &plotRect);
    void buildPaintSnapshot(int channelIndex);
    void computeAutoYFromSnapshot(int startIndex, int endIndex,
                                  double &minValue, double &maxValue) const;
    int drawWaveformLane(QPainter *painter, const QRect &laneRect,
                         int channelIndex, double yMin, double yMax,
                         int startIndex, int endIndex);
    void drawDataPointDots(QPainter *painter, const QColor &color, int pointCount);
    void resetZoom();
    void markAutoYDirty();
    QRect currentPlotRect() const;
    int rawWindowSampleCount() const;
    int bucketWidth() const;
    int displaySampleCount() const;
    int visibleSampleStart() const;
    int visibleSampleEnd() const;
    void resetChannelBuffers(ChannelData &channel);
    void writeDisplaySample(ChannelData &channel, double value);

    QVector<ChannelData> m_channels;
    ScopeViewMode m_viewMode = ScopeViewMode::Overlay;
    bool m_running = false;
    bool m_autoYRange = true;
    bool m_yRangeSettingAuto = true;
    bool m_dragSelecting = false;
    QPoint m_dragStart;
    QPoint m_dragCurrent;
    double m_viewStartRatio = 0.0;
    double m_viewEndRatio = 1.0;
    double m_yViewMin = -1.0;
    double m_yViewMax = 1.0;
    double m_manualYMin = -1.0;
    double m_manualYMax = 1.0;
    int m_sampleIntervalUs = 1000;
    int m_displayWindowMs = 50;
    DragZoomMode m_dragMode = DragZoomMode::None;
    bool m_autoYDirty = true;
    double m_cachedAutoYMin = -1.0;
    double m_cachedAutoYMax = 1.0;
    bool m_staticCacheDirty = true;
    mutable std::array<std::array<float, kUiRingSize>, kMaxChannels> m_paintSnapshot {};
    mutable std::array<int, kMaxChannels> m_paintSnapshotCount {};
    mutable std::array<int, kMaxChannels> m_paintSnapshotOffset {};
    mutable std::vector<QPointF> m_pointBuffer;
    QTimer *m_renderTimer = nullptr;
    mutable QElapsedTimer m_frameClock;
    mutable double m_avgFrameMs = 0.0;
    mutable int m_frameSamples = 0;
    mutable QElapsedTimer m_autoYClock;
};