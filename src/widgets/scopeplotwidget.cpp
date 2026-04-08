#include "widgets/scopeplotwidget.h"

#include "ui/style_constants.h"

#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

#include <cmath>

using namespace MotorDev;

ScopePlotWidget::ScopePlotWidget(QWidget *parent)
    : QWidget(parent) {
    setAutoFillBackground(false);
    setMinimumHeight(320);
}

QSize ScopePlotWidget::minimumSizeHint() const {
    return {720, 320};
}

void ScopePlotWidget::setRunning(bool running) {
    if (m_running == running) {
        return;
    }
    m_running = running;
    update();
}

void ScopePlotWidget::setViewMode(ScopeToolBar::ViewMode mode) {
    if (m_viewMode == mode) {
        return;
    }
    m_viewMode = mode;
    update();
}

void ScopePlotWidget::setAutoYAxisRange() {
    m_autoYRange = true;
    update();
}

void ScopePlotWidget::setManualYAxisRange(double minValue, double maxValue) {
    if (minValue >= maxValue) {
        return;
    }

    m_autoYRange = false;
    m_manualYMin = minValue;
    m_manualYMax = maxValue;
    m_yViewMin = minValue;
    m_yViewMax = maxValue;
    update();
}

void ScopePlotWidget::setChannelData(const QVector<PlotChannelData> &channels) {
    m_channels = channels;
    update();
}

void ScopePlotWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Style::Color::ScopeBackground);

    const QRect frameRect = rect().adjusted(8, 8, -8, -8);
    painter.setPen(QPen(Style::Color::ScopeFrameBorder, 1));
    painter.setBrush(Style::Color::ScopeFrameBackground);
    painter.drawRoundedRect(frameRect, 6, 6);

    const QRect plotRect = frameRect.adjusted(44, 14, -14, -32);
    double yMin = m_yViewMin;
    double yMax = m_yViewMax;
    if (m_autoYRange) {
        calculateAutoYRange(yMin, yMax);
    }
    paintGrid(&painter, plotRect);

    if (m_viewMode == ScopeToolBar::ViewMode::Overlay) {
        painter.save();
        painter.setClipRect(plotRect);
        for (const PlotChannelData &channel : m_channels) {
            painter.setPen(QPen(channel.color, 1.6));
            painter.drawPolyline(waveformPolygon(plotRect, channel.samples, yMin, yMax));
        }
        painter.restore();
        paintLegend(&painter, plotRect);
    } else {
        painter.save();
        painter.setClipRect(plotRect);

        const int channelCount = m_channels.size();
        if (channelCount > 0) {
            const int gap = 10;
            const int laneHeight = (plotRect.height() - gap * (channelCount - 1)) / channelCount;
            painter.setFont(QFont(QStringLiteral("Segoe UI"), 9));

            for (int i = 0; i < channelCount; ++i) {
                const QRect laneRect(plotRect.left(),
                                     plotRect.top() + i * (laneHeight + gap),
                                     plotRect.width(),
                                     laneHeight);

                painter.setPen(QPen(Style::Color::ScopeStackedLaneBorder, 1));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(laneRect, 4, 4);

                painter.setPen(QPen(m_channels.at(i).color, 1.5));
                painter.drawPolyline(waveformPolygon(laneRect.adjusted(0, 4, 0, -4), m_channels.at(i).samples, yMin, yMax));

                painter.setPen(Style::Color::ScopeTextHeader);
                painter.drawText(laneRect.adjusted(8, 6, -8, -6),
                                 Qt::AlignTop | Qt::AlignLeft,
                                 m_channels.at(i).name);
            }
        }

        painter.restore();
    }

    paintSelection(&painter, plotRect);
    const double previousMin = m_yViewMin;
    const double previousMax = m_yViewMax;
    m_yViewMin = yMin;
    m_yViewMax = yMax;
    paintYAxis(&painter, plotRect);
    m_yViewMin = previousMin;
    m_yViewMax = previousMax;
    paintTimeAxis(&painter, frameRect);

    painter.setPen(Style::Color::ScopeTextMuted);
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    painter.drawText(frameRect.adjusted(12, 8, -12, -8),
                     Qt::AlignTop | Qt::AlignLeft,
                     m_running ? tr("Live scope preview") : tr("Scope idle preview"));
}

void ScopePlotWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRect plotRect = currentPlotRect();
    if (!plotRect.contains(event->position().toPoint())) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragSelecting = true;
    m_dragStart = event->position().toPoint();
    m_dragCurrent = m_dragStart;
    m_dragMode = DragZoomMode::None;
    update();
    event->accept();
}

void ScopePlotWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragSelecting) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    m_dragCurrent = event->position().toPoint();
    const int dx = std::abs(m_dragCurrent.x() - m_dragStart.x());
    const int dy = std::abs(m_dragCurrent.y() - m_dragStart.y());
    if (dx >= 6 || dy >= 6) {
        m_dragMode = dx >= dy ? DragZoomMode::Horizontal : DragZoomMode::Vertical;
    }
    update();
    event->accept();
}

void ScopePlotWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_dragSelecting || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const QRect plotRect = currentPlotRect();
    const int left = qBound(plotRect.left(), qMin(m_dragStart.x(), m_dragCurrent.x()), plotRect.right());
    const int right = qBound(plotRect.left(), qMax(m_dragStart.x(), m_dragCurrent.x()), plotRect.right());
    const int top = qBound(plotRect.top(), qMin(m_dragStart.y(), m_dragCurrent.y()), plotRect.bottom());
    const int bottom = qBound(plotRect.top(), qMax(m_dragStart.y(), m_dragCurrent.y()), plotRect.bottom());
    const int dragWidth = right - left;
    const int dragHeight = bottom - top;

    if (m_dragMode == DragZoomMode::Horizontal && dragWidth >= 18) {
        const double localStart = static_cast<double>(left - plotRect.left()) / static_cast<double>(plotRect.width());
        const double localEnd = static_cast<double>(right - plotRect.left()) / static_cast<double>(plotRect.width());
        const double currentSpan = m_viewEndRatio - m_viewStartRatio;

        const double nextStart = m_viewStartRatio + currentSpan * localStart;
        const double nextEnd = m_viewStartRatio + currentSpan * localEnd;

        m_viewStartRatio = qBound(0.0, nextStart, 0.98);
        m_viewEndRatio = qBound(m_viewStartRatio + 0.02, nextEnd, 1.0);
    } else if (m_dragMode == DragZoomMode::Vertical && dragHeight >= 18) {
        const double localTop = static_cast<double>(top - plotRect.top()) / static_cast<double>(plotRect.height());
        const double localBottom = static_cast<double>(bottom - plotRect.top()) / static_cast<double>(plotRect.height());
        double currentMin = m_yViewMin;
        double currentMax = m_yViewMax;
        if (m_autoYRange) {
            calculateAutoYRange(currentMin, currentMax);
        }
        const double currentSpan = currentMax - currentMin;

        const double nextMax = currentMax - currentSpan * localTop;
        const double nextMin = currentMax - currentSpan * localBottom;

        m_autoYRange = false;
        m_manualYMin = nextMin;
        m_manualYMax = nextMax;
        m_yViewMin = nextMin;
        m_yViewMax = qMax(nextMax, nextMin + 0.05);
    }

    m_dragSelecting = false;
    m_dragMode = DragZoomMode::None;
    update();
    event->accept();
}

void ScopePlotWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && currentPlotRect().contains(event->position().toPoint())) {
        m_dragSelecting = false;
        m_dragMode = DragZoomMode::None;
        m_viewStartRatio = 0.0;
        m_viewEndRatio = 1.0;
        if (!m_autoYRange) {
            m_yViewMin = m_manualYMin;
            m_yViewMax = m_manualYMax;
        }
        update();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void ScopePlotWidget::paintGrid(QPainter *painter, const QRect &rect) {
    painter->save();
    painter->setClipRect(rect);
    painter->fillRect(rect, Style::Color::ScopePanelBackground);

    QPen minorPen(Style::Color::ScopeGridMinor, 1);
    QPen majorPen(Style::Color::ScopeGridMajor, 1);

    for (int col = 0; col <= 10; ++col) {
        painter->setPen(col % 5 == 0 ? majorPen : minorPen);
        const int x = rect.left() + (rect.width() * col) / 10;
        painter->drawLine(x, rect.top(), x, rect.bottom());
    }

    for (int row = 0; row <= 8; ++row) {
        painter->setPen(row % 4 == 0 ? majorPen : minorPen);
        const int y = rect.top() + (rect.height() * row) / 8;
        painter->drawLine(rect.left(), y, rect.right(), y);
    }

    painter->restore();
}

void ScopePlotWidget::paintOverlay(QPainter *painter, const QRect &rect) {
    painter->save();
    painter->setClipRect(rect);

    for (const PlotChannelData &channel : m_channels) {
        painter->setPen(QPen(channel.color, 1.6));
        painter->drawPolyline(waveformPolygon(rect, channel.samples, m_yViewMin, m_yViewMax));
    }

    painter->restore();
}

void ScopePlotWidget::paintStacked(QPainter *painter, const QRect &rect) {
    painter->save();
    painter->setClipRect(rect);

    const int channelCount = m_channels.size();
    if (channelCount == 0) {
        painter->restore();
        return;
    }

    const int gap = 10;
    const int laneHeight = (rect.height() - gap * (channelCount - 1)) / channelCount;
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 9));

    for (int i = 0; i < channelCount; ++i) {
        const QRect laneRect(rect.left(),
                             rect.top() + i * (laneHeight + gap),
                             rect.width(),
                             laneHeight);

        painter->setPen(QPen(Style::Color::ScopeStackedLaneBorder, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(laneRect, 4, 4);

        painter->setPen(QPen(m_channels.at(i).color, 1.5));
        painter->drawPolyline(waveformPolygon(laneRect.adjusted(0, 4, 0, -4), m_channels.at(i).samples, m_yViewMin, m_yViewMax));

        painter->setPen(Style::Color::ScopeTextHeader);
        painter->drawText(laneRect.adjusted(8, 6, -8, -6),
                          Qt::AlignTop | Qt::AlignLeft,
                          m_channels.at(i).name);
    }

    painter->restore();
}

void ScopePlotWidget::paintTimeAxis(QPainter *painter, const QRect &rect) {
    const QRect axisRect(rect.left() + 48, rect.bottom() - 24, rect.width() - 62, 20);
    painter->setPen(Style::Color::ScopeTextSubtle);
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));

    for (int i = 0; i <= 8; ++i) {
        const int x = axisRect.left() + (axisRect.width() * i) / 8;
        painter->drawLine(x, axisRect.top(), x, axisRect.top() + 5);
        const double timeValue = (m_viewStartRatio + (m_viewEndRatio - m_viewStartRatio) * (static_cast<double>(i) / 8.0)) * 40.0;
        const QString label = QStringLiteral("%1 ms").arg(QString::number(timeValue, 'f', timeValue < 10.0 ? 1 : 0));
        painter->drawText(x - 18, axisRect.top() + 8, 40, 12, Qt::AlignHCenter | Qt::AlignTop, label);
    }
}

void ScopePlotWidget::paintYAxis(QPainter *painter, const QRect &rect) {
    painter->setPen(Style::Color::ScopeTextSubtle);
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));

    const int tickX = rect.left();
    const int labelRight = rect.left() - 8;
    const QList<double> values = {
        m_yViewMax,
        m_yViewMin + (m_yViewMax - m_yViewMin) * 0.75,
        m_yViewMin + (m_yViewMax - m_yViewMin) * 0.5,
        m_yViewMin + (m_yViewMax - m_yViewMin) * 0.25,
        m_yViewMin};

    for (int i = 0; i < values.size(); ++i) {
        const double ratio = static_cast<double>(i) / static_cast<double>(values.size() - 1);
        const int y = rect.top() + qRound(ratio * rect.height());
        painter->drawLine(tickX - 4, y, tickX, y);
        painter->drawText(labelRight - 30, y - 7, 28, 14, Qt::AlignRight | Qt::AlignVCenter,
                          QString::number(values.at(i), 'f', 1));
    }
}

void ScopePlotWidget::paintLegend(QPainter *painter, const QRect &rect) {
    const int itemWidth = 98;
    const int itemHeight = 18;
    const int top = rect.top() + 8;
    int left = rect.left() + 10;

    painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));
    for (const PlotChannelData &channel : m_channels) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(Style::Color::ScopeLegendBackground);
        painter->drawRoundedRect(QRect(left, top, itemWidth, itemHeight), 4, 4);
        painter->setBrush(channel.color);
        painter->drawRect(left + 6, top + 5, 10, 8);
        painter->setPen(Style::Color::ScopeTextDark);
        painter->drawText(QRect(left + 22, top + 2, itemWidth - 24, itemHeight - 4),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          channel.name.left(10));
        left += itemWidth + 6;
    }
}

void ScopePlotWidget::paintSelection(QPainter *painter, const QRect &rect) {
    if (!m_dragSelecting) {
        return;
    }

    QRect selectionRect;
    if (m_dragMode == DragZoomMode::Vertical) {
        const int top = qBound(rect.top(), qMin(m_dragStart.y(), m_dragCurrent.y()), rect.bottom());
        const int bottom = qBound(rect.top(), qMax(m_dragStart.y(), m_dragCurrent.y()), rect.bottom());
        if (bottom <= top) {
            return;
        }
        selectionRect = QRect(rect.left(), top, rect.width(), bottom - top);
    } else {
        const int left = qBound(rect.left(), qMin(m_dragStart.x(), m_dragCurrent.x()), rect.right());
        const int right = qBound(rect.left(), qMax(m_dragStart.x(), m_dragCurrent.x()), rect.right());
        if (right <= left) {
            return;
        }
        selectionRect = QRect(left, rect.top(), right - left, rect.height());
    }

    painter->save();
    painter->setPen(QPen(Style::Color::ScopeSelectionBorder, 1, Qt::DashLine));
    painter->setBrush(Style::Color::ScopeSelectionFill);
    painter->drawRect(selectionRect);
    painter->restore();
}

QRect ScopePlotWidget::currentPlotRect() const {
    const QRect frameRect = rect().adjusted(8, 8, -8, -8);
    return frameRect.adjusted(44, 14, -14, -32);
}

int ScopePlotWidget::longestSampleCount() const {
    int maxCount = 0;
    for (const PlotChannelData &channel : m_channels) {
        maxCount = qMax(maxCount, channel.samples.size());
    }
    return maxCount;
}

int ScopePlotWidget::baseWindowSampleCount() const {
    constexpr int DefaultVisibleSamples = 160;
    const int total = longestSampleCount();
    if (total <= 0) {
        return 0;
    }
    return qMin(DefaultVisibleSamples, total);
}

void ScopePlotWidget::calculateAutoYRange(double &minValue, double &maxValue) const {
    if (m_channels.isEmpty()) {
        minValue = -1.0;
        maxValue = 1.0;
        return;
    }

    const int startIndex = visibleSampleStart();
    const int endIndex = qMax(startIndex, visibleSampleEnd());
    bool hasValue = false;
    double dataMin = 0.0;
    double dataMax = 0.0;

    for (const PlotChannelData &channel : m_channels) {
        for (int i = startIndex; i <= endIndex && i < channel.samples.size(); ++i) {
            const double value = channel.samples.at(i);
            if (!hasValue) {
                dataMin = value;
                dataMax = value;
                hasValue = true;
            } else {
                dataMin = qMin(dataMin, value);
                dataMax = qMax(dataMax, value);
            }
        }
    }

    if (!hasValue) {
        minValue = -1.0;
        maxValue = 1.0;
        return;
    }

    const double span = qMax(0.1, dataMax - dataMin);
    const double padding = qMax(0.05, span * 0.1);
    minValue = dataMin - padding;
    maxValue = dataMax + padding;
}

int ScopePlotWidget::visibleSampleStart() const {
    const int total = longestSampleCount();
    const int windowSamples = baseWindowSampleCount();
    if (total <= 0 || windowSamples <= 0) {
        return 0;
    }

    const int baseStart = qMax(0, total - windowSamples);
    const int windowLastIndex = qMax(0, windowSamples - 1);
    const int offset = static_cast<int>(std::floor(m_viewStartRatio * windowLastIndex));
    return qBound(0, baseStart + offset, total - 1);
}

int ScopePlotWidget::visibleSampleEnd() const {
    const int total = longestSampleCount();
    const int windowSamples = baseWindowSampleCount();
    if (total <= 0 || windowSamples <= 0) {
        return 0;
    }

    const int baseStart = qMax(0, total - windowSamples);
    const int windowLastIndex = qMax(0, windowSamples - 1);
    const int offset = static_cast<int>(std::ceil(m_viewEndRatio * windowLastIndex));
    return qBound(0, baseStart + offset, total - 1);
}

QPolygonF ScopePlotWidget::waveformPolygon(const QRect &rect, const QVector<double> &samples, double minValue, double maxValue) const {
    QPolygonF polygon;
    if (samples.isEmpty()) {
        return polygon;
    }

    const int startIndex = qMin(visibleSampleStart(), qMax(0, samples.size() - 1));
    const int endIndex = qMin(qMax(startIndex + 1, visibleSampleEnd()), qMax(0, samples.size() - 1));
    const int visibleCount = endIndex - startIndex + 1;
    polygon.reserve(visibleCount);

    for (int i = startIndex; i <= endIndex && i < samples.size(); ++i) {
        const double tx = visibleCount == 1
                              ? 0.0
                              : static_cast<double>(i - startIndex) / static_cast<double>(visibleCount - 1);
        const double x = rect.left() + tx * rect.width();
        const double normalized = (samples.at(i) - minValue) / (maxValue - minValue);
        const double clamped = qBound(0.0, normalized, 1.0);
        const double y = rect.bottom() - clamped * rect.height();
        polygon.append(QPointF(x, y));
    }
    return polygon;
}
