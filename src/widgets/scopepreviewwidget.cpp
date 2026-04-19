#include "widgets/scopepreviewwidget.h"

#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scopestylepanel.h"
#include "widgets/topbar.h"

#include <QHBoxLayout>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

using namespace MotorDev;

ScopePreviewWidget::ScopePreviewWidget(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    m_channels.resize(8);
    m_channels[0] = {true,  QStringLiteral("Speed"),   QStringLiteral("0x0010"), Style::Color::ScopeWaveCh1, 4.0, Qt::SolidLine, false};
    m_channels[1] = {true,  QStringLiteral("Torque"),  QStringLiteral("0x0012"), Style::Color::ScopeWaveCh2, 4.0, Qt::SolidLine, false};
    m_channels[2] = {true,  QStringLiteral("Temp"),    QStringLiteral("0x0014"), Style::Color::ScopeWaveCh3, 4.0, Qt::SolidLine, false};
    m_channels[3] = {true,  QStringLiteral("Current"), QStringLiteral("0x0016"), Style::Color::ScopeWaveCh4, 4.0, Qt::SolidLine, false};
    m_channels[4] = {false, QString(),                  QStringLiteral("0x0018"), Style::Color::ScopeWaveCh5, 4.0, Qt::SolidLine, false};
    m_channels[5] = {false, QString(),                  QStringLiteral("0x001A"), Style::Color::ScopeWaveCh6, 4.0, Qt::SolidLine, false};
    m_channels[6] = {false, QString(),                  QStringLiteral("0x001C"), Style::Color::ScopeWaveCh7, 4.0, Qt::SolidLine, false};
    m_channels[7] = {false, QString(),                  QStringLiteral("0x001E"), Style::Color::ScopeWaveCh8, 4.0, Qt::SolidLine, false};
    m_defaultChannels = m_channels;

    auto *bottomLayout = qobject_cast<QVBoxLayout *>(m_bottomPanelHost->layout());
    m_bottomPanel = new ScopeBottomPanel(m_plotArea, m_bottomPanelHost);
    bottomLayout->addWidget(m_bottomPanel);
    if (auto *plotAreaLayout = qobject_cast<QVBoxLayout *>(m_plotArea->layout())) {
        plotAreaLayout->setStretch(0, 1);
        plotAreaLayout->setStretch(1, 0);
    }

    m_stylePanel->setVisible(false);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);

    for (int i = 0; i < m_channels.size(); ++i) {
        m_stylePanel->setChannelColor(i, m_channels[i].color);
        m_stylePanel->setChannelLineWidth(i, static_cast<int>(m_channels[i].lineWidth));
        m_stylePanel->setChannelLineStyle(i, m_channels[i].lineStyle);
        m_stylePanel->setChannelShowDataPoints(i, m_channels[i].showDataPoints);
    }

    m_plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
    refreshPlotData();
    connectSignals();
    m_topBar->setScopeControlsVisible(true);
    m_topBar->setViewMode(0);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(33);
    connect(m_previewTimer, &QTimer::timeout, this, &ScopePreviewWidget::appendPreviewFrame);
}

ScopePreviewWidget::~ScopePreviewWidget() = default;

void ScopePreviewWidget::connectSignals() {
    connect(m_plotWidget, &ScopePlotWidget::samplingToggleRequested, this, &ScopePreviewWidget::setRunning);
    connect(m_topBar, &TopBar::viewModeChanged, this, [this](int mode) {
        m_plotWidget->setViewMode(mode == 1 ? ScopeViewMode::Stacked : ScopeViewMode::Overlay);
    });
    connect(m_topBar, &TopBar::styleToggleRequested, this, [this]() {
        m_stylePanel->setVisible(!m_stylePanel->isVisible());
    });

    connect(m_bottomPanel, &ScopeBottomPanel::channelToggled, this, [this](int index, bool enabled) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].enabled = enabled;
        refreshPlotData();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelDescriptionChanged, this, [this](int index, const QString &text) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].description = text.trimmed();
        refreshPlotData();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelAddressChanged, this, [this](int index, const QString &text) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].addressText = text.trimmed();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::sampleIntervalChanged, this, [this](const QString &text) {
        m_sampleIntervalUs = sampleIntervalUsForText(text);
        m_plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
        refreshPlotData();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::displayWindowChanged, this, [this](const QString &text) {
        m_displayWindowMs = displayWindowMsForText(text);
        m_plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
        refreshPlotData();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisAutoRequested, m_plotWidget, &ScopePlotWidget::setAutoYAxisRange);
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisManualRequested, m_plotWidget, &ScopePlotWidget::setManualYAxisRange);

    connect(m_stylePanel, &ScopeStylePanel::colorChanged, this, [this](int index, const QColor &color) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].color = color;
        refreshPlotData();
    });
    connect(m_stylePanel, &ScopeStylePanel::lineWidthChanged, this, [this](int index, int width) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].lineWidth = static_cast<qreal>(width);
        refreshPlotData();
    });
    connect(m_stylePanel, &ScopeStylePanel::lineStyleChanged, this, [this](int index, Qt::PenStyle style) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].lineStyle = style;
        refreshPlotData();
    });
    connect(m_stylePanel, &ScopeStylePanel::dataPointsChanged, this, [this](int index, bool show) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].showDataPoints = show;
        refreshPlotData();
    });
    connect(m_stylePanel, &ScopeStylePanel::defaultSettingsRequested, this, &ScopePreviewWidget::resetStyleDefaults);
}

void ScopePreviewWidget::refreshPlotData() {
    QVector<ScopePlotWidget::PlotChannelData> plotChannels;
    plotChannels.reserve(m_channels.size());
    for (int index = 0; index < m_channels.size(); ++index) {
        const ChannelState &channel = m_channels.at(index);
        if (!channel.enabled) {
            continue;
        }
        ScopePlotWidget::PlotChannelData plotChannel;
        plotChannel.index = index;
        plotChannel.name = channel.description.isEmpty()
                               ? QStringLiteral("CH%1").arg(index + 1)
                               : QStringLiteral("CH%1 %2").arg(index + 1).arg(channel.description);
        plotChannel.color = channel.color;
        plotChannel.enabled = channel.enabled;
        plotChannel.lineWidth = channel.lineWidth;
        plotChannel.lineStyle = channel.lineStyle;
        plotChannel.showDataPoints = channel.showDataPoints;
        plotChannels.append(plotChannel);
    }
    m_plotWidget->setChannelData(plotChannels);
}

void ScopePreviewWidget::setRunning(bool running) {
    if (m_running == running) {
        if (m_bottomPanel != nullptr) {
            m_bottomPanel->setRunning(running);
        }
        m_plotWidget->setRunning(running);
        return;
    }

    m_running = running;
    if (m_bottomPanel != nullptr) {
        m_bottomPanel->setRunning(running);
    }
    m_plotWidget->setRunning(running);
    if (running) {
        m_plotWidget->clearData();
        m_plotWidget->resetView();
        m_phase = 0;
        m_previewTimer->start();
    } else {
        m_previewTimer->stop();
    }
}

void ScopePreviewWidget::appendPreviewFrame() {
    const uint8_t mask = currentChannelMask();
    if (mask == 0) {
        return;
    }

    QVector<int16_t> samples;
    samples.reserve(m_channels.size());
    for (int index = 0; index < m_channels.size(); ++index) {
        if ((mask & (1u << index)) == 0) {
            continue;
        }
        const double t = (m_phase * 0.08) + (index * 0.55);
        const double carrier = qSin(t);
        const double harmonic = qSin(t * (1.6 + index * 0.08));
        const double value = (carrier * (300.0 + index * 55.0)) + (harmonic * 90.0);
        samples.append(static_cast<int16_t>(qRound(value)));
    }
    m_plotWidget->appendSamples(mask, samples);
    m_phase += 1;
}

void ScopePreviewWidget::resetStyleDefaults() {
    for (int index = 0; index < m_channels.size(); ++index) {
        m_channels[index].color = m_defaultChannels[index].color;
        m_channels[index].lineWidth = m_defaultChannels[index].lineWidth;
        m_channels[index].lineStyle = m_defaultChannels[index].lineStyle;
        m_channels[index].showDataPoints = m_defaultChannels[index].showDataPoints;
        m_stylePanel->setChannelColor(index, m_channels[index].color);
        m_stylePanel->setChannelLineWidth(index, static_cast<int>(m_channels[index].lineWidth));
        m_stylePanel->setChannelLineStyle(index, m_channels[index].lineStyle);
        m_stylePanel->setChannelShowDataPoints(index, m_channels[index].showDataPoints);
    }
    refreshPlotData();
}

void ScopePreviewWidget::setupUi() {
    setObjectName(QStringLiteral("ScopePreviewWidget"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_topBar = new TopBar(this);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    rootLayout->addWidget(m_topBar);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("splitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(0);
    rootLayout->addWidget(m_splitter);
    rootLayout->setStretch(0, 0);
    rootLayout->setStretch(1, 1);

    m_plotArea = new QWidget(m_splitter);
    m_plotArea->setObjectName(QStringLiteral("plotArea"));
    auto *plotAreaLayout = new QVBoxLayout(m_plotArea);
    plotAreaLayout->setObjectName(QStringLiteral("plotAreaLayout"));
    plotAreaLayout->setSpacing(0);
    plotAreaLayout->setContentsMargins(0, 0, 0, 0);

    m_plotWidget = new ScopePlotWidget(m_plotArea);
    m_plotWidget->setObjectName(QStringLiteral("plotWidget"));
    plotAreaLayout->addWidget(m_plotWidget);

    m_bottomPanelHost = new QWidget(m_plotArea);
    m_bottomPanelHost->setObjectName(QStringLiteral("bottomPanelHost"));
    auto *bottomPanelHostLayout = new QVBoxLayout(m_bottomPanelHost);
    bottomPanelHostLayout->setObjectName(QStringLiteral("bottomPanelHostLayout"));
    bottomPanelHostLayout->setSpacing(0);
    bottomPanelHostLayout->setContentsMargins(0, 0, 0, 0);
    plotAreaLayout->addWidget(m_bottomPanelHost);

    m_stylePanel = new ScopeStylePanel(m_splitter);
    m_stylePanel->setObjectName(QStringLiteral("stylePanel"));
}

int ScopePreviewWidget::sampleIntervalUsForText(const QString &text) const {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("100 us")) return 100;
    if (normalized == QStringLiteral("200 us")) return 200;
    if (normalized == QStringLiteral("300 us")) return 300;
    if (normalized == QStringLiteral("500 us")) return 500;
    if (normalized == QStringLiteral("750 us")) return 750;
    if (normalized == QStringLiteral("1500 us")) return 1500;
    if (normalized == QStringLiteral("2000 us")) return 2000;
    return 1000;
}

int ScopePreviewWidget::displayWindowMsForText(const QString &text) const {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("50 ms")) return 50;
    if (normalized == QStringLiteral("200 ms")) return 200;
    if (normalized == QStringLiteral("500 ms")) return 500;
    if (normalized == QStringLiteral("1000 ms")) return 1000;
    if (normalized == QStringLiteral("2000 ms")) return 2000;
    if (normalized == QStringLiteral("4000 ms")) return 4000;
    return 50;
}

uint8_t ScopePreviewWidget::currentChannelMask() const {
    uint8_t mask = 0;
    for (int index = 0; index < m_channels.size(); ++index) {
        if (m_channels.at(index).enabled) {
            mask |= static_cast<uint8_t>(1u << index);
        }
    }
    return mask;
}
