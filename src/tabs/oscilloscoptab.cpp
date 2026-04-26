// =============================================================================
// @file    oscilloscoptab.cpp
// @brief   示波器页面实现 — UI 构建、多服务协调、全屏/样式面板切换
// =============================================================================
#include "tabs/oscilloscoptab.h"

#include "models/scopechannelmodel.h"
#include "protocol/register_utils.h"
#include "protocol/sampling_config.h"
#include "services/commanddispatcher.h"
#include "services/cyclicwriteservice.h"
#include "services/generatorservice.h"
#include "services/registerservice.h"
#include "services/scopeservice.h"
#include "ui/style_constants.h"
#include "widgets/scopebottompanel.h"
#include "widgets/scopemarqueelabel.h"
#include "widgets/scopeplotwidget.h"
#include "widgets/scoperegisterpanel.h"
#include "widgets/scopestylepanel.h"

#include <QDebug>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

OscilloscopTab::OscilloscopTab(SerialManager *serialManager,
                               CommandDispatcher *dispatcher,
                               QWidget *parent)
    : QWidget(parent)
    , m_serialManager(serialManager)
    , m_dispatcher(dispatcher) {
    // 初始化各模型和服务
    m_channelModel = new ScopeChannelModel(this);
    m_service = new ScopeService(serialManager, dispatcher, m_channelModel, this);
    m_regService = new RegisterService(dispatcher, this);
    m_generatorService = new GeneratorService(dispatcher, this);
    m_cyclicWriteService = new CyclicWriteService(m_regService, this);
    m_cyclicWriteService->setRowCount(ScopeRegisterPanel::rowCount());

    setupUi();
    connectSignals();
    refreshPlotData();

    // 性能统计定时器：每秒输出一次绘制耗时、FPS、采样率
    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(1000);
    connect(m_perfTimer, &QTimer::timeout, this, &OscilloscopTab::onPerfTimerTick);
    m_perfTimer->start();
}

OscilloscopTab::~OscilloscopTab() {
    exitFullscreen(); // 确保全屏窗口被正确清理
}

// =============================================================================
// UI 构建
// =============================================================================

void OscilloscopTab::setupUi() {
    setObjectName(QStringLiteral("OscilloscopTab"));
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setStretch(0, 1);

    // 水平分割器：左侧波形区 | 右侧样式面板（可折叠）
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("splitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(0);
    rootLayout->addWidget(m_splitter);

    // --- 波形区（PlotWidget + BottomPanel 垂直排列）---
    m_plotArea = new QWidget(m_splitter);
    m_plotArea->setObjectName(QStringLiteral("plotArea"));
    m_plotLayout = new QVBoxLayout(m_plotArea);
    m_plotLayout->setObjectName(QStringLiteral("plotLayout"));
    m_plotLayout->setSpacing(0);
    m_plotLayout->setContentsMargins(0, 0, 0, 0);

    m_plotWidget = new ScopePlotWidget(m_plotArea);
    m_plotWidget->setObjectName(QStringLiteral("plotWidget"));
    m_plotLayout->addWidget(m_plotWidget);

    // 底部面板宿主
    m_bottomPanelHost = new QWidget(m_plotArea);
    m_bottomPanelHost->setObjectName(QStringLiteral("bottomPanelHost"));
    m_plotLayout->addWidget(m_bottomPanelHost);

    // --- 样式面板（默认隐藏）---
    m_stylePanel = new ScopeStylePanel(m_splitter);
    m_stylePanel->setObjectName(QStringLiteral("stylePanel"));

    // 创建底部面板
    auto *bottomLayout = qobject_cast<QVBoxLayout *>(m_bottomPanelHost->layout());
    if (bottomLayout == nullptr) {
        bottomLayout = new QVBoxLayout(m_bottomPanelHost);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(0);
    }

    m_bottomPanel = new ScopeBottomPanel(m_plotArea, m_bottomPanelHost);
    bottomLayout->addWidget(m_bottomPanel);
    m_plotLayout->setStretch(0, 1);  // PlotWidget 占满剩余空间
    m_plotLayout->setStretch(1, 0);  // BottomPanel 固定高度
    m_stylePanel->setVisible(false);

    // 从通道模型初始化样式面板
    for (int i = 0; i < m_channelModel->channelCount(); ++i) {
        const ScopeChannelState &ch = m_channelModel->channel(i);
        m_stylePanel->setChannelColor(i, ch.color);
        m_stylePanel->setChannelLineWidth(i, static_cast<int>(ch.lineWidth));
        m_stylePanel->setChannelLineStyle(i, ch.lineStyle);
        m_stylePanel->setChannelShowDataPoints(i, ch.showDataPoints);
    }

    m_splitter->setStretchFactor(0, 1); // 波形区弹性伸展
    m_splitter->setStretchFactor(1, 0); // 样式面板固定宽度
}

// =============================================================================
// 信号槽连接
// =============================================================================

void OscilloscopTab::connectSignals() {
    // -------------------------------------------------------------------------
    // PlotWidget 事件
    // -------------------------------------------------------------------------
    connect(m_plotWidget, &ScopePlotWidget::fullscreenToggleRequested, this, &OscilloscopTab::togglePlotFullscreen);
    connect(m_plotWidget, &ScopePlotWidget::samplingToggleRequested, this, &OscilloscopTab::onSamplingToggleRequested);

    // -------------------------------------------------------------------------
    // BottomPanel → ChannelModel：通道启用/描述/地址
    // -------------------------------------------------------------------------
    connect(m_bottomPanel, &ScopeBottomPanel::channelToggled, m_channelModel, &ScopeChannelModel::setEnabled);
    connect(m_bottomPanel, &ScopeBottomPanel::channelDescriptionChanged, m_channelModel, &ScopeChannelModel::setDescription);
    connect(m_bottomPanel, &ScopeBottomPanel::channelAddressChanged, m_channelModel, &ScopeChannelModel::setAddressText);

    // -------------------------------------------------------------------------
    // BottomPanel → 采样配置
    // -------------------------------------------------------------------------
    connect(m_bottomPanel, &ScopeBottomPanel::sampleIntervalChanged, this, [this](const QString &text) {
        m_sampleIntervalIndex = SamplingConfig::intervalIndexForText(text);
        m_sampleIntervalText = text;
        qDebug().noquote() << QStringLiteral("[Scope GUI] Sample interval=%1 (0x%2)")
                                  .arg(text)
                                  .arg(m_sampleIntervalIndex, 2, 16, QLatin1Char('0'));
        refreshMarqueeStatus();
    });
    connect(m_bottomPanel, &ScopeBottomPanel::displayWindowChanged, this, [this](const QString &text) {
        m_displayWindowMs = SamplingConfig::displayWindowMsForText(text);
        qDebug().noquote() << QStringLiteral("[Scope GUI] Display window=%1").arg(text);
    });

    // -------------------------------------------------------------------------
    // BottomPanel → 寄存器面板 / 杂项
    // -------------------------------------------------------------------------
    connect(m_bottomPanel, &ScopeBottomPanel::registerReadRequested, this, &OscilloscopTab::onRegisterReadRequested);
    connect(m_bottomPanel, &ScopeBottomPanel::registerWriteRequested, this, &OscilloscopTab::onRegisterWriteRequested);
    connect(m_bottomPanel, &ScopeBottomPanel::registerStartRequested, this, &OscilloscopTab::onRegisterStartRequested);
    connect(m_bottomPanel, &ScopeBottomPanel::registerStopRequested, this, &OscilloscopTab::onRegisterStopRequested);
    connect(m_bottomPanel, &ScopeBottomPanel::clearPanelRequested, this, &OscilloscopTab::onRegisterClearRequested);
    connect(m_bottomPanel, &ScopeBottomPanel::loadParamsRequested, this, []() {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Load parameters");
    });

    // BottomPanel → 波形生成器服务
    connect(m_bottomPanel, &ScopeBottomPanel::generatorLinearStartRequested,
            m_generatorService, &GeneratorService::startLinear);
    connect(m_bottomPanel, &ScopeBottomPanel::generatorCosineStartRequested,
            m_generatorService, &GeneratorService::startCosine);
    connect(m_bottomPanel, &ScopeBottomPanel::generatorStopRequested,
            m_generatorService, &GeneratorService::stop);
    connect(m_bottomPanel, &ScopeBottomPanel::captureNoteChanged, this, [](const QString &text) {
        qDebug().noquote() << QStringLiteral("[Scope GUI] Capture note=%1").arg(text);
    });

    // -------------------------------------------------------------------------
    // BottomPanel → PlotWidget Y 轴控制
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // StylePanel → ChannelModel：通道样式属性
    // -------------------------------------------------------------------------
    connect(m_stylePanel, &ScopeStylePanel::colorChanged, m_channelModel, &ScopeChannelModel::setColor);
    connect(m_stylePanel, &ScopeStylePanel::lineWidthChanged, m_channelModel, &ScopeChannelModel::setLineWidth);
    connect(m_stylePanel, &ScopeStylePanel::lineStyleChanged, m_channelModel, &ScopeChannelModel::setLineStyle);
    connect(m_stylePanel, &ScopeStylePanel::dataPointsChanged, m_channelModel, &ScopeChannelModel::setShowDataPoints);
    connect(m_stylePanel, &ScopeStylePanel::defaultSettingsRequested, m_channelModel, &ScopeChannelModel::resetStylesToDefault);

    // -------------------------------------------------------------------------
    // ChannelModel → 刷新绘图 + 样式面板同步
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // ScopeService → UI：采样状态和数据
    // -------------------------------------------------------------------------
    connect(m_service, &ScopeService::runningChanged, this, [this](bool running) {
        m_bottomPanel->setRunning(running);
        m_plotWidget->setRunning(running);
        m_scopeRunning = running;
        refreshMarqueeStatus();
        emit runningChanged(running);
    });
    connect(m_service, &ScopeService::samplesReceived, m_plotWidget, &ScopePlotWidget::appendSamples);
    connect(m_service, &ScopeService::acquisitionConfigured, m_plotWidget, &ScopePlotWidget::configureAcquisition);
    connect(m_service, &ScopeService::resetViewRequested, m_plotWidget, &ScopePlotWidget::resetView);

    // -------------------------------------------------------------------------
    // RegisterService → 寄存器面板反馈
    // -------------------------------------------------------------------------
    connect(m_regService, &RegisterService::rowReadOk, this, [this](int row, qint16 value) {
        if (row < 0 || m_bottomPanel == nullptr || m_bottomPanel->registerPanel() == nullptr) {
            return;
        }

        ScopeRegisterPanel *panel = m_bottomPanel->registerPanel();
        panel->setValueText(
            row, QStringLiteral("0x%1").arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0')).toUpper());
        panel->setButtonFeedback(row, true, QStringLiteral("ok"));
    });
    connect(m_regService, &RegisterService::rowReadError, this, [this](int row) {
        if (row < 0) {
            return;
        }
        qDebug().noquote() << QStringLiteral("[Scope Reg] Read error row %1").arg(row + 1);
        if (m_bottomPanel != nullptr && m_bottomPanel->registerPanel() != nullptr) {
            m_bottomPanel->registerPanel()->setButtonFeedback(row, true, QStringLiteral("error"));
        }
    });
    connect(m_regService, &RegisterService::rowWriteOk, this, [this](int row) {
        if (row < 0) {
            return;
        }
        qDebug().noquote() << QStringLiteral("[Scope Reg] Write OK row %1").arg(row + 1);
        if (m_bottomPanel != nullptr && m_bottomPanel->registerPanel() != nullptr) {
            m_bottomPanel->registerPanel()->setButtonFeedback(row, false, QStringLiteral("ok"));
        }
    });
    connect(m_regService, &RegisterService::rowWriteError, this, [this](int row) {
        if (row < 0) {
            return;
        }
        qDebug().noquote() << QStringLiteral("[Scope Reg] Write error row %1").arg(row + 1);
        if (m_bottomPanel != nullptr && m_bottomPanel->registerPanel() != nullptr) {
            m_bottomPanel->registerPanel()->setButtonFeedback(row, false, QStringLiteral("error"));
        }
    });

    // 循环写入数据提供者：从寄存器面板读取地址和值
    if (m_bottomPanel != nullptr && m_bottomPanel->registerPanel() != nullptr) {
        m_cyclicWriteService->setRowDataProvider([this](int row, quint16 &addr, quint16 &value) -> bool {
            if (m_bottomPanel == nullptr || m_bottomPanel->registerPanel() == nullptr) {
                return false;
            }

            ScopeRegisterPanel *panel = m_bottomPanel->registerPanel();
            const QString addrText = panel->addressText(row).trimmed();
            const QString valueText = panel->valueText(row).trimmed();
            if (addrText.isEmpty() || valueText.isEmpty()) {
                return false;
            }
            if (!RegisterUtils::parseNumber(addrText, &addr)) {
                return false;
            }
            if (!RegisterUtils::parseNumber(valueText, &value)) {
                return false;
            }
            panel->setAddressError(row, false);
            return true;
        });
    }

    // -------------------------------------------------------------------------
    // 生成器/循环写入状态 → 跑马灯更新
    // -------------------------------------------------------------------------
    connect(m_generatorService, &GeneratorService::runningChanged, this, [this](bool running) {
        if (m_bottomPanel != nullptr && m_bottomPanel->generatorPanel() != nullptr) {
            m_bottomPanel->generatorPanel()->setRunning(running);
        }
        refreshMarqueeStatus();
    });
    connect(m_cyclicWriteService, &CyclicWriteService::runningChanged, this, [this](bool running) {
        if (m_bottomPanel != nullptr && m_bottomPanel->registerPanel() != nullptr) {
            m_bottomPanel->registerPanel()->setCyclicRunning(running);
        }
        refreshMarqueeStatus();
    });
}

// =============================================================================
// 公共槽
// =============================================================================

/// @brief 切换视图模式（Overlay ↔ Stacked）
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

/// @brief 采样启停请求处理
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

// =============================================================================
// 通道数据同步
// =============================================================================

/// @brief 从 ChannelModel 读取启用通道的配置，构造 PlotChannelData 列表传给 PlotWidget
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

// =============================================================================
// 全屏控制
// =============================================================================

void OscilloscopTab::toggleStylePanel() { m_stylePanel->setVisible(!m_stylePanel->isVisible()); }

/// @brief 退出全屏：将 PlotWidget 从独立窗口移回原布局位置
void OscilloscopTab::exitFullscreen() {
    if (m_fullscreenWindow == nullptr) {
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
}

/// @brief 切换全屏：将 PlotWidget 移入独立无边框全屏窗口
void OscilloscopTab::togglePlotFullscreen() {
    if (m_fullscreenWindow == nullptr) {
        // 进入全屏：记录原始位置，创建无边框全屏窗口
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

    // 退出全屏
    exitFullscreen();
    m_plotWidget->setFocus();
}

/// @brief Escape 键退出全屏
void OscilloscopTab::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_fullscreenWindow != nullptr) {
        togglePlotFullscreen();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// =============================================================================
// 视图模式转换
// =============================================================================

ScopeViewMode OscilloscopTab::viewModeFromInt(int mode) { return mode == 1 ? ScopeViewMode::Stacked : ScopeViewMode::Overlay; }
int OscilloscopTab::viewModeToInt(ScopeViewMode mode) { return mode == ScopeViewMode::Stacked ? 1 : 0; }

// =============================================================================
// 寄存器面板操作
// =============================================================================

/// @brief 单行寄存器读取：解析地址，设置 pending 状态，发起读取
void OscilloscopTab::onRegisterReadRequested(int row) {
    if (m_regService == nullptr || m_bottomPanel == nullptr || m_bottomPanel->registerPanel() == nullptr) {
        return;
    }

    ScopeRegisterPanel *panel = m_bottomPanel->registerPanel();
    quint16 address = 0;
    if (!RegisterUtils::parseNumber(panel->addressText(row), &address)) {
        panel->setAddressError(row, true);
        return;
    }

    panel->setAddressError(row, false);
    panel->setButtonFeedback(row, true, QStringLiteral("pending"));
    m_regService->readSingleRow(row, address);
}

/// @brief 单行寄存器写入：解析地址和值，设置 pending 状态，发起写入
void OscilloscopTab::onRegisterWriteRequested(int row) {
    if (m_regService == nullptr || m_bottomPanel == nullptr || m_bottomPanel->registerPanel() == nullptr) {
        return;
    }

    ScopeRegisterPanel *panel = m_bottomPanel->registerPanel();
    quint16 address = 0;
    quint16 value = 0;
    if (!RegisterUtils::parseNumber(panel->addressText(row), &address)) {
        panel->setAddressError(row, true);
        return;
    }
    if (!RegisterUtils::parseNumber(panel->valueText(row), &value)) {
        qDebug().noquote() << QStringLiteral("[Scope Reg] Skip invalid value row %1").arg(row + 1);
        return;
    }

    panel->setAddressError(row, false);
    panel->setButtonFeedback(row, false, QStringLiteral("pending"));
    m_regService->writeSingleRow(row, address, static_cast<qint16>(value));
}

/// @brief 启动循环写入：从寄存器面板读取间隔参数
void OscilloscopTab::onRegisterStartRequested() {
    if (m_bottomPanel == nullptr || m_bottomPanel->registerPanel() == nullptr || m_cyclicWriteService == nullptr) {
        return;
    }

    bool ok = false;
    const int intervalMs = m_bottomPanel->registerPanel()->intervalText().trimmed().toInt(&ok);
    if (!ok || intervalMs <= 0) {
        qDebug().noquote() << QStringLiteral("[Scope Reg] Ignore invalid interval");
        return;
    }

    m_cyclicWriteService->start(intervalMs);
}

void OscilloscopTab::onRegisterStopRequested() {
    if (m_cyclicWriteService != nullptr) {
        m_cyclicWriteService->stop();
    }
}

/// @brief 清除寄存器面板：停止循环写入 + 清空所有行
void OscilloscopTab::onRegisterClearRequested() {
    if (m_cyclicWriteService != nullptr) {
        m_cyclicWriteService->stop();
    }
    if (m_bottomPanel != nullptr && m_bottomPanel->registerPanel() != nullptr) {
        m_bottomPanel->registerPanel()->clearAll();
    }

    refreshMarqueeStatus();
    qDebug().noquote() << QStringLiteral("[Scope GUI] Clear register panel");
}

// =============================================================================
// 性能统计
// =============================================================================

/// @brief 每秒输出一次性能数据：绘制耗时、FPS、批次/秒、采样数/秒
void OscilloscopTab::onPerfTimerTick() {
    if (!m_service->isRunning()) return;
    qDebug().noquote() << QStringLiteral("[Scope Perf] paint=%1ms fps=%2 batchPerSec=%3 samplesPerSec=%4")
                              .arg(QString::number(m_plotWidget->paintAverageMs(), 'f', 2),
                                   QString::number(m_plotWidget->paintFps(), 'f', 1),
                                   QString::number(m_service->perfBatchCount()),
                                   QString::number(m_service->perfSampleCount()));
    m_service->resetPerfCounters();
}

// =============================================================================
// 跑马灯状态
// =============================================================================

/// @brief 刷新底部跑马灯显示文本，汇总当前活跃的采样/循环写入/生成器状态
void OscilloscopTab::refreshMarqueeStatus() {
    if (m_bottomPanel == nullptr || m_bottomPanel->marqueeLabel() == nullptr) {
        return;
    }

    QStringList parts;
    if (m_scopeRunning) {
        QString sampleText = m_sampleIntervalText;
        sampleText.remove(QLatin1Char(' '));
        parts << QStringLiteral("Sampling @ %1").arg(sampleText);
    }
    if (m_cyclicWriteService != nullptr && m_cyclicWriteService->isRunning()) {
        parts << QStringLiteral("Cyclic Write @ %1ms").arg(m_cyclicWriteService->intervalMs());
    }
    if (m_generatorService != nullptr && m_generatorService->isRunning()) {
        if (m_generatorService->modeLabel() == QStringLiteral("Cosine")) {
            parts << QStringLiteral("Generator: Cosine %1ch").arg(m_generatorService->cosineChannelCount());
        } else {
            parts << QStringLiteral("Generator: %1").arg(m_generatorService->modeLabel());
        }
    }

    m_bottomPanel->marqueeLabel()->setText(parts.isEmpty() ? QStringLiteral("Idle")
                                                           : parts.join(QStringLiteral("  |  ")));
}
