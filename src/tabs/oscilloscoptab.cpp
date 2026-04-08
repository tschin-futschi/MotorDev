#include "tabs/oscilloscoptab.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scopetoolbar.h"

#include <QDebug>
#include <QMetaObject>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QVector<double> makeInitialScopeSamples() {
    return QVector<double>(160, 0.0);
}
}

OscilloscopTab::OscilloscopTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent)
    , m_serialManager(serialManager)
    , m_channels({
          {true, QStringLiteral("Speed"), QStringLiteral("0x0010"), 0x0010, Style::Color::ScopeWaveCh1, makeInitialScopeSamples()},
          {true, QStringLiteral("Torque"), QStringLiteral("0x0012"), 0x0012, Style::Color::ScopeWaveCh2, makeInitialScopeSamples()},
          {true, QStringLiteral("Temp"), QStringLiteral("0x0014"), 0x0014, Style::Color::ScopeWaveCh3, makeInitialScopeSamples()},
          {true, QStringLiteral("Current"), QStringLiteral("0x0016"), 0x0016, Style::Color::ScopeWaveCh4, makeInitialScopeSamples()},
          {false, QString(), QStringLiteral("0x0018"), 0x0018, QColor(QStringLiteral("#8ecae6")), makeInitialScopeSamples()},
          {false, QString(), QStringLiteral("0x001A"), 0x001A, QColor(QStringLiteral("#b08968")), makeInitialScopeSamples()},
          {false, QString(), QStringLiteral("0x001C"), 0x001C, QColor(QStringLiteral("#f28482")), makeInitialScopeSamples()},
          {false, QString(), QStringLiteral("0x001E"), 0x001E, QColor(QStringLiteral("#84a98c")), makeInitialScopeSamples()},
      }) {
    setupUi();
    connectSignals();
    refreshPlotData();
}

void OscilloscopTab::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mainContent = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(mainContent);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_toolBar = new ScopeToolBar(this);
    m_plotWidget = new ScopePlotWidget(this);
    m_bottomPanel = new ScopeBottomPanel(mainContent, this);

    mainLayout->addWidget(m_toolBar);
    mainLayout->addWidget(m_plotWidget, 1);
    mainLayout->addWidget(m_bottomPanel);

    layout->addWidget(mainContent, 1);

    setStyleSheet(QStringLiteral("background:%1;").arg(Style::Color::ScopeBackground.name()));
}

void OscilloscopTab::connectSignals() {
    connect(m_toolBar, &ScopeToolBar::viewModeChanged, this, [this](ScopeToolBar::ViewMode mode) {
        m_viewMode = mode;
        m_plotWidget->setViewMode(mode);
        logPlaceholderAction(mode == ScopeToolBar::ViewMode::Overlay
                                 ? QStringLiteral("Switch to Overlay")
                                 : QStringLiteral("Switch to Stacked"));
    });

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
    connect(m_bottomPanel, &ScopeBottomPanel::runningToggled, this, [this](bool running) {
        onRunningToggleRequested(running);
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

    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::frameReceived, this, &OscilloscopTab::handleFrameReceived);
        connect(m_serialManager, &SerialManager::streamDataReceived, this, &OscilloscopTab::handleStreamDataReceived);
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

void OscilloscopTab::updateSampleInterval(const QString &text) {
    m_sampleIntervalIndex = sampleIntervalIndexForText(text);
    logPlaceholderAction(QStringLiteral("Sample interval=%1 (%2)")
                             .arg(text, formatByte(m_sampleIntervalIndex)));
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
        sendNextStartCommand();
        return;
    case PendingCommand::StartSampling:
        m_startPending = false;
        m_pendingCommand = PendingCommand::None;
        setRunning(true, true);
        return;
    case PendingCommand::StopSampling:
        m_stopPending = false;
        m_pendingCommand = PendingCommand::None;
        setRunning(false, true);
        return;
    case PendingCommand::None:
        return;
    }
}

void OscilloscopTab::handleFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);

    if (cmd == MotorProtocol::CmdErrorResponse) {
        m_lastError = QStringLiteral("Protocol error %1").arg(formatByte(MotorProtocol::decodeErrorCode(data)));
        m_startPending = false;
        m_stopPending = false;
        m_pendingCommand = PendingCommand::None;
        setRunning(false, true);
        logPlaceholderAction(m_lastError);
        return;
    }

    if (cmd == 0x06) {
        m_lastError = QString::fromUtf8(data);
        logPlaceholderAction(QStringLiteral("Debug info=%1").arg(m_lastError));
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
    m_pendingCommand = PendingCommand::None;
    setRunning(false, true);
    logPlaceholderAction(QStringLiteral("Serial error=%1").arg(message));
}

void OscilloscopTab::handleStreamDataReceived(uint8_t channelMask, const QVector<int16_t> &samples) {
    if (!m_running) {
        return;
    }

    m_lastStreamMask = channelMask;
    m_hasReceivedStream = true;

    int sampleIndex = 0;
    for (int channelIndex = 0; channelIndex < m_channels.size(); ++channelIndex) {
        if ((channelMask & (1u << channelIndex)) == 0) {
            continue;
        }
        if (sampleIndex >= samples.size()) {
            break;
        }

        QVector<double> &buffer = m_channels[channelIndex].samples;
        buffer.append(static_cast<double>(samples.at(sampleIndex)));
        if (buffer.size() > 512) {
            buffer.remove(0, buffer.size() - 512);
        }
        ++sampleIndex;
    }

    refreshPlotData();
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
        plotChannel.name = channel.description.isEmpty()
                               ? QStringLiteral("CH%1").arg(index + 1)
                               : QStringLiteral("CH%1 %2").arg(index + 1).arg(channel.description);
        plotChannel.color = channel.color;
        plotChannel.samples = channel.samples;
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
    if (normalized == QStringLiteral("500 us")) {
        return 0x03;
    }
    if (normalized == QStringLiteral("2000 us")) {
        return 0x07;
    }
    return 0x05;
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

void OscilloscopTab::logPlaceholderAction(const QString &action) {
    qDebug().noquote() << QStringLiteral("[Scope GUI] %1").arg(action);
}
