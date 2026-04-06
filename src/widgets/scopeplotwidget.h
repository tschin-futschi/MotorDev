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
    explicit ScopePlotWidget(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;

public slots:
    void setRunning(bool running);
    void setViewMode(ScopeToolBar::ViewMode mode);

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

    struct ChannelData {
        QString name;
        QColor color;
        QVector<double> samples;
    };

    void paintGrid(QPainter *painter, const QRect &rect);
    void paintOverlay(QPainter *painter, const QRect &rect);
    void paintStacked(QPainter *painter, const QRect &rect);
    void paintTimeAxis(QPainter *painter, const QRect &rect);
    void paintYAxis(QPainter *painter, const QRect &rect);
    void paintLegend(QPainter *painter, const QRect &rect);
    void paintSelection(QPainter *painter, const QRect &rect);
    QRect currentPlotRect() const;
    int visibleSampleStart() const;
    int visibleSampleEnd() const;
    QPolygonF waveformPolygon(const QRect &rect, const QVector<double> &samples) const;

    QVector<ChannelData> m_channels;
    ScopeToolBar::ViewMode m_viewMode = ScopeToolBar::ViewMode::Overlay;
    bool m_running = false;
    bool m_dragSelecting = false;
    QPoint m_dragStart;
    QPoint m_dragCurrent;
    double m_viewStartRatio = 0.0;
    double m_viewEndRatio = 1.0;
    double m_yViewMin = -1.0;
    double m_yViewMax = 1.0;
    DragZoomMode m_dragMode = DragZoomMode::None;
};
