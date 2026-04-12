#include "tabs/oscilloscoptab.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scopestylepanel.h"
#include "widgets/scopetoolbar.h"

#include <QDebug>
#include <QMetaObject>
#include <QSplitter>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}
}

ScopeStreamBatcher::ScopeStreamBatcher(QObject *parent)
    : QObject(parent) {
}

void ScopeStreamBatcher::enqueue(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (samples.isEmpty()) {
        return;
    }

    ensureTimer();

    ScopeStreamPacket packet;
    packet.channelMask = channelMask;
    packet.samples = samples;
    m_pendingBatch.append(packet);

    if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

void ScopeStreamBatcher::clearPending() {
    m_pendingBatch.clear();
    if (m_flushTimer != nullptr) {
        m_flushTimer->stop();
    }
}

void ScopeStreamBatcher::ensureTimer() {
    if (m_flushTimer != nullptr) {
        return;
    }

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(8);
    connect(m_flushTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingBatch.isEmpty()) {
            m_flushTimer->stop();
            return;
        }

        ScopeStreamBatch batch = m_pendingBatch;
        m_pendingBatch.clear();
        emit batchReady(batch);
    });
}

OscilloscopTab::OscilloscopTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent)
    , m_serialManager(serialManager)
    , m_channels({
          {true, QStringLiteral("Speed"), QStringLiteral("0x0010"), 0x0010, Style::Color::ScopeWaveCh1},
          {true, QStringLiteral("Torque"), QStringLiteral("0x0012"), 0x0012, Style::Color::ScopeWaveCh2},
          {true, QStringLiteral("Temp"), QStringLiteral("0x0014"), 0x0014, Style::Color::ScopeWaveCh3},
          {true, QStringLiteral("Current"), QStringLiteral("0x0016"), 0x0016, Style::Color::ScopeWaveCh4},
          {false, QString(), QStringLiteral("0x0018"), 0x0018, Style::Color::ScopeWaveCh5},
          {false, QString(), QStringLiteral("0x001A"), 0x001A, Style::Color::ScopeWaveCh6},
          {false, QString(), QStringLiteral("0x001C"), 0x001C, Style::Color::ScopeWaveCh7},
          {false, QString(), QStringLiteral("0x001E"), 0x001E, Style::Color::ScopeWaveCh8},
      }) {
    qRegisterMetaType<ScopeStreamBatch>("ScopeStreamBatch");
    m_defaultChannels = m_channels;
    setupUi();
    connectSignals();
    refreshPlotData();

    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(1000);
    connect(m_perfTimer, &QTimer::timeout, this, &OscilloscopTab::onPerfTimerTick);
    m_perfTimer->start();
}

void OscilloscopTab::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_toolBar = new ScopeToolBar(this);
    layout->addWidget(m_toolBar);

    auto *plotArea = new QWidget(this);
    auto *plotLayout = new QVBoxLayout(plotArea);
    plotLayout->setContentsMargins(0, 0, 0, 0);
    plotLayout->setSpacing(0);

    m_plotWidget = new ScopePlotWidget(plotArea);
    m_bottomPanel = new ScopeBottomPanel(plotArea, this);

    plotLayout->addWidget(m_plotWidget, 1);
    plotLayout->addWidget(m_bottomPanel);

    m_stylePanel = new ScopeStylePanel(this);
    m_stylePanel->setVisible(false);

    for (int i = 0; i < m_channels.size(); ++i) {
        m_stylePanel->setChannelColor(i, m_channels[i].color);
        m_stylePanel->setChannelLineWidth(i, static_cast<int>(m_channels[i].lineWidth));
        m_stylePanel->setChannelLineStyle(i, m_channels[i].lineStyle);
    }

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(plotArea);
    m_splitter->addWidget(m_stylePanel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setHandleWidth(0);

    layout->addWidget(m_splitter, 1);

    setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::ScopeBackground.name()));
}

void OscilloscopTab::connectSignals() {
    connect(m_toolBar, &ScopeToolBar::samplingToggleRequested, this, [this](bool running) {
        onRunningToggleRequested(running);
    });

    connect(m_toolBar, &ScopeToolBar::viewModeChanged, this, [this](ScopeToolBar::ViewMode mode) {
        m_viewMode = mode;
        m_plotWidget->setViewMode(mode);
        logPlaceholderAction(mode == ScopeToolBar::ViewMode::Overlay
                                 ? QStringLiteral("Switch to Overlay")
                                 : QStringLiteral("Switch to Stacked"));
    });
    connect(m_toolBar, &ScopeToolBar::styleToggleRequested,
            this, &OscilloscopTab::toggleStylePanel);

    connect(m_bottomPanel, &ScopeBottomPanel::registerReadRequested, this, [this](int row) {
        logPlaceholderAction(QStringLiteral("Register R row %1").arg(row + 1));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::registerWriteRequested, this, [this](int row) {
        logPlaceholderAction(QStringLiteral("Register W row %1").arg(row + 1));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::clearPanelRequested, this, [this]() {
        logPlaceholderAction(QStringLiteral("Clear register panel"));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::loadParamsRequested, this, [this]() {
        logPlaceholderAction(QStringLiteral("Load parameters"));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelToggled, this, [this](int index, bool enabled) {
        updateChannelEnabled(index, enabled);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelDescriptionChanged, this, [this](int index, const QString &text) {
        updateChannelDescription(index, text);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::channelAddressChanged, this, [this](int index, const QString &text) {
        updateChannelAddress(index, text);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::sampleIntervalChanged, this, [this](const QString &text) {
        updateSampleInterval(text);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::displayWindowChanged, this, [this](const QString &text) {
        updateDisplayWindow(text);
    });
    connect(m_bottomPanel, &ScopeBottomPanel::captureNoteChanged, this, [this](const QString &text) {
        logPlaceholderAction(QStringLiteral("Capture note=%1").arg(text));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisAutoRequested, this, [this]() {
        m_plotWidget->setAutoYAxisRange();
        logPlaceholderAction(QStringLiteral("Y axis=auto"));
    });
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisManualRequested, this, [this](double minValue, double maxValue) {
        m_plotWidget->setManualYAxisRange(minValue, maxValue);
        logPlaceholderAction(QStringLiteral("Y axis=%1..%2")
                                 .arg(QString::number(minValue, 'f', 1))
                                 .arg(QString::number(maxValue, 'f', 1)));
    });
    connect(m_stylePanel, &ScopeStylePanel::colorChanged,
            this, &OscilloscopTab::updateChannelColor);
    connect(m_stylePanel, &ScopeStylePanel::lineWidthChanged,
            this, &OscilloscopTab::updateChannelLineWidth);
    connect(m_stylePanel, &ScopeStylePanel::lineStyleChanged,
            this, &OscilloscopTab::updateChannelLineStyle);
    connect(m_stylePanel, &ScopeStylePanel::defaultSettingsRequested,
            this, &OscilloscopTab::resetChannelStylesToDefault);

    if (m_serialManager != nullptr) {
        m_streamBatcher = new ScopeStreamBatcher();
        m_streamBatcher->moveToThread(m_serialManager->thread());
        connect(m_serialManager->thread(), &QThread::finished, m_streamBatcher, &QObject::deleteLater);
        connect(m_serialManager, &SerialManager::commandSent, this, &OscilloscopTab::handleCommandSent);
        connect(m_serialManager, &SerialManager::frameReceived, this, &OscilloscopTab::handleFrameReceived);
        connect(m_serialManager,
                &SerialManager::streamDataReceived,
                m_streamBatcher,
                &ScopeStreamBatcher::enqueue,
                Qt::DirectConnection);
        connect(m_streamBatcher,
                &ScopeStreamBatcher::batchReady,
                this,
                &OscilloscopTab::handleStreamBatchReceived,
                Qt::QueuedConnection);
        connect(m_serialManager, &SerialManager::errorOccurred, this, &OscilloscopTab::handleErrorOccurred);
    }
}

void OscilloscopTab::onRunningToggleRequested(bool running) {
    if (running) {
        startSamplingSequence();
        setRunning(m_running, true);
        return;
    }

    if (!m_running || m_pendingCommand != PendingCommand::None || m_serialManager == nullptr) {
        setRunning(m_running, true);
        return;
    }

    const QByteArray payload = MotorProtocol::encodeStopSampling();
    if (!sendCommand(MotorProtocol::CmdStopSampling, payload)) {
        setRunning(m_running, true);
        return;
    }

    clearPendingStreamBatch();
    m_stopPending = true;
    m_pendingCommand = PendingCommand::StopSampling;
    setRunning(m_running, true);
    logPlaceholderAction(QStringLiteral("TX stop sampling"));
}

void OscilloscopTab::setRunning(bool running, bool updateUi) {
    if (m_running == running) {
        if (updateUi) {
            m_toolBar->setRunning(running);
            m_bottomPanel->setRunning(running);
            m_plotWidget->setRunning(running);
        }
        return;
    }

    m_running = running;
    if (updateUi) {
        m_toolBar->setRunning(running);
        m_bottomPanel->setRunning(running);
        m_plotWidget->setRunning(running);
    }
    logPlaceholderAction(running ? QStringLiteral("Start") : QStringLiteral("Stop"));
}

void OscilloscopTab::updateChannelEnabled(int index, bool enabled) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].enabled = enabled;
    refreshPlotData();
    logPlaceholderAction(QStringLiteral("Channel %1 %2")
                             .arg(index + 1)
                             .arg(enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
}

void OscilloscopTab::updateChannelDescription(int index, const QString &text) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].description = text.trimmed();
    refreshPlotData();
    logPlaceholderAction(QStringLiteral("Channel %1 desc=%2").arg(index + 1).arg(text));
}

void OscilloscopTab::updateChannelAddress(int index, const QString &text) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].addressText = text.trimmed();
    m_channels[index].registerAddress = parseRegisterAddress(text);
    logPlaceholderAction(QStringLiteral("Channel %1 addr=%2").arg(index + 1).arg(text));
}

void OscilloscopTab::updateChannelColor(int index, const QColor &color) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].color = color;
    refreshPlotData();
}

void OscilloscopTab::updateChannelLineWidth(int index, int width) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].lineWidth = static_cast<qreal>(width);
    refreshPlotData();
}

void OscilloscopTab::updateChannelLineStyle(int index, Qt::PenStyle style) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].lineStyle = style;
    refreshPlotData();
}

void OscilloscopTab::resetChannelStylesToDefault() {
    const int count = qMin(m_channels.size(), m_defaultChannels.size());
    for (int index = 0; index < count; ++index) {
        m_channels[index].color = m_defaultChannels[index].color;
        m_channels[index].lineWidth = m_defaultChannels[index].lineWidth;
        m_channels[index].lineStyle = m_defaultChannels[index].lineStyle;

        m_stylePanel->setChannelColor(index, m_channels[index].color);
        m_stylePanel->setChannelLineWidth(index, static_cast<int>(m_channels[index].lineWidth));
        m_stylePanel->setChannelLineStyle(index, m_channels[index].lineStyle);
    }

    refreshPlotData();
    logPlaceholderAction(QStringLiteral("Reset channel styles to default"));
}

void OscilloscopTab::updateSampleInterval(const QString &text) {
    m_sampleIntervalIndex = sampleIntervalIndexForText(text);
    logPlaceholderAction(QStringLiteral("Sample interval=%1 (%2)")
                             .arg(text, formatByte(m_sampleIntervalIndex)));
}

void OscilloscopTab::updateDisplayWindow(const QString &text) {
    m_displayWindowMs = displayWindowMsForText(text);
    logPlaceholderAction(QStringLiteral("Display window=%1").arg(text));
}

void OscilloscopTab::toggleStylePanel() {
    m_stylePanel->setVisible(!m_stylePanel->isVisible());
}

void OscilloscopTab::startSamplingSequence() {
    if (m_serialManager == nullptr || m_pendingCommand != PendingCommand::None) {
        setRunning(m_running, true);
        return;
    }

    if (!hasValidSamplingConfig()) {
        m_lastError = QStringLiteral("No valid scope channel mapping");
        setRunning(false, true);
        logPlaceholderAction(QStringLiteral("Start blocked: %1").arg(m_lastError));
        return;
    }

    m_startPending = true;
    m_stopPending = false;
    m_hasReceivedStream = false;
    clearPendingStreamBatch();
    m_plotWidget->configureAcquisition(sampleIntervalUsForIndex(m_sampleIntervalIndex), m_displayWindowMs);
    m_plotWidget->resetView();
    logStartSnapshot();
    sendNextStartCommand();
}

void OscilloscopTab::sendNextStartCommand() {
    if (!m_startPending) {
        return;
    }

    switch (m_pendingCommand) {
    case PendingCommand::None: {
        const QByteArray payload = MotorProtocol::encodeSetSampleInterval(m_sampleIntervalIndex);
        if (!sendCommand(MotorProtocol::CmdSetSampleInterval, payload)) {
            m_startPending = false;
            setRunning(false, true);
            return;
        }
        m_pendingCommand = PendingCommand::SetSampleInterval;
        logPlaceholderAction(QStringLiteral("TX set sample interval %1").arg(formatByte(m_sampleIntervalIndex)));
        return;
    }
    case PendingCommand::SetSampleInterval: {
        const uint8_t mask = currentChannelMask();
        const QByteArray payload = MotorProtocol::encodeSetSampleChannels(mask);
        if (!sendCommand(MotorProtocol::CmdSetSampleChannels, payload)) {
            m_startPending = false;
            setRunning(false, true);
            return;
        }
        m_pendingCommand = PendingCommand::SetSampleChannels;
        logPlaceholderAction(QStringLiteral("TX set sample channels %1").arg(formatByte(mask)));
        return;
    }
    case PendingCommand::SetSampleChannels: {
        const QByteArray payload = MotorProtocol::encodeSetChannelRegisterMap(currentRegisterMap());
        if (!sendCommand(MotorProtocol::CmdSetChannelRegisterMap, payload)) {
            m_startPending = false;
            setRunning(false, true);
            return;
        }
        m_pendingCommand = PendingCommand::SetChannelRegisterMap;
        logPlaceholderAction(QStringLiteral("TX set channel register map"));
        return;
    }
    case PendingCommand::SetChannelRegisterMap: {
        const QByteArray payload = MotorProtocol::encodeStartSampling();
        if (!sendCommand(MotorProtocol::CmdStartSampling, payload)) {
            m_startPending = false;
            setRunning(false, true);
            return;
        }
        m_pendingCommand = PendingCommand::StartSampling;
        logPlaceholderAction(QStringLiteral("TX start sampling"));
        return;
    }
    case PendingCommand::StartSampling:
    case PendingCommand::StopSampling:
        return;
    }
}

void OscilloscopTab::finishPendingCommand() {
    switch (m_pendingCommand) {
    case PendingCommand::SetSampleInterval:
    case PendingCommand::SetSampleChannels:
    case PendingCommand::SetChannelRegisterMap:
        m_pendingSeq = InvalidSeq;
        sendNextStartCommand();
        return;
    case PendingCommand::StartSampling:
        m_startPending = false;
        clearPendingCommandState();
        setRunning(true, true);
        return;
    case PendingCommand::StopSampling:
        m_stopPending = false;
        clearPendingCommandState();
        setRunning(false, true);
        return;
    case PendingCommand::None:
        return;
    }
}

void OscilloscopTab::clearPendingCommandState() {
    m_pendingCommand = PendingCommand::None;
    m_pendingSeq = InvalidSeq;
}

void OscilloscopTab::handleCommandSent(uint8_t cmd, uint8_t seq) {
    if (m_pendingCommand == PendingCommand::None) {
        return;
    }

    if (cmd != commandForPending(m_pendingCommand)) {
        return;
    }

    m_pendingSeq = seq;
}

uint8_t OscilloscopTab::commandForPending(PendingCommand pending) {
    switch (pending) {
    case PendingCommand::SetSampleInterval:
        return MotorProtocol::CmdSetSampleInterval;
    case PendingCommand::SetSampleChannels:
        return MotorProtocol::CmdSetSampleChannels;
    case PendingCommand::SetChannelRegisterMap:
        return MotorProtocol::CmdSetChannelRegisterMap;
    case PendingCommand::StartSampling:
        return MotorProtocol::CmdStartSampling;
    case PendingCommand::StopSampling:
        return MotorProtocol::CmdStopSampling;
    case PendingCommand::None:
        return InvalidSeq;
    }

    return InvalidSeq;
}

void OscilloscopTab::handleFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (cmd == MotorProtocol::CmdErrorResponse) {
        if (m_pendingSeq != InvalidSeq && seq != m_pendingSeq) {
            return;
        }
        m_lastError = QStringLiteral("Protocol error %1").arg(formatByte(MotorProtocol::decodeErrorCode(data)));
        m_startPending = false;
        m_stopPending = false;
        clearPendingCommandState();
        setRunning(false, true);
        logPlaceholderAction(m_lastError);
        return;
    }

    if (cmd == MotorProtocol::CmdDebugInfo) {
        m_lastError = QString::fromUtf8(data);
        logPlaceholderAction(QStringLiteral("Debug info=%1").arg(m_lastError));
        return;
    }

    if (m_pendingSeq != InvalidSeq && seq != m_pendingSeq) {
        return;
    }

    switch (m_pendingCommand) {
    case PendingCommand::SetSampleInterval:
        if (cmd == MotorProtocol::CmdSetSampleInterval) {
            finishPendingCommand();
        }
        break;
    case PendingCommand::SetSampleChannels:
        if (cmd == MotorProtocol::CmdSetSampleChannels) {
            finishPendingCommand();
        }
        break;
    case PendingCommand::SetChannelRegisterMap:
        if (cmd == MotorProtocol::CmdSetChannelRegisterMap) {
            finishPendingCommand();
        }
        break;
    case PendingCommand::StartSampling:
        if (cmd == MotorProtocol::CmdStartSampling) {
            finishPendingCommand();
        }
        break;
    case PendingCommand::StopSampling:
        if (cmd == MotorProtocol::CmdStopSampling) {
            finishPendingCommand();
        }
        break;
    case PendingCommand::None:
        break;
    }
}

void OscilloscopTab::handleErrorOccurred(const QString &message) {
    if (m_pendingCommand == PendingCommand::None) {
        return;
    }

    m_lastError = message;
    m_startPending = false;
    m_stopPending = false;
    clearPendingCommandState();
    setRunning(false, true);
    logPlaceholderAction(QStringLiteral("Serial error=%1").arg(message));
}

void OscilloscopTab::handleStreamDataReceived(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (!m_running || m_debugStreamActive) {
        return;
    }

    m_lastStreamMask = channelMask;
    m_hasReceivedStream = true;
    m_plotWidget->appendSamples(channelMask, samples);
}

void OscilloscopTab::handleStreamBatchReceived(const ScopeStreamBatch &batch) {
    if (!m_running || m_debugStreamActive || batch.isEmpty()) {
        return;
    }

    m_perfBatchCount += 1;
    for (const ScopeStreamPacket &packet : batch) {
        m_lastStreamMask = packet.channelMask;
        m_hasReceivedStream = true;
        m_perfSampleCount += packet.samples.size();
        m_plotWidget->appendSamples(packet.channelMask, packet.samples);
    }
}

void OscilloscopTab::clearPendingStreamBatch() {
    if (m_streamBatcher == nullptr) {
        return;
    }

    QMetaObject::invokeMethod(m_streamBatcher, &ScopeStreamBatcher::clearPending, Qt::QueuedConnection);
}

void OscilloscopTab::setDebugStreamActive(bool active) {
    m_debugStreamActive = active;
}

void OscilloscopTab::ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (!m_running || !m_debugStreamActive) {
        return;
    }

    m_lastStreamMask = channelMask;
    m_hasReceivedStream = true;
    m_perfBatchCount += 1;
    m_perfSampleCount += samples.size();
    m_plotWidget->appendSamples(channelMask, samples);
}

void OscilloscopTab::refreshPlotData() {
    QVector<ScopePlotWidget::PlotChannelData> plotChannels;
    plotChannels.reserve(m_channels.size());

    for (int index = 0; index < m_channels.size(); ++index) {
        const ScopeChannelState &channel = m_channels.at(index);
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
        plotChannels.append(plotChannel);
    }

    m_plotWidget->setChannelData(plotChannels);
}

bool OscilloscopTab::hasValidSamplingConfig() const {
    return currentChannelMask() != 0x00;
}

uint8_t OscilloscopTab::currentChannelMask() const {
    uint8_t mask = 0;
    for (int index = 0; index < m_channels.size(); ++index) {
        if (m_channels.at(index).enabled && m_channels.at(index).registerAddress != 0xFFFF) {
            mask |= static_cast<uint8_t>(1u << index);
        }
    }
    return mask;
}

QVector<quint16> OscilloscopTab::currentRegisterMap() const {
    QVector<quint16> mapping;
    mapping.reserve(m_channels.size());
    for (const ScopeChannelState &channel : m_channels) {
        mapping.append(channel.enabled ? channel.registerAddress : 0xFFFF);
    }
    return mapping;
}

quint16 OscilloscopTab::parseRegisterAddress(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return 0xFFFF;
    }

    bool ok = false;
    const uint value = trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
                           ? trimmed.mid(2).toUInt(&ok, 16)
                           : trimmed.toUInt(&ok, 16);
    if (!ok || value > 0xFFFF) {
        return 0xFFFF;
    }
    return static_cast<quint16>(value);
}

uint8_t OscilloscopTab::sampleIntervalIndexForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("100 us")) {
        return 0x00;
    }
    if (normalized == QStringLiteral("200 us")) {
        return 0x01;
    }
    if (normalized == QStringLiteral("300 us")) {
        return 0x02;
    }
    if (normalized == QStringLiteral("500 us")) {
        return 0x03;
    }
    if (normalized == QStringLiteral("750 us")) {
        return 0x04;
    }
    if (normalized == QStringLiteral("1500 us")) {
        return 0x06;
    }
    if (normalized == QStringLiteral("2000 us")) {
        return 0x07;
    }
    return 0x05;
}

int OscilloscopTab::sampleIntervalUsForIndex(uint8_t index) {
    switch (index) {
    case 0x00:
        return 100;
    case 0x01:
        return 200;
    case 0x02:
        return 300;
    case 0x03:
        return 500;
    case 0x04:
        return 750;
    case 0x05:
        return 1000;
    case 0x06:
        return 1500;
    case 0x07:
        return 2000;
    default:
        return 1000;
    }
}

int OscilloscopTab::displayWindowMsForText(const QString &text) {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("50 ms")) {
        return 50;
    }
    if (normalized == QStringLiteral("200 ms")) {
        return 200;
    }
    if (normalized == QStringLiteral("500 ms")) {
        return 500;
    }
    if (normalized == QStringLiteral("1000 ms")) {
        return 1000;
    }
    if (normalized == QStringLiteral("2000 ms")) {
        return 2000;
    }
    if (normalized == QStringLiteral("4000 ms")) {
        return 4000;
    }
    return 50;
}

bool OscilloscopTab::sendCommand(uint8_t cmd, const QByteArray &data) {
    if (m_serialManager == nullptr) {
        m_lastError = QStringLiteral("Serial manager is unavailable");
        logPlaceholderAction(m_lastError);
        return false;
    }

    QMetaObject::invokeMethod(
        m_serialManager,
        "sendCommand",
        Qt::QueuedConnection,
        Q_ARG(uint8_t, cmd),
        Q_ARG(QByteArray, data));
    return true;
}

void OscilloscopTab::logStartSnapshot() const {
    const int intervalUs = sampleIntervalUsForIndex(m_sampleIntervalIndex);
    const int rawWindowPoints = qMax(
        1,
        qRound((static_cast<double>(m_displayWindowMs) * 1000.0) / static_cast<double>(intervalUs)));
    const int bucket = qMax(1, (rawWindowPoints + 3000 - 1) / 3000);
    const int uiPoints = qMax(1, (rawWindowPoints + bucket - 1) / bucket);
    QStringList activeChannels;
    for (int index = 0; index < m_channels.size(); ++index) {
        const ScopeChannelState &channel = m_channels.at(index);
        if (!channel.enabled || channel.registerAddress == 0xFFFF) {
            continue;
        }

        activeChannels.append(QStringLiteral("CH%1=%2")
                                  .arg(index + 1)
                                  .arg(channel.addressText.isEmpty() ? QStringLiteral("0xFFFF") : channel.addressText));
    }

    qDebug().noquote()
        << QStringLiteral("[Scope Start] interval=%1us index=%2 window=%3ms raw_points=%4 bucket=%5 ui_points=%6 mask=%7 channels=%8")
               .arg(intervalUs)
               .arg(formatByte(m_sampleIntervalIndex))
               .arg(m_displayWindowMs)
               .arg(rawWindowPoints)
               .arg(bucket)
               .arg(formatByte(currentChannelMask()))
               .arg(activeChannels.join(QStringLiteral(", ")));
}

void OscilloscopTab::logPlaceholderAction(const QString &action) {
    qDebug().noquote() << QStringLiteral("[Scope GUI] %1").arg(action);
}

void OscilloscopTab::onPerfTimerTick() {
    if (!m_running) {
        return;
    }

    qDebug().noquote()
        << QStringLiteral("[Scope Perf] paint=%1ms fps=%2 batchPerSec=%3 samplesPerSec=%4")
               .arg(QString::number(m_plotWidget->paintAverageMs(), 'f', 2),
                    QString::number(m_plotWidget->paintFps(), 'f', 1),
                    QString::number(m_perfBatchCount),
                    QString::number(m_perfSampleCount));
    m_perfBatchCount = 0;
    m_perfSampleCount = 0;
}
