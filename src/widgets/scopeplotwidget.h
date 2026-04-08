#pragma once

#include "widgets/scopetoolbar.h"

#include <QColor>
#include <QPoint>
#include <QPolygonF>
#include <QRect>
#include <QVector>
#include <QWidget>

class ScopePlotWidget : public QWidget {
    Q_OBJECT

public:
    struct PlotChannelData {
        QString name;
        QColor color;
        QVector<double> samples;
    };

    explicit ScopePlotWidget(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;

public slots:
    void setRunning(bool running);
    void setViewMode(ScopeToolBar::ViewMode mode);
    void setAutoYAxisRange();
    void setManualYAxisRange(double minValue, double maxValue);
    void setChannelData(const QVector<PlotChannelData> &channels);

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

    void paintGrid(QPainter *painter, const QRect &rect);
    void paintOverlay(QPainter *painter, const QRect &rect);
    void paintStacked(QPainter *painter, const QRect &rect);
    void paintTimeAxis(QPainter *painter, const QRect &rect);
    void paintYAxis(QPainter *painter, const QRect &rect);
    void paintLegend(QPainter *painter, const QRect &rect);
    void paintSelection(QPainter *painter, const QRect &rect);
    void calculateAutoYRange(double &minValue, double &maxValue) const;
    QRect currentPlotRect() const;
    int longestSampleCount() const;
    int baseWindowSampleCount() const;
    int visibleSampleStart() const;
    int visibleSampleEnd() const;
    QPolygonF waveformPolygon(const QRect &rect, const QVector<double> &samples, double minValue, double maxValue) const;

    QVector<PlotChannelData> m_channels;
    ScopeToolBar::ViewMode m_viewMode = ScopeToolBar::ViewMode::Overlay;
    bool m_running = false;
    bool m_autoYRange = true;
    bool m_dragSelecting = false;
    QPoint m_dragStart;
    QPoint m_dragCurrent;
    double m_viewStartRatio = 0.0;
    double m_viewEndRatio = 1.0;
    double m_yViewMin = -1.0;
    double m_yViewMax = 1.0;
    double m_manualYMin = -1.0;
    double m_manualYMax = 1.0;
    DragZoomMode m_dragMode = DragZoomMode::None;
};
