// =============================================================================
// @file    scopepreviewwidget.cpp
// @brief   示波器预览控件实现 — 模拟数据生成、完整示波器 UI 组装
//
// 本控件是示波器功能的"独立预览"组装容器，不依赖真实串口数据，
// 可用于 UI 开发调试和功能演示。内部管理 8 通道状态，
// 并用定时器持续生成正弦模拟波形喂给 ScopePlotWidget。
//
// 整体布局（自上而下）：
//   [TopBar]                        ← 顶部工具栏（视图模式切换、样式开关等）
//   [QSplitter: 水平]
//     ├─ [plotArea: 垂直]
//     │    ├─ [ScopePlotWidget]     ← OpenGL 波形绘制区（stretch = 1）
//     │    └─ [bottomPanelHost]     ← 底部面板宿主（stretch = 0）
//     │         └─ [ScopeBottomPanel] ← 通道开关、采样率、Y 轴等
//     └─ [ScopeStylePanel]          ← 右侧样式面板（默认隐藏）
//
// 模拟波形公式：
//   carrier  = sin(phase * 0.08 + channelIndex * 0.55)
//   harmonic = sin(carrier_t * (1.6 + channelIndex * 0.08))
//   value    = carrier * (300 + channelIndex * 55) + harmonic * 90
//   → 每通道基频不同、幅值不同，叠加高次谐波后视觉上有明显区分。
//
// 通道状态管理：
//   m_channels[0..7]   ← 当前通道配置（颜色/线宽/线型/启用状态等）
//   m_defaultChannels   ← 构造时快照，用于 resetStyleDefaults 一键恢复
// =============================================================================
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

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造预览控件：搭建 UI → 初始化 8 通道默认配置 → 连接信号 → 创建模拟定时器。
///
/// 通道默认配置：
///   - CH1~CH4：启用，分别命名 Speed / Torque / Temp / Current
///   - CH5~CH8：禁用，无描述
///   - 地址从 0x0010 起，每通道 +2（16 位寄存器宽度）
///   - 线宽 4px，实线，无数据点显示
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
    m_defaultChannels = m_channels;                     ///< 快照默认值，供 resetStyleDefaults 使用

    // --- 将 ScopeBottomPanel 嵌入 bottomPanelHost ---
    // 注意：ScopeBottomPanel 的 plotArea 参数用于其内部浮动面板（overlay window）
    // 的位置计算，确保弹出窗口在波形区上方。
    auto *bottomLayout = qobject_cast<QVBoxLayout *>(m_bottomPanelHost->layout());
    m_bottomPanel = new ScopeBottomPanel(m_plotArea, m_bottomPanelHost);
    bottomLayout->addWidget(m_bottomPanel);
    if (auto *plotAreaLayout = qobject_cast<QVBoxLayout *>(m_plotArea->layout())) {
        plotAreaLayout->setStretch(0, 1);               // plotWidget 占满剩余空间
        plotAreaLayout->setStretch(1, 0);               // bottomPanel 按内容高度
    }

    // --- 样式面板初始状态 ---
    m_stylePanel->setVisible(false);                    // 默认隐藏，通过 TopBar 按钮切换
    m_splitter->setStretchFactor(0, 1);                 // plotArea 优先占空间
    m_splitter->setStretchFactor(1, 0);                 // stylePanel 固定宽度

    // --- 将通道默认配置同步到样式面板控件 ---
    for (int i = 0; i < m_channels.size(); ++i) {
        m_stylePanel->setChannelColor(i, m_channels[i].color);
        m_stylePanel->setChannelLineWidth(i, static_cast<int>(m_channels[i].lineWidth));
        m_stylePanel->setChannelLineStyle(i, m_channels[i].lineStyle);
        m_stylePanel->setChannelShowDataPoints(i, m_channels[i].showDataPoints);
    }

    // --- 初始化采集参数并生成首帧静态数据 ---
    m_plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
    refreshPlotData();
    connectSignals();
    m_topBar->setScopeControlsVisible(true);
    m_topBar->setViewMode(0);                           // 默认 Overlay 模式

    // --- 模拟数据定时器（33ms ≈ 30fps，仅在 running 时启动） ---
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(33);
    connect(m_previewTimer, &QTimer::timeout, this, &ScopePreviewWidget::appendPreviewFrame);
}

ScopePreviewWidget::~ScopePreviewWidget() = default;

// =============================================================================
// 信号槽连接
// =============================================================================

/// @brief 连接所有子面板的信号到本控件的通道状态管理逻辑。
///
/// 信号分组：
///   1. plotWidget  → 采样启停切换
///   2. topBar      → 视图模式（Overlay / Stacked）、样式面板显隐
///   3. bottomPanel → 通道开关、描述、地址、采样率、显示窗口、Y 轴模式
///   4. stylePanel  → 颜色、线宽、线型、数据点显示、恢复默认
void ScopePreviewWidget::connectSignals() {
    // --- 采样启停 ---
    connect(m_plotWidget, &ScopePlotWidget::samplingToggleRequested, this, &ScopePreviewWidget::setRunning);

    // --- 视图模式切换（0=Overlay, 1=Stacked） ---
    connect(m_topBar, &TopBar::viewModeChanged, this, [this](int mode) {
        m_plotWidget->setViewMode(mode == 1 ? ScopeViewMode::Stacked : ScopeViewMode::Overlay);
    });

    // --- 样式面板显隐切换 ---
    connect(m_topBar, &TopBar::styleToggleRequested, this, [this]() {
        m_stylePanel->setVisible(!m_stylePanel->isVisible());
    });

    // --- 通道开关（BottomPanel 通道条的勾选框） ---
    connect(m_bottomPanel, &ScopeBottomPanel::channelToggled, this, [this](int index, bool enabled) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].enabled = enabled;
        refreshPlotData();
    });

    // --- 通道描述文本变更 ---
    connect(m_bottomPanel, &ScopeBottomPanel::channelDescriptionChanged, this, [this](int index, const QString &text) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].description = text.trimmed();
        refreshPlotData();
    });

    // --- 通道寄存器地址变更（仅存储，不触发绘制刷新） ---
    connect(m_bottomPanel, &ScopeBottomPanel::channelAddressChanged, this, [this](int index, const QString &text) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].addressText = text.trimmed();
    });

    // --- 采样间隔变更 → 重新配置采集参数 ---
    connect(m_bottomPanel, &ScopeBottomPanel::sampleIntervalChanged, this, [this](const QString &text) {
        m_sampleIntervalUs = sampleIntervalUsForText(text);
        m_plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
        refreshPlotData();
    });

    // --- 显示窗口变更 → 重新配置采集参数 ---
    connect(m_bottomPanel, &ScopeBottomPanel::displayWindowChanged, this, [this](const QString &text) {
        m_displayWindowMs = displayWindowMsForText(text);
        m_plotWidget->configureAcquisition(m_sampleIntervalUs, m_displayWindowMs);
        refreshPlotData();
    });

    // --- Y 轴模式：自动 / 手动 ---
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisAutoRequested, m_plotWidget, &ScopePlotWidget::setAutoYAxisRange);
    connect(m_bottomPanel, &ScopeBottomPanel::yAxisManualRequested, m_plotWidget, &ScopePlotWidget::setManualYAxisRange);

    // --- 样式面板：颜色变更 ---
    connect(m_stylePanel, &ScopeStylePanel::colorChanged, this, [this](int index, const QColor &color) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].color = color;
        refreshPlotData();
    });

    // --- 样式面板：线宽变更 ---
    connect(m_stylePanel, &ScopeStylePanel::lineWidthChanged, this, [this](int index, int width) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].lineWidth = static_cast<qreal>(width);
        refreshPlotData();
    });

    // --- 样式面板：线型变更 ---
    connect(m_stylePanel, &ScopeStylePanel::lineStyleChanged, this, [this](int index, Qt::PenStyle style) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].lineStyle = style;
        refreshPlotData();
    });

    // --- 样式面板：数据点显示变更 ---
    connect(m_stylePanel, &ScopeStylePanel::dataPointsChanged, this, [this](int index, bool show) {
        if (index < 0 || index >= m_channels.size()) return;
        m_channels[index].showDataPoints = show;
        refreshPlotData();
    });

    // --- 样式面板：恢复默认 ---
    connect(m_stylePanel, &ScopeStylePanel::defaultSettingsRequested, this, &ScopePreviewWidget::resetStyleDefaults);
}

// =============================================================================
// 数据刷新
// =============================================================================

/// @brief 根据当前 m_channels 状态，构建 PlotChannelData 列表推送给 ScopePlotWidget。
///
/// 仅包含 enabled == true 的通道。通道名称格式：
///   - 有描述时："CH1 Speed"
///   - 无描述时："CH1"
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

// =============================================================================
// 采样启停
// =============================================================================

/// @brief 控制模拟采样的运行/停止状态。
///
/// 启动时：清空历史数据、重置视图、重置相位计数器、启动定时器。
/// 停止时：停止定时器（数据保留在画面上供查看）。
///
/// 特殊情况：若 running 与当前状态相同，仍需同步子面板状态
/// （例如外部直接调用 setRunning 确保 UI 一致）。
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
        m_plotWidget->clearData();                      // 清空旧波形数据
        m_plotWidget->resetView();                      // 重置缩放/平移
        m_phase = 0;                                    // 相位归零
        m_previewTimer->start();
    } else {
        m_previewTimer->stop();
    }
}

// =============================================================================
// 模拟数据生成
// =============================================================================

/// @brief 定时器回调：生成一帧模拟波形数据并追加到 plotWidget。
///
/// 波形公式（每通道独立）：
///   t        = phase * 0.08 + channelIndex * 0.55  ← 通道间相位偏移
///   carrier  = sin(t)                               ← 基频载波
///   harmonic = sin(t * (1.6 + index * 0.08))        ← 高次谐波（频率随通道递增）
///   value    = carrier * (300 + index * 55) + harmonic * 90
///
/// 输出为 int16_t 采样值，通过 appendSamples(mask, samples) 推送。
/// mask 中仅包含 enabled 通道的位标志。
void ScopePreviewWidget::appendPreviewFrame() {
    const uint8_t mask = currentChannelMask();
    if (mask == 0) {
        return;                                         // 无启用通道，跳过
    }

    QVector<int16_t> samples;
    samples.reserve(m_channels.size());
    for (int index = 0; index < m_channels.size(); ++index) {
        if ((mask & (1u << index)) == 0) {
            continue;                                   // 跳过未启用通道
        }
        const double t = (m_phase * 0.08) + (index * 0.55);
        const double carrier = qSin(t);
        const double harmonic = qSin(t * (1.6 + index * 0.08));
        const double value = (carrier * (300.0 + index * 55.0)) + (harmonic * 90.0);
        samples.append(static_cast<int16_t>(qRound(value)));
    }
    m_plotWidget->appendSamples(mask, samples);
    m_phase += 1;                                       // 递增相位步进
}

// =============================================================================
// 样式恢复
// =============================================================================

/// @brief 将所有通道的样式属性恢复为构造时的默认值。
///
/// 恢复范围：颜色、线宽、线型、数据点显示。
/// 同时同步更新 ScopeStylePanel 的控件状态，最后刷新绘图数据。
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

// =============================================================================
// UI 构建
// =============================================================================

/// @brief 创建预览控件的完整 UI 层级。
///
/// 布局结构：
///   QVBoxLayout(rootLayout)
///     ├─ TopBar                       (stretch = 0)
///     └─ QSplitter(水平)              (stretch = 1)
///          ├─ plotArea (QWidget)
///          │    ├─ ScopePlotWidget     (stretch = 1)
///          │    └─ bottomPanelHost     (stretch = 0)
///          └─ ScopeStylePanel          (固定宽度 200px)
///
/// 注意：ScopeBottomPanel 在构造函数中创建（需要 plotArea 作为参数），
/// 不在 setupUi 中创建。
void ScopePreviewWidget::setupUi() {
    setObjectName(QStringLiteral("ScopePreviewWidget"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // --- 顶部工具栏 ---
    m_topBar = new TopBar(this);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    rootLayout->addWidget(m_topBar);

    // --- 水平分割器：左侧绘图区 / 右侧样式面板 ---
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("splitter"));
    m_splitter->setChildrenCollapsible(false);          // 禁止拖拽折叠
    m_splitter->setHandleWidth(0);                      // 无可见分割条
    rootLayout->addWidget(m_splitter);
    rootLayout->setStretch(0, 0);                       // topBar 固定高度
    rootLayout->setStretch(1, 1);                       // splitter 填满

    // --- 左侧绘图区容器 ---
    m_plotArea = new QWidget(m_splitter);
    m_plotArea->setObjectName(QStringLiteral("plotArea"));
    auto *plotAreaLayout = new QVBoxLayout(m_plotArea);
    plotAreaLayout->setObjectName(QStringLiteral("plotAreaLayout"));
    plotAreaLayout->setSpacing(0);
    plotAreaLayout->setContentsMargins(0, 0, 0, 0);

    // --- OpenGL 波形绘制控件 ---
    m_plotWidget = new ScopePlotWidget(m_plotArea);
    m_plotWidget->setObjectName(QStringLiteral("plotWidget"));
    plotAreaLayout->addWidget(m_plotWidget);

    // --- 底部面板宿主（ScopeBottomPanel 将在构造函数中动态添加） ---
    m_bottomPanelHost = new QWidget(m_plotArea);
    m_bottomPanelHost->setObjectName(QStringLiteral("bottomPanelHost"));
    auto *bottomPanelHostLayout = new QVBoxLayout(m_bottomPanelHost);
    bottomPanelHostLayout->setObjectName(QStringLiteral("bottomPanelHostLayout"));
    bottomPanelHostLayout->setSpacing(0);
    bottomPanelHostLayout->setContentsMargins(0, 0, 0, 0);
    plotAreaLayout->addWidget(m_bottomPanelHost);

    // --- 右侧样式面板 ---
    m_stylePanel = new ScopeStylePanel(m_splitter);
    m_stylePanel->setObjectName(QStringLiteral("stylePanel"));
}

// =============================================================================
// 辅助转换
// =============================================================================

/// @brief 将采样间隔的显示文本转换为微秒数值。
///
/// 支持的预设值：100/200/300/500/750/1000/1500/2000 us。
/// 无法匹配时返回默认 1000 us。
int ScopePreviewWidget::sampleIntervalUsForText(const QString &text) const {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("100 us")) return 100;
    if (normalized == QStringLiteral("200 us")) return 200;
    if (normalized == QStringLiteral("300 us")) return 300;
    if (normalized == QStringLiteral("500 us")) return 500;
    if (normalized == QStringLiteral("750 us")) return 750;
    if (normalized == QStringLiteral("1500 us")) return 1500;
    if (normalized == QStringLiteral("2000 us")) return 2000;
    return 1000;                                        // 默认 1000 us
}

/// @brief 将显示窗口的显示文本转换为毫秒数值。
///
/// 支持的预设值：50/200/500/1000/2000/4000 ms。
/// 无法匹配时返回默认 50 ms。
int ScopePreviewWidget::displayWindowMsForText(const QString &text) const {
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("50 ms")) return 50;
    if (normalized == QStringLiteral("200 ms")) return 200;
    if (normalized == QStringLiteral("500 ms")) return 500;
    if (normalized == QStringLiteral("1000 ms")) return 1000;
    if (normalized == QStringLiteral("2000 ms")) return 2000;
    if (normalized == QStringLiteral("4000 ms")) return 4000;
    return 50;                                          // 默认 50 ms
}

/// @brief 根据当前通道启用状态生成 8 位通道掩码。
///
/// bit 0 = CH1, bit 1 = CH2, ..., bit 7 = CH8。
/// 用于 appendSamples 接口，与协议中的 channel_mask 格式一致。
uint8_t ScopePreviewWidget::currentChannelMask() const {
    uint8_t mask = 0;
    for (int index = 0; index < m_channels.size(); ++index) {
        if (m_channels.at(index).enabled) {
            mask |= static_cast<uint8_t>(1u << index);
        }
    }
    return mask;
}
