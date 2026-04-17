#include "widgets/scopepreviewwidget.h"

#include "ui_scopepreviewwidget.h"
#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scopestylepanel.h"
#include "widgets/topbar.h"

#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>
#include <utility>

using namespace MotorDev;

ScopePreviewWidget::ScopePreviewWidget(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ScopePreviewWidget>()) {
    ui->setupUi(this);
    ui->rootLayout->setStretch(0, 0);
    ui->rootLayout->setStretch(1, 1);
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

    auto *bottomLayout = qobject_cast<QVBoxLayout *>(ui->bottomPanelHost->layout());
    m_bottomPanel = new ScopeBottomPanel(ui->plotArea, ui->bottomPanelHost);
    bottomLayout->addWidget(m_bottomPanel);
    ui->plotAreaLayout->setStretch(0, 1);
    ui->plotAreaLayout->setStretch(1, 0);

    ui->stylePanel->setVisible(false);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 0);

    for (int i = 0; i < m_channels.size(); ++i) {
        ui->stylePanel->setChannelColor(i, m_channels[i].color);
        ui->stylePanel->setChannelLineWidth(i, static_cast<int>(m_channels[i].lineWidth));
        ui->stylePanel->setChannelLineStyle(i, m_channels[i].lineStyle);
        ui->stylePanel->setChannelShowDataPoints(i, m_channels[i].showDataPoints);
    }

    ui->plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
    refreshPlotData();
    connectSignals();
    ui->topBar->setScopeControlsVisible(true);
    ui->topBar->setRunning(false);
    ui->topBar->setViewMode(0);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(33);
    connect(m_previewTimer, &QTimer::timeout, this, &ScopePreviewWidget::appendPreviewFrame);
}

ScopePreviewWidget::~ScopePreviewWidget() = default;

void ScopePreviewWidget::connectSignals() {
    connect(ui->topBar, &TopBar::samplingToggleRequested, this, &ScopePreviewWidget::setRunning);
    connect(ui->topBar, &TopBar::viewModeChanged, this, [this](int mode) {
        ui->plotWidget->setViewMode(mode == 1 ? ScopeViewMode::Stacked : ScopeViewMode::Overlay);
    });
    connect(ui->topBar, &TopBar::styleToggleRequested, this, [this]() {
        ui->stylePanel->setVisible(!ui->stylePanel->isVisible());
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
        ui->plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
        refreshPlotData();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::displayWindowChanged, this, [this](const QString &text) {
        m_displayWindowMs = displayWindowMsForText(text);
        ui->plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
        refreshPlotData();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisAutoRequested, ui->plotWidget, &ScopePlotWidget::setAutoYAxisRange);
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisManualRequested, ui->plotWidget, &ScopePlotWidget::setManualYAxisRange);

    connect(ui->stylePanel, &ScopeStylePanel::colorChanged, this, [this](int index, const QColor &color) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].color = color;
        refreshPlotData();
    });
    connect(ui->stylePanel, &ScopeStylePanel::lineWidthChanged, this, [this](int index, int width) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].lineWidth = static_cast<qreal>(width);
        refreshPlotData();
    });
    connect(ui->stylePanel, &ScopeStylePanel::lineStyleChanged, this, [this](int index, Qt::PenStyle style) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].lineStyle = style;
        refreshPlotData();
    });
    connect(ui->stylePanel, &ScopeStylePanel::dataPointsChanged, this, [this](int index, bool show) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].showDataPoints = show;
        refreshPlotData();
    });
    connect(ui->stylePanel, &ScopeStylePanel::defaultSettingsRequested, this, &ScopePreviewWidget::resetStyleDefaults);
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
    ui->plotWidget->setChannelData(plotChannels);
}

void ScopePreviewWidget::setRunning(bool running) {
    if (m_running == running) {
        ui->topBar->setRunning(running);
        if (m_bottomPanel != nullptr) {
            m_bottomPanel->setRunning(running);
        }
        ui->plotWidget->setRunning(running);
        return;
    }

    m_running = running;
    ui->topBar->setRunning(running);
    if (m_bottomPanel != nullptr) {
        m_bottomPanel->setRunning(running);
    }
    ui->plotWidget->setRunning(running);
    if (running) {
        ui->plotWidget->clearData();
        ui->plotWidget->resetView();
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
    ui->plotWidget->appendSamples(mask, samples);
    m_phase += 1;
}

void ScopePreviewWidget::resetStyleDefaults() {
    for (int index = 0; index < m_channels.size(); ++index) {
        m_channels[index].color = m_defaultChannels[index].color;
        m_channels[index].lineWidth = m_defaultChannels[index].lineWidth;
        m_channels[index].lineStyle = m_defaultChannels[index].lineStyle;
        m_channels[index].showDataPoints = m_defaultChannels[index].showDataPoints;
        ui->stylePanel->setChannelColor(index, m_channels[index].color);
        ui->stylePanel->setChannelLineWidth(index, static_cast<int>(m_channels[index].lineWidth));
        ui->stylePanel->setChannelLineStyle(index, m_channels[index].lineStyle);
        ui->stylePanel->setChannelShowDataPoints(index, m_channels[index].showDataPoints);
    }
    refreshPlotData();
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