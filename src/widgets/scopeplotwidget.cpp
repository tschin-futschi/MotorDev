#include "widgets/scopeplotwidget.h"

#include "ui/style_constants.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QSurfaceFormat>

using namespace MotorDev;

namespace {
// nth_element-based median over a fixed-size buffer (no allocation).
double medianValueInPlace(std::vector<double> &values, int count) {
    if (count <= 0) {
        return 0.0;
    }
    if (count == 1) {
        return values[0];
    }

    const int mid = count / 2;
    if ((count % 2) != 0) {
        std::nth_element(values.begin(), values.begin() + mid, values.begin() + count);
        return values[mid];
    }

    std::nth_element(values.begin(), values.begin() + mid, values.begin() + count);
    const double upper = values[mid];
    std::nth_element(values.begin(), values.begin() + mid - 1, values.begin() + mid);
    return (values[mid - 1] + upper) / 2.0;
}

void configureScopePen(QPen &pen, const QColor &color, qreal width, Qt::PenStyle style) {
    pen = QPen(color, width);
    pen.setCosmetic(true);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::BevelJoin);
    pen.setStyle(style);
}

int extraThicknessShells(qreal requestedWidth, qreal baseWidth) {
    if (requestedWidth <= baseWidth) {
        return 0;
    }

    return qMax(0, qCeil((requestedWidth - baseWidth) / 2.0));
}

constexpr QPoint kShellOneOffsets[] = {
    QPoint(-1, 0), QPoint(1, 0), QPoint(0, -1), QPoint(0, 1),
    QPoint(-1, -1), QPoint(1, -1), QPoint(-1, 1), QPoint(1, 1)
};

constexpr QPoint kShellTwoOffsets[] = {
    QPoint(-2, 0), QPoint(2, 0), QPoint(0, -2), QPoint(0, 2),
    QPoint(-2, -1), QPoint(-2, 1), QPoint(2, -1), QPoint(2, 1),
    QPoint(-1, -2), QPoint(1, -2), QPoint(-1, 2), QPoint(1, 2)
};
} // namespace

ScopePlotWidget::ScopePlotWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    m_channels.resize(kMaxChannels);
    m_pointBuffer.reserve(kUiRingSize);

    // Request 4x MSAA for smooth lines at near-zero GPU cost.
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    setFormat(fmt);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setTimerType(Qt::PreciseTimer);
    m_renderTimer->setInterval(16); // ~60 fps
    connect(m_renderTimer, &QTimer::timeout,
            this, QOverload<>::of(&QOpenGLWidget::update));
    m_renderTimer->start();

    clearData();
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
    // Slow the timer down a bit when idle to save CPU, but never stop it
    // entirely so the last frame stays visible and any state change still
    // gets repainted on the next tick.
    m_renderTimer->setInterval(running ? 16 : 100);
}

void ScopePlotWidget::setViewMode(ScopeToolBar::ViewMode mode) {
    if (m_viewMode == mode) {
        return;
    }
    m_viewMode = mode;
}

void ScopePlotWidget::setAutoYAxisRange() {
    m_yRangeSettingAuto = true;
    m_autoYRange = true;
    m_autoYDirty = true;
}

void ScopePlotWidget::setManualYAxisRange(double minValue, double maxValue) {
    if (minValue >= maxValue) {
        return;
    }

    m_yRangeSettingAuto = false;
    m_autoYRange = false;
    m_manualYMin = minValue;
    m_manualYMax = maxValue;
    m_yViewMin = minValue;
    m_yViewMax = maxValue;
}

void ScopePlotWidget::setChannelData(const QVector<PlotChannelData> &channels) {
    for (auto &channel : m_channels) {
        channel.enabled = false;
    }

    for (const PlotChannelData &channel : channels) {
        if (channel.index < 0 || channel.index >= m_channels.size()) {
            continue;
        }

        ChannelData &target = m_channels[channel.index];
        target.name = channel.name;
        target.color = channel.color;
        target.enabled = channel.enabled;
        target.lineWidth = channel.lineWidth;
        target.lineStyle = channel.lineStyle;
        target.showDataPoints = channel.showDataPoints;
    }
    markAutoYDirty();
}

void ScopePlotWidget::configureAcquisition(int intervalUs, int windowMs) {
    if (intervalUs <= 0 || windowMs <= 0) {
        return;
    }

    m_sampleIntervalUs = intervalUs;
    m_displayWindowMs = windowMs;
    clearData();
}

void ScopePlotWidget::appendSamples(uint8_t channelMask, const QVector<int16_t> &samples) {
    const int rawSamples = rawWindowSampleCount();
    const int samplesPerBucket = bucketWidth();
    int sampleIndex = 0;
    for (int channelIndex = 0; channelIndex < m_channels.size(); ++channelIndex) {
        if ((channelMask & (1u << channelIndex)) == 0) {
            continue;
        }
        if (sampleIndex >= samples.size()) {
            break;
        }

        ChannelData &channel = m_channels[channelIndex];
        if (channel.rawRing.size() != rawSamples
            || static_cast<int>(channel.bucketBuffer.size()) != samplesPerBucket) {
            resetChannelBuffers(channel);
        }

        const double sampleValue = static_cast<double>(samples.at(sampleIndex++));
        channel.rawRing[channel.rawHead] = sampleValue;
        channel.rawHead = (channel.rawHead + 1) % rawSamples;
        channel.rawCount = qMin(channel.rawCount + 1, rawSamples);

        // Index-only write into the fixed-capacity bucket. No allocation.
        channel.bucketBuffer[channel.bucketFill++] = sampleValue;
        if (channel.bucketFill >= samplesPerBucket) {
            const double median = medianValueInPlace(channel.bucketBuffer, channel.bucketFill);
            writeDisplaySample(channel, median);
            channel.bucketFill = 0;
        }
    }
    markAutoYDirty();
    // No update() here -- the 16ms render timer is the single source of
    // repaints (REF/HANDOFF.md principle 1).
}

void ScopePlotWidget::clearData() {
    for (auto &channel : m_channels) {
        resetChannelBuffers(channel);
    }
    markAutoYDirty();
}

void ScopePlotWidget::resetView() {
    m_viewStartRatio = 0.0;
    m_viewEndRatio = 1.0;
    if (!m_autoYRange) {
        m_yViewMin = m_manualYMin;
        m_yViewMax = m_manualYMax;
    }
    if (m_autoYRange) {
        m_autoYDirty = true;
    }
}

void ScopePlotWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // --- frame time tracking (exponential moving average, alpha = 0.1) ---
    if (!m_frameClock.isValid()) {
        m_frameClock.start();
    } else {
        const double ms = m_frameClock.nsecsElapsed() * 1e-6;
        m_frameClock.restart();
        if (m_frameSamples == 0) {
            m_avgFrameMs = ms;
        } else {
            m_avgFrameMs = 0.9 * m_avgFrameMs + 0.1 * ms;
        }
        ++m_frameSamples;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    const QRect frameRect = rect().adjusted(8, 8, -8, -8);
    const QRect plotRect = frameRect.adjusted(58, 14, -14, -32);

    // GL backend redraws every frame; static frame is cheap on GPU.
    paintStaticFrame(&painter, frameRect, plotRect);

    // --- snapshot stage: flatten every enabled channel into m_paintSnapshot
    // exactly once. AutoY and the waveform pass both read from this buffer.
    for (int c = 0; c < m_channels.size(); ++c) {
        if (m_channels[c].enabled) {
            buildPaintSnapshot(c);
        }
    }

    const int startIndex = visibleSampleStart();
    const int endIndex = qMax(startIndex, visibleSampleEnd());

    // --- AutoY: throttled, single linear scan over the snapshot ---
    if (m_autoYRange && m_autoYDirty) {
        const bool throttled = m_autoYClock.isValid()
                               && m_autoYClock.elapsed() < kAutoYMinIntervalMs;
        if (!throttled) {
            computeAutoYFromSnapshot(startIndex, endIndex,
                                     m_cachedAutoYMin, m_cachedAutoYMax);
            m_autoYDirty = false;
            m_autoYClock.restart();
        }
    }

    double yMin = m_yViewMin;
    double yMax = m_yViewMax;
    if (m_autoYRange) {
        yMin = m_cachedAutoYMin;
        yMax = m_cachedAutoYMax;
    }

    if (m_viewMode == ScopeToolBar::ViewMode::Overlay) {
        painter.save();
        painter.setClipRect(plotRect);
        paintOverlay(&painter, plotRect, yMin, yMax);
        painter.restore();
        paintLegend(&painter, plotRect);
    } else {
        painter.save();
        painter.setClipRect(plotRect);
        paintStacked(&painter, plotRect, yMin, yMax);
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
    paintFrameTimeReadout(&painter, frameRect);
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
        if (m_autoYRange) {
            m_autoYDirty = true;
        }
    } else if (m_dragMode == DragZoomMode::Vertical && dragHeight >= 18) {
        const double localTop = static_cast<double>(top - plotRect.top()) / static_cast<double>(plotRect.height());
        const double localBottom = static_cast<double>(bottom - plotRect.top()) / static_cast<double>(plotRect.height());
        double currentMin = m_yViewMin;
        double currentMax = m_yViewMax;
        if (m_autoYRange) {
            currentMin = m_cachedAutoYMin;
            currentMax = m_cachedAutoYMax;
        }
        const double currentSpan = currentMax - currentMin;

        const double nextMax = currentMax - currentSpan * localTop;
        const double nextMin = currentMax - currentSpan * localBottom;

        m_autoYRange = false;
        m_yViewMin = nextMin;
        m_yViewMax = qMax(nextMax, nextMin + 0.05);
    }

    m_dragSelecting = false;
    m_dragMode = DragZoomMode::None;
    event->accept();
}

void ScopePlotWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && currentPlotRect().contains(event->position().toPoint())) {
        m_dragSelecting = false;
        m_dragMode = DragZoomMode::None;
        m_viewStartRatio = 0.0;
        m_viewEndRatio = 1.0;
        if (m_yRangeSettingAuto) {
            m_autoYRange = true;
            m_yViewMin = m_cachedAutoYMin;
            m_yViewMax = m_cachedAutoYMax;
            m_autoYDirty = true;
        } else {
            m_autoYRange = false;
            m_yViewMin = m_manualYMin;
            m_yViewMax = m_manualYMax;
        }
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

void ScopePlotWidget::paintOverlay(QPainter *painter, const QRect &rect, double yMin, double yMax) {
    const int startIndex = visibleSampleStart();
    const int endIndex = qMax(startIndex, visibleSampleEnd());
    for (int c = 0; c < m_channels.size(); ++c) {
        if (!m_channels[c].enabled) {
            continue;
        }

        const qreal requestedWidth = qMax<qreal>(1.0, m_channels[c].lineWidth);
        const qreal baseWidth = qMin<qreal>(requestedWidth, 4.0);
        QPen pen;
        configureScopePen(pen, m_channels[c].color, baseWidth, m_channels[c].lineStyle);
        painter->setPen(pen);
        const int drawnPoints = drawWaveformLane(painter, rect, c, yMin, yMax, startIndex, endIndex);

        const int shellCount = extraThicknessShells(requestedWidth, baseWidth);
        if (shellCount >= 1) {
            for (const QPoint &offset : kShellOneOffsets) {
                painter->save();
                painter->translate(offset);
                painter->setPen(pen);
                drawWaveformLane(painter, rect, c, yMin, yMax, startIndex, endIndex);
                painter->restore();
            }
        }
        if (shellCount >= 2) {
            for (const QPoint &offset : kShellTwoOffsets) {
                painter->save();
                painter->translate(offset);
                painter->setPen(pen);
                drawWaveformLane(painter, rect, c, yMin, yMax, startIndex, endIndex);
                painter->restore();
            }
        }
        if (m_channels[c].showDataPoints) {
            drawDataPointDots(painter, m_channels[c].color, drawnPoints);
        }
    }
}

void ScopePlotWidget::paintStacked(QPainter *painter, const QRect &rect, double yMin, double yMax) {
    int channelCount = 0;
    for (const ChannelData &channel : m_channels) {
        if (channel.enabled) {
            ++channelCount;
        }
    }
    if (channelCount == 0) {
        return;
    }

    const int gap = 10;
    const int laneHeight = (rect.height() - gap * (channelCount - 1)) / channelCount;
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 9));

    const int startIndex = visibleSampleStart();
    const int endIndex = qMax(startIndex, visibleSampleEnd());

    int laneIndex = 0;
    for (int c = 0; c < m_channels.size(); ++c) {
        const ChannelData &channel = m_channels[c];
        if (!channel.enabled) {
            continue;
        }

        const QRect laneRect(rect.left(),
                             rect.top() + laneIndex * (laneHeight + gap),
                             rect.width(),
                             laneHeight);

        painter->setPen(QPen(Style::Color::ScopeStackedLaneBorder, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(laneRect, 4, 4);

        const qreal requestedWidth = qMax<qreal>(1.0, channel.lineWidth);
        const qreal baseWidth = qMin<qreal>(requestedWidth, 4.0);
        QPen pen;
        configureScopePen(pen, channel.color, baseWidth, channel.lineStyle);
        painter->setPen(pen);
        const QRect waveformRect = laneRect.adjusted(0, 4, 0, -4);
        const int drawnPoints = drawWaveformLane(painter, waveformRect, c,
                                                 yMin, yMax, startIndex, endIndex);
        const int shellCount = extraThicknessShells(requestedWidth, baseWidth);
        if (shellCount >= 1) {
            for (const QPoint &offset : kShellOneOffsets) {
                painter->save();
                painter->translate(offset);
                painter->setPen(pen);
                drawWaveformLane(painter, waveformRect, c, yMin, yMax, startIndex, endIndex);
                painter->restore();
            }
        }
        if (shellCount >= 2) {
            for (const QPoint &offset : kShellTwoOffsets) {
                painter->save();
                painter->translate(offset);
                painter->setPen(pen);
                drawWaveformLane(painter, waveformRect, c, yMin, yMax, startIndex, endIndex);
                painter->restore();
            }
        }
        if (channel.showDataPoints) {
            drawDataPointDots(painter, channel.color, drawnPoints);
        }

        painter->setPen(Style::Color::ScopeTextHeader);
        painter->drawText(laneRect.adjusted(8, 6, -8, -6),
                          Qt::AlignTop | Qt::AlignLeft,
                          channel.name);
        ++laneIndex;
    }
}

int ScopePlotWidget::drawWaveformLane(QPainter *painter, const QRect &laneRect,
                                      int channelIndex, double yMin, double yMax,
                                      int startIndex, int endIndex) {
    const int visibleWindowCount = endIndex - startIndex + 1;
    const int liveCount = m_paintSnapshotCount[channelIndex];
    const int liveOffset = m_paintSnapshotOffset[channelIndex];
    if (visibleWindowCount <= 1 || liveCount <= 0 || laneRect.width() <= 0 || laneRect.height() <= 0) {
        return 0;
    }

    const int actualStart = qMax(startIndex, liveOffset);
    const int actualEnd = qMin(endIndex, liveOffset + liveCount - 1);
    if (actualEnd < actualStart) {
        return 0;
    }

    const double span = qMax(yMax - yMin, 1e-9);
    const double oneOverSpan = 1.0 / span;
    const double xStep = static_cast<double>(laneRect.width()) /
                         static_cast<double>(visibleWindowCount - 1);
    const double xLeft = static_cast<double>(laneRect.left());
    const double yBottom = static_cast<double>(laneRect.bottom());
    const double yHeight = static_cast<double>(laneRect.height());
    const int pointCount = actualEnd - actualStart + 1;

    if (static_cast<int>(m_pointBuffer.size()) < pointCount) {
        m_pointBuffer.resize(pointCount);
    }

    const float *snapshot = m_paintSnapshot[channelIndex].data();
    for (int i = 0; i < pointCount; ++i) {
        const int sampleIndex = actualStart + i;
        const int windowOffset = sampleIndex - startIndex;
        const float v = snapshot[sampleIndex];
        double normalized = (static_cast<double>(v) - yMin) * oneOverSpan;
        if (normalized < 0.0) {
            normalized = 0.0;
        } else if (normalized > 1.0) {
            normalized = 1.0;
        }
        m_pointBuffer[i] = QPointF(xLeft + windowOffset * xStep,
                                   yBottom - normalized * yHeight);
    }

    painter->drawPolyline(m_pointBuffer.data(), pointCount);
    return pointCount;
}

void ScopePlotWidget::drawDataPointDots(QPainter *painter, const QColor &color, int pointCount) {
    constexpr qreal kDotRadius = 3.0;
    constexpr qreal kMinDotSpacingPx = 8.0;
    constexpr qreal kMinSpacingSq = kMinDotSpacingPx * kMinDotSpacingPx;

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    qreal lastDrawnX = -1e9;
    qreal lastDrawnY = -1e9;

    for (int i = 0; i < pointCount; ++i) {
        const qreal px = m_pointBuffer[i].x();
        const qreal py = m_pointBuffer[i].y();
        const qreal dx = px - lastDrawnX;
        const qreal dy = py - lastDrawnY;
        if (dx * dx + dy * dy < kMinSpacingSq) {
            continue;
        }

        painter->drawEllipse(m_pointBuffer[i], kDotRadius, kDotRadius);
        lastDrawnX = px;
        lastDrawnY = py;
    }
}

void ScopePlotWidget::buildPaintSnapshot(int channelIndex) {
    const ChannelData &channel = m_channels[channelIndex];
    const int activeDisplayCount = displaySampleCount();
    const int liveCount = qMin(channel.uiCount, activeDisplayCount);
    const int liveOffset = qMax(0, activeDisplayCount - liveCount);
    m_paintSnapshotCount[channelIndex] = liveCount;
    m_paintSnapshotOffset[channelIndex] = liveOffset;

    float *out = m_paintSnapshot[channelIndex].data();
    std::fill(out, out + kUiRingSize, 0.0f);
    if (liveCount <= 0) {
        return;
    }

    const int oldestIndex = (channel.uiHead - liveCount + kUiRingSize) % kUiRingSize;
    const int firstSegment = qMin(liveCount, kUiRingSize - oldestIndex);
    const int secondSegment = liveCount - firstSegment;

    float *dst = out + liveOffset;
    std::copy(channel.uiRing.begin() + oldestIndex,
              channel.uiRing.begin() + oldestIndex + firstSegment,
              dst);
    if (secondSegment > 0) {
        std::copy(channel.uiRing.begin(),
                  channel.uiRing.begin() + secondSegment,
                  dst + firstSegment);
    }
}

void ScopePlotWidget::computeAutoYFromSnapshot(int startIndex, int endIndex,
                                               double &minValue, double &maxValue) const {
    bool hasValue = false;
    float dataMin = 0.0f;
    float dataMax = 0.0f;

    for (int c = 0; c < m_channels.size(); ++c) {
        if (!m_channels[c].enabled) {
            continue;
        }
        const int liveCount = m_paintSnapshotCount[c];
        const int liveOffset = m_paintSnapshotOffset[c];
        if (liveCount <= 0) {
            continue;
        }
        const int first = qMax(startIndex, liveOffset);
        const int last = qMin(endIndex, liveOffset + liveCount - 1);
        if (last < first) {
            continue;
        }
        const float *snapshot = m_paintSnapshot[c].data();
        for (int i = first; i <= last; ++i) {
            const float v = snapshot[i];
            if (!hasValue) {
                dataMin = v;
                dataMax = v;
                hasValue = true;
            } else {
                if (v < dataMin) dataMin = v;
                if (v > dataMax) dataMax = v;
            }
        }
    }

    if (!hasValue) {
        minValue = -1.0;
        maxValue = 1.0;
        return;
    }

    constexpr double kStep = 100.0;
    const double rawMin = static_cast<double>(dataMin);
    const double rawMax = static_cast<double>(dataMax);
    const double span = qMax(kStep, rawMax - rawMin);
    const double padding = qMax(kStep * 0.5, span * 0.1);
    minValue = std::floor((rawMin - padding) / kStep) * kStep;
    maxValue = std::ceil((rawMax + padding) / kStep) * kStep;
    if (maxValue <= minValue) {
        maxValue = minValue + kStep;
    }
}

void ScopePlotWidget::paintTimeAxis(QPainter *painter, const QRect &rect) {
    const QRect axisRect(rect.left() + 62, rect.bottom() - 24, rect.width() - 76, 20);
    painter->setPen(Style::Color::ScopeTextSubtle);
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));

    for (int i = 0; i <= 8; ++i) {
        const int x = axisRect.left() + (axisRect.width() * i) / 8;
        painter->drawLine(x, axisRect.top(), x, axisRect.top() + 5);
        const double timeValue = (m_viewStartRatio + (m_viewEndRatio - m_viewStartRatio)
                                  * (static_cast<double>(i) / 8.0)) * static_cast<double>(m_displayWindowMs);
        const QString label = QStringLiteral("%1 ms").arg(QString::number(timeValue, 'f', timeValue < 10.0 ? 1 : 0));
        painter->drawText(x - 28, axisRect.top() + 8, 56, 12, Qt::AlignHCenter | Qt::AlignTop, label);
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
        painter->drawText(labelRight - 48, y - 7, 46, 14, Qt::AlignRight | Qt::AlignVCenter,
                          QString::number(values.at(i), 'f', 0));
    }
}

void ScopePlotWidget::paintLegend(QPainter *painter, const QRect &rect) {
    const int itemWidth = 98;
    const int itemHeight = 18;
    const int top = rect.top() + 8;
    int left = rect.left() + 10;

    painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));
    for (const ChannelData &channel : m_channels) {
        if (!channel.enabled) {
            continue;
        }

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

void ScopePlotWidget::paintFrameTimeReadout(QPainter *painter, const QRect &rect) {
    painter->setPen(Style::Color::ScopeTextSubtle);
    painter->setFont(QFont(QStringLiteral("Segoe UI"), 9));

    if (m_avgFrameMs > 0.0) {
        const double fps = 1000.0 / m_avgFrameMs;
        // Pre-rendered into a static QString avoids per-frame heap churn for
        // QString::arg in the hot path; we use a small stack buffer instead.
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.1f fps  (%.2f ms)", fps, m_avgFrameMs);
        painter->setPen(Style::Color::ScopeTextSubtle);
        painter->drawText(rect.adjusted(0, 8, -12, 0),
                          Qt::AlignTop | Qt::AlignRight,
                          QString::fromLatin1(buf));
    }

    painter->drawText(rect.adjusted(0, 22, -12, 0),
                      Qt::AlignTop | Qt::AlignRight,
                      m_running ? tr("Live scope preview") : tr("Scope idle preview"));
}

void ScopePlotWidget::paintStaticFrame(QPainter *painter, const QRect &frameRect, const QRect &plotRect) {
    painter->fillRect(rect(), Style::Color::ScopeBackground);
    painter->setPen(QPen(Style::Color::ScopeFrameBorder, 1));
    painter->setBrush(Style::Color::ScopeFrameBackground);
    painter->drawRoundedRect(frameRect, 6, 6);
    paintGrid(painter, plotRect);
}

void ScopePlotWidget::markAutoYDirty() {
    if (m_autoYRange) {
        m_autoYDirty = true;
    }
}

QRect ScopePlotWidget::currentPlotRect() const {
    const QRect frameRect = rect().adjusted(8, 8, -8, -8);
    return frameRect.adjusted(58, 14, -14, -32);
}

int ScopePlotWidget::rawWindowSampleCount() const {
    return qMax(
        1,
        qRound((static_cast<double>(m_displayWindowMs) * 1000.0) / static_cast<double>(m_sampleIntervalUs)));
}

int ScopePlotWidget::bucketWidth() const {
    const int rawSamples = rawWindowSampleCount();
    return qMax(1, (rawSamples + kUiRingSize - 1) / kUiRingSize);
}

int ScopePlotWidget::displaySampleCount() const {
    const int rawSamples = rawWindowSampleCount();
    const int bucketSamples = bucketWidth();
    return qMax(1, (rawSamples + bucketSamples - 1) / bucketSamples);
}

int ScopePlotWidget::visibleSampleStart() const {
    const int windowLastIndex = qMax(0, displaySampleCount() - 1);
    const int offset = static_cast<int>(std::floor(m_viewStartRatio * windowLastIndex));
    return qBound(0, offset, windowLastIndex);
}

int ScopePlotWidget::visibleSampleEnd() const {
    const int windowLastIndex = qMax(0, displaySampleCount() - 1);
    const int offset = static_cast<int>(std::ceil(m_viewEndRatio * windowLastIndex));
    return qBound(0, offset, windowLastIndex);
}

void ScopePlotWidget::resetChannelBuffers(ChannelData &channel) {
    channel.rawRing.fill(0.0, rawWindowSampleCount());
    channel.rawHead = 0;
    channel.rawCount = 0;
    channel.uiRing.fill(0.0f);
    channel.uiHead = 0;
    channel.uiCount = 0;
    channel.bucketBuffer.assign(static_cast<size_t>(bucketWidth()), 0.0);
    channel.bucketFill = 0;
}

void ScopePlotWidget::writeDisplaySample(ChannelData &channel, double value) {
    channel.uiRing[channel.uiHead] = static_cast<float>(value);
    channel.uiHead = (channel.uiHead + 1) % kUiRingSize;
    channel.uiCount = qMin(channel.uiCount + 1, kUiRingSize);
}
