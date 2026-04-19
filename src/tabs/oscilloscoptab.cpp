#include "tabs/oscilloscoptab.h"

#include "models/scopechannelmodel.h"
#include "services/scopeservice.h"
#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scopestylepanel.h"

#include <QDebug>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

OscilloscopTab::OscilloscopTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent) {
    m_channelModel = new ScopeChannelModel(this);
    m_service = new ScopeService(serialManager, m_channelModel, this);
    setupUi();
    connectSignals();
    refreshPlotData();

    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(1000);
    connect(m_perfTimer, &QTimer::timeout, this, &OscilloscopTab::onPerfTimerTick);
    m_perfTimer->start();
}

OscilloscopTab::~OscilloscopTab() {
    if (m_fullscreenWindow != nullptr) {
        if (m_fullscreenWindow->layout() != nullptr) {
            m_fullscreenWindow->layout()->removeWidget(m_plotWidget);
        }
        m_plotLayout->insertWidget(m_plotLayoutIndex >= 0 ? m_plotLayoutIndex : 0, m_plotWidget);
        m_plotLayout->setStretch(0, 1);
        m_plotLayout->setStretch(1, 0);
        delete m_fullscreenWindow;
        m_fullscreenWindow = nullptr;
    }
}

void OscilloscopTab::setupUi() {
    setObjectName(QStringLiteral("OscilloscopTab"));
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setStretch(0, 1);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("splitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(0);
    rootLayout->addWidget(m_splitter);

    m_plotArea = new QWidget(m_splitter);
    m_plotArea->setObjectName(QStringLiteral("plotArea"));
    m_plotLayout = new QVBoxLayout(m_plotArea);
    m_plotLayout->setObjectName(QStringLiteral("plotLayout"));
    m_plotLayout->setSpacing(0);
    m_plotLayout->setContentsMargins(0, 0, 0, 0);

    m_plotWidget = new ScopePlotWidget(m_plotArea);
    m_plotWidget->setObjectName(QStringLiteral("plotWidget"));
    m_plotLayout->addWidget(m_plotWidget);

    m_bottomPanelHost = new QWidget(m_plotArea);
    m_bottomPanelHost->setObjectName(QStringLiteral("bottomPanelHost"));
    m_plotLayout->addWidget(m_bottomPanelHost);

    m_stylePanel = new ScopeStylePanel(m_splitter);
    m_stylePanel->setObjectName(QStringLiteral("stylePanel"));

    auto *bottomLayout = qobject_cast<QVBoxLayout *>(m_bottomPanelHost->layout());
    if (bottomLayout == nullptr) {
        bottomLayout = new QVBoxLayout(m_bottomPanelHost);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(0);
    }

    m_bottomPanel = new ScopeBottomPanel(m_plotArea, m_bottomPanelHost);
    bottomLayout->addWidget(m_bottomPanel);
    m_plotLayout->setStretch(0, 1);
    m_plotLayout->setStretch(1, 0);
    m_stylePanel->setVisible(false);

    for (int i = 0; i < m_channelModel->channelCount(); ++i) {
        const ScopeChannelState &ch = m_channelModel->channel(i);
        m_stylePanel->setChannelColor(i, ch.color);
        m_stylePanel->setChannelLineWidth(i, static_cast<int>(ch.lineWidth));
        m_stylePanel->setChannelLineStyle(i, ch.lineStyle);
        m_stylePanel->setChannelShowDataPoints(i, ch.showDataPoints);
    }

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
}

void OscilloscopTab::connectSignals() {
    connect(m_plotWidget, &ScopePlotWidget::fullscreenToggleRequested, this, &OscilloscopTab::togglePlotFullscreen);
    connect(m_plotWidget, &ScopePlotWidget::samplingToggleRequested, this, &OscilloscopTab::onSamplingToggleRequested);

    // Bottom panel → channel model
    connect(m_bottomPanel, &ScopeBottomPanel::channelToggled, m_channelModel, &ScopeChannelModel::setEnabled);
    connect(m_bottomPanel, &ScopeBottomPanel::channelDescriptionChanged, m_channelModel, &ScopeChannelModel::setDescription);
    connect(m_bottomPanel, &ScopeBottomPanel::channelAddressChanged, m_channelModel, &ScopeChannelModel::setAddressText);

    // Bottom panel → local UI state
    connect(m_bottomPanel, &ScopeBottomPanel::sampleIntervalChanged, this, [this](const QString &text) {
        m_sampleIntervalIndex = ScopeService::sampleIntervalIndexForText(text);
        qDebug().noquote() << QStringLiteral("[Scope GUI] Sample interval=%1 (0x%2)")
                                  .arg(text)
                                  .arg(m_sampleIntervalIndex, 2, 16, QLatin1Char('0'));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::displayWindowChanged, this, [this](const QString &text) {
        m_displayWindowMs = ScopeService::displayWindowMsForText(text);
        qDebug().noquote() << QStringLiteral("[Scope GUI] Display window=%1").arg(text);
    });

    // Bottom panel → placeholder stubs
    connect(m_bottomPanel, &ScopeBottomPanel::registerReadRequested, this, [](int row) {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Register R row %1").arg(row + 1);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::registerWriteRequested, this, [](int row) {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Register W row %1").arg(row + 1);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::clearPanelRequested, this, []() {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Clear register panel");
    });
    connect(m_bottomPanel, &ScopeBottomPanel::loadParamsRequested, this, []() {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Load parameters");
    });
    connect(m_bottomPanel, &ScopeBottomPanel::captureNoteChanged, this, [](const QString &text) {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Capture note=%1").arg(text);
    });

    // Bottom panel → plot widget Y-axis
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisAutoRequested, this, [this]() {
        m_plotWidget->setAutoYAxisRange();
        qDebug().noquote() << QStringLiteral("[Scope GUI] Y axis=auto");
    });
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisManualRequested, this, [this](double minValue, double maxValue) {
        m_plotWidget->setManualYAxisRange(minValue, maxValue);
        qDebug().noquote() << QStringLiteral("[Scope GUI] Y axis=%1..%2")
                                  .arg(QString::number(minValue, 'f', 1))
                                  .arg(QString::number(maxValue, 'f', 1));
    });

    // Style panel → channel model
    connect(m_stylePanel, &ScopeStylePanel::colorChanged, m_channelModel, &ScopeChannelModel::setColor);
    connect(m_stylePanel, &ScopeStylePanel::lineWidthChanged, m_channelModel, &ScopeChannelModel::setLineWidth);
    connect(m_stylePanel, &ScopeStylePanel::lineStyleChanged, m_channelModel, &ScopeChannelModel::setLineStyle);
    connect(m_stylePanel, &ScopeStylePanel::dataPointsChanged, m_channelModel, &ScopeChannelModel::setShowDataPoints);
    connect(m_stylePanel, &ScopeStylePanel::defaultSettingsRequested, m_channelModel, &ScopeChannelModel::resetStylesToDefault);

    // Channel model → refresh plot + sync style panel on reset
    connect(m_channelModel, &ScopeChannelModel::channelChanged, this, &OscilloscopTab::refreshPlotData);
    connect(m_channelModel, &ScopeChannelModel::stylesReset, this, [this]() {
        for (int i = 0; i < m_channelModel->channelCount(); ++i) {
            const ScopeChannelState &ch = m_channelModel->channel(i);
            m_stylePanel->setChannelColor(i, ch.color);
            m_stylePanel->setChannelLineWidth(i, static_cast<int>(ch.lineWidth));
            m_stylePanel->setChannelLineStyle(i, ch.lineStyle);
            m_stylePanel->setChannelShowDataPoints(i, ch.showDataPoints);
        }
        qDebug().noquote() << QStringLiteral("[Scope GUI] Reset channel styles to default");
    });

    // Service → UI
    connect(m_service, &ScopeService::runningChanged, this, [this](bool running) {
        m_bottomPanel->setRunning(running);
        m_plotWidget->setRunning(running);
        emit runningChanged(running);
    });
    connect(m_service, &ScopeService::samplesReceived, m_plotWidget, &ScopePlotWidget::appendSamples);
    connect(m_service, &ScopeService::acquisitionConfigured, m_plotWidget, &ScopePlotWidget::configureAcquisition);
    connect(m_service, &ScopeService::resetViewRequested, m_plotWidget, &ScopePlotWidget::resetView);
}

void OscilloscopTab::onViewModeChangeRequested(int mode) {
    const ScopeViewMode nextMode = viewModeFromInt(mode);
    if (m_viewMode == nextMode) {
        emit viewModeChanged(viewModeToInt(m_viewMode));
        return;
    }
    m_viewMode = nextMode;
    m_plotWidget->setViewMode(m_viewMode);
    emit viewModeChanged(viewModeToInt(m_viewMode));
    qDebug().noquote() << QStringLiteral("[Scope GUI] %1")
                              .arg(m_viewMode == ScopeViewMode::Overlay
                                       ? QStringLiteral("Switch to Overlay")
                                       : QStringLiteral("Switch to Stacked"));
}

void OscilloscopTab::onSamplingToggleRequested(bool running) {
    if (running) {
        m_service->requestStart(m_sampleIntervalIndex, m_displayWindowMs);
    } else {
        m_service->requestStop();
    }
}

void OscilloscopTab::onStyleToggleRequested() {
    toggleStylePanel();
}

void OscilloscopTab::setDebugStreamActive(bool active) {
    m_service->setDebugStreamActive(active);
}

void OscilloscopTab::ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples) {
    m_service->ingestDebugStream(channelMask, samples);
}

void OscilloscopTab::refreshPlotData() {
    QVector<ScopePlotWidget::PlotChannelData> plotChannels;
    plotChannels.reserve(m_channelModel->channelCount());
    for (int index = 0; index < m_channelModel->channelCount(); ++index) {
        const ScopeChannelState &channel = m_channelModel->channel(index);
        if (!channel.enabled) continue;
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

void OscilloscopTab::toggleStylePanel() { m_stylePanel->setVisible(!m_stylePanel->isVisible()); }

void OscilloscopTab::togglePlotFullscreen() {
    if (m_fullscreenWindow == nullptr) {
        m_plotLayoutIndex = m_plotLayout->indexOf(m_plotWidget);
        m_fullscreenWindow = new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
        m_fullscreenWindow->setStyleSheet(
            QStringLiteral("background:%1;").arg(Style::Color::ScopeBackground.name()));

        auto *layout = new QVBoxLayout(m_fullscreenWindow);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_plotLayout->removeWidget(m_plotWidget);
        layout->addWidget(m_plotWidget);

        m_fullscreenWindow->showFullScreen();
        m_plotWidget->setFocus();
        return;
    }

    if (m_fullscreenWindow->layout() != nullptr) {
        m_fullscreenWindow->layout()->removeWidget(m_plotWidget);
    }
    m_plotLayout->insertWidget(m_plotLayoutIndex >= 0 ? m_plotLayoutIndex : 0, m_plotWidget);
    m_plotLayout->setStretch(0, 1);
    m_plotLayout->setStretch(1, 0);

    delete m_fullscreenWindow;
    m_fullscreenWindow = nullptr;
    m_plotWidget->setFocus();
}

void OscilloscopTab::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_fullscreenWindow != nullptr) {
        togglePlotFullscreen();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

ScopeViewMode OscilloscopTab::viewModeFromInt(int mode) { return mode == 1 ? ScopeViewMode::Stacked : ScopeViewMode::Overlay; }
int OscilloscopTab::viewModeToInt(ScopeViewMode mode) { return mode == ScopeViewMode::Stacked ? 1 : 0; }

void OscilloscopTab::onPerfTimerTick() {
    if (!m_service->isRunning()) return;
    qDebug().noquote() << QStringLiteral("[Scope Perf] paint=%1ms fps=%2 batchPerSec=%3 samplesPerSec=%4")
                              .arg(QString::number(m_plotWidget->paintAverageMs(), 'f', 2),
                                   QString::number(m_plotWidget->paintFps(), 'f', 1),
                                   QString::number(m_service->perfBatchCount()),
                                   QString::number(m_service->perfSampleCount()));
    m_service->resetPerfCounters();
}
