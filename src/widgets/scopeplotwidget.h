#pragma once

#include "widgets/scopetoolbar.h"

#include <array>
#include <cstdint>
#include <vector>

#include <QColor>
#include <QElapsedTimer>
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
    };

    explicit ScopePlotWidget(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;

    // Performance counters exposed for the OscilloscopTab 1 Hz log.
    double paintAverageMs() const { return m_avgFrameMs; }
    double paintFps() const { return m_avgFrameMs > 0.0 ? 1000.0 / m_avgFrameMs : 0.0; }

public slots:
    void setRunning(bool running);
    void setViewMode(ScopeToolBar::ViewMode mode);
    void setAutoYAxisRange();
    void setManualYAxisRange(double minValue, double maxValue);
    void setChannelData(const QVector<PlotChannelData> &channels);
    void configureAcquisition(int intervalUs, int windowMs);
    void appendSamples(uint8_t channelMask, const QVector<int16_t> &samples);
    void clearData();
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

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
        // Raw ring is kept for compatibility with previous design and possible
        // future inspection use. It is NOT read in the paint hot path.
        QVector<double> rawRing;
        int rawHead = 0;
        int rawCount = 0;
        // Committed median values used by the renderer. Fixed capacity ring,
        // time order is recovered via uiHead / uiCount in buildPaintSnapshot.
        std::array<float, kUiRingSize> uiRing {};
        int uiHead = 0;
        int uiCount = 0;
        // Bucket accumulator. Fixed capacity = bucketWidth, set in
        // configureAcquisition. The hot path only writes into bucketBuffer
        // by index, never appends, so there is no allocation per sample.
        std::vector<double> bucketBuffer;
        int bucketFill = 0;
        bool enabled = false;
        qreal lineWidth = 4.0;
        Qt::PenStyle lineStyle = Qt::SolidLine;
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

    // Build the time-ordered, fixed length [kUiRingSize] view of channel `c`
    // into m_paintSnapshot[c]. Leading slots (when uiCount < kUiRingSize)
    // are zero-filled. After this call the visible-window code can index by
    // [start..end] without any extra modulo arithmetic.
    void buildPaintSnapshot(int channelIndex);

    // Compute auto Y range from m_paintSnapshot for all currently enabled
    // channels within [start, end]. Operates on the very same buffer the
    // renderer is about to draw, so it is a single linear scan.
    void computeAutoYFromSnapshot(int startIndex, int endIndex,
                                  double &minValue, double &maxValue) const;

    void drawWaveformLane(QPainter *painter, const QRect &laneRect,
                          int channelIndex, double yMin, double yMax,
                          int startIndex, int endIndex);

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
    ScopeToolBar::ViewMode m_viewMode = ScopeToolBar::ViewMode::Overlay;
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

    // Reused across frames to avoid per-frame allocation.
    // m_paintSnapshot is channel-major and time-ordered (oldest .. newest).
    // m_pointBuffer is rebuilt per channel via index-only writes.
    mutable std::array<std::array<float, kUiRingSize>, kMaxChannels> m_paintSnapshot {};
    mutable std::array<int, kMaxChannels> m_paintSnapshotCount {};
    mutable std::array<int, kMaxChannels> m_paintSnapshotOffset {};
    mutable std::vector<QPointF> m_pointBuffer;

    // UI-driven render timer. Always running; interval is faster while the
    // scope is running and slower while idle. Per project plan, all data and
    // state changes never call update() directly -- the timer is the only
    // trigger.
    QTimer *m_renderTimer = nullptr;

    // Frame time measurement. Exponential moving average, alpha = 0.1.
    mutable QElapsedTimer m_frameClock;
    mutable double m_avgFrameMs = 0.0;
    mutable int m_frameSamples = 0;

    // Auto Y throttle clock.
    mutable QElapsedTimer m_autoYClock;
};
