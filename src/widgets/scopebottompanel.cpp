// =============================================================================
// @file    scopebottompanel.cpp
// @brief   示波器底部面板实现 — 通道条、采样配置、覆盖窗口管理
//
// 底部面板布局：
//   ┌─ channelFrame（可折叠）─────────────────────────┐
//   │ [Interval▼] [Y Axis▼] [Window▼] [Note 输入框]  │
//   │ [CH1] [CH2] [CH3] [CH4] [CH5] [CH6] [CH7] [CH8]│
//   └─────────────────────────────────────────────────┘
//   ---弹簧--- [Hide Channels] [Show Register] [Show Generator] [Marquee]
//
// 覆盖窗口（Register / Generator）：
//   - 作为独立 Qt::Tool 顶级窗口，悬浮于主窗口之上
//   - 通过 eventFilter 限制在屏幕可用区域内
//   - 面板本身不持有业务逻辑，仅转发信号到 OscilloscpTab
// =============================================================================
#include "widgets/scopebottompanel.h"

#include "protocol/sampling_config.h"
#include "ui/style_constants.h"
#include "widgets/scopechannelstrip.h"
#include "widgets/scopegeneratorpanel.h"
#include "widgets/scopemarqueelabel.h"
#include "widgets/scoperegisterpanel.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造底部面板，创建 8 个通道条和两个覆盖窗口。
///
/// @param overlayHost  覆盖窗口居中对齐的参考控件（通常为绘图区域）
/// @param parent       父控件
ScopeBottomPanel::ScopeBottomPanel(QWidget *overlayHost, QWidget *parent)
    : QWidget(parent)
    , m_overlayHost(overlayHost) {
    setupUi();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_channelFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // Note 输入框占据剩余空间
    if (auto *channelHeaderLayout = qobject_cast<QHBoxLayout *>(m_channelFrame->layout()->itemAt(0)->layout())) {
        channelHeaderLayout->setStretch(channelHeaderLayout->indexOf(m_noteEdit), 1);
    }

    // ---------- 创建 8 个通道条 ----------
    auto *stripRowLayout = qobject_cast<QHBoxLayout *>(m_channelStripRow->layout());
    for (int i = 0; i < 8; ++i) {
        m_channels[i] = new ScopeChannelStrip(i, m_channelStripRow);
        stripRowLayout->addWidget(m_channels[i], 1);
    }

    // ---------- 创建覆盖窗口（Qt::Tool 顶级窗口） ----------
    // 面板创建时不指定 parent，所有权通过 layout→addWidget 转移到覆盖窗口。
    // 覆盖窗口在析构函数中手动 delete。
    m_registerPanel = new ScopeRegisterPanel(nullptr);
    m_generatorPanel = new ScopeGeneratorPanel(nullptr);
    m_registerWindow = createOverlayWindow(tr("Register R/W"), m_registerPanel, QSize(500, 400));
    m_generatorWindow = createOverlayWindow(tr("Wave Generator"), m_generatorPanel, QSize(420, 340));

    // ---------- Y 轴菜单：Auto / Manual ----------
    m_yAxisMenu = new QMenu(m_yAxisButton);
    m_yAxisMenu->addAction(tr("Auto"));
    m_yAxisMenu->addAction(tr("Manual..."));
    m_yAxisButton->setMenu(m_yAxisMenu);

    // 默认采样间隔 = 1000 us（索引 4），显示窗口 = 50 ms（索引 0）
    m_intervalCombo->setCurrentIndex(4);
    m_windowCombo->setCurrentIndex(0);

    connectSignals();
    refreshPanels();
    refreshYAxisButton();
}

/// @brief 析构：手动销毁覆盖窗口（它们是无 parent 的顶级窗口）。
ScopeBottomPanel::~ScopeBottomPanel() {
    delete m_registerWindow;
    m_registerWindow = nullptr;
    delete m_generatorWindow;
    m_generatorWindow = nullptr;
}

// =============================================================================
// 公共访问接口
// =============================================================================

/// @brief 返回寄存器面板指针（供外部连接信号用）。
ScopeRegisterPanel *ScopeBottomPanel::registerPanel() const {
    return m_registerPanel;
}

/// @brief 返回波形生成器面板指针。
ScopeGeneratorPanel *ScopeBottomPanel::generatorPanel() const {
    return m_generatorPanel;
}

/// @brief 返回跑马灯标签指针（供外部更新状态文本）。
ScopeMarqueeLabel *ScopeBottomPanel::marqueeLabel() const {
    return m_marqueeLabel;
}

// =============================================================================
// 采样状态控制
// =============================================================================

/// @brief 设置采样运行状态：运行时禁用配置控件。
///
/// 禁用项目：采样间隔、显示窗口、所有通道条。
void ScopeBottomPanel::setRunning(bool running) {
    if (m_running == running) {
        return;
    }

    m_running = running;
    const bool configEditable = !running;
    m_intervalCombo->setEnabled(configEditable);
    m_windowCombo->setEnabled(configEditable);
    for (ScopeChannelStrip *channel : m_channels) {
        if (channel != nullptr) {
            channel->setEnabled(configEditable);
        }
    }
}

// =============================================================================
// 事件过滤器 — 覆盖窗口位置约束
// =============================================================================

/// @brief 监听覆盖窗口的 Move/Show/Hide 事件。
///
/// - Move/Show: 将窗口位置限制在屏幕可用区域内（防止拖出屏幕）
/// - Show/Hide/Close: 刷新面板切换按钮文本
bool ScopeBottomPanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_registerWindow || watched == m_generatorWindow) {
        // 将窗口位置钳制到屏幕可用区域内
        auto clampToScreen = [this](QWidget *window) {
            if (window == nullptr) {
                return;
            }

            QScreen *screen = window->screen();
            if (screen == nullptr && m_overlayHost != nullptr && m_overlayHost->windowHandle() != nullptr) {
                screen = m_overlayHost->windowHandle()->screen();
            }
            if (screen == nullptr) {
                screen = QGuiApplication::primaryScreen();
            }
            if (screen == nullptr) {
                return;
            }

            const QRect available = screen->availableGeometry();
            const int maxX = qMax(available.left(), available.right() - window->width() + 1);
            const int maxY = qMax(available.top(), available.bottom() - window->height() + 1);
            const int x = qBound(available.left(), window->x(), maxX);
            const int y = qBound(available.top(), window->y(), maxY);
            if (x != window->x() || y != window->y()) {
                window->move(x, y);
            }
        };

        if (event->type() == QEvent::Move || event->type() == QEvent::Show) {
            clampToScreen(static_cast<QWidget *>(watched));
        }

        if (event->type() == QEvent::Show || event->type() == QEvent::Hide || event->type() == QEvent::Close) {
            refreshPanels();
        }
    }

    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// 覆盖窗口创建
// =============================================================================

/// @brief 创建一个 Qt::Tool 顶级覆盖窗口，内部嵌入指定的内容控件。
///
/// @param title    窗口标题
/// @param content  内容控件（所有权转移到窗口）
/// @param size     初始窗口大小
/// @return 新创建的覆盖窗口指针
QWidget *ScopeBottomPanel::createOverlayWindow(const QString &title, QWidget *content, const QSize &size) {
    auto *window = new QWidget(nullptr, Qt::Tool);
    window->setWindowTitle(title);
    window->setProperty("scopeOverlayWindow", true);    // QSS 选择器标记
    window->setAttribute(Qt::WA_DeleteOnClose, false);   // 手动管理生命周期
    window->hide();
    window->installEventFilter(this);

    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addWidget(content);

    window->resize(size);
    // 标记尚未定位：首次 Show 时才计算居中位置（构造期 overlayHost 还未显示，
    // mapToGlobal 拿不到正确坐标，会落到屏幕左上角）。用户后续拖动后保留位置。
    window->setProperty("scopeOverlayPositioned", false);
    return window;
}

// =============================================================================
// 覆盖窗口居中
// =============================================================================

/// @brief 将覆盖窗口居中放置在 overlayHost 上（host 不可用时回退到顶级窗口）。
void ScopeBottomPanel::centerOverlayOnHost(QWidget *window) {
    if (window == nullptr) {
        return;
    }

    QWidget *host = m_overlayHost;
    if (host == nullptr || !host->isVisible()) {
        host = this->window();  // 顶级窗口兜底
    }
    if (host == nullptr) {
        return;
    }

    const QPoint center = host->mapToGlobal(host->rect().center());
    const int w = qMax(window->width(), window->sizeHint().width());
    const int h = qMax(window->height(), window->sizeHint().height());
    window->move(center.x() - w / 2, center.y() - h / 2);
}

// =============================================================================
// 面板状态刷新
// =============================================================================

/// @brief 刷新通道区域可见性和切换按钮文本。
void ScopeBottomPanel::refreshPanels() {
    // 通道区域折叠/展开
    m_channelFrame->setVisible(m_channelsVisible);
    setMinimumHeight(m_channelsVisible ? Style::Size::ScopeBottomPanelMinExpanded
                                       : Style::Size::ScopeBottomPanelMinCollapsed);
    setMaximumHeight(m_channelsVisible ? Style::Size::ScopeBottomPanelMaxExpanded
                                       : Style::Size::ScopeBottomPanelMinCollapsed);

    // 同步按钮文本
    m_channelsToggleButton->setText(m_channelsVisible ? tr("Hide Channels") : tr("Show Channels"));
    m_registerToggleButton->setText(m_registerWindow->isVisible() ? tr("Hide Register") : tr("Show Register"));
    m_generatorToggleButton->setText(m_generatorWindow->isVisible() ? tr("Hide Generator") : tr("Show Generator"));

    updateGeometry();
}

/// @brief 刷新 Y 轴按钮文本（Auto 或手动范围）。
void ScopeBottomPanel::refreshYAxisButton() {
    if (m_yAxisAuto) {
        m_yAxisButton->setText(tr("Y Axis: Auto"));
        return;
    }

    m_yAxisButton->setText(tr("Y Axis: %1 ~ %2")
                                 .arg(QString::number(m_manualYMin, 'f', 1))
                                 .arg(QString::number(m_manualYMax, 'f', 1)));
}

// =============================================================================
// Y 轴手动范围输入对话框
// =============================================================================

/// @brief 弹出模态对话框让用户输入 Y 轴最小/最大值。
///
/// @param[out] minValue  用户输入的最小值
/// @param[out] maxValue  用户输入的最大值
/// @return true=用户确认且输入有效，false=取消或无效
bool ScopeBottomPanel::promptManualYAxisRange(double &minValue, double &maxValue) {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("设置 Y Axis 范围"));
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(8);

    auto *minEdit = new QLineEdit(QString::number(m_manualYMin, 'f', 1), &dialog);
    auto *maxEdit = new QLineEdit(QString::number(m_manualYMax, 'f', 1), &dialog);
    minEdit->setPlaceholderText(tr("例如 -1000"));
    maxEdit->setPlaceholderText(tr("例如 1000"));
    formLayout->addRow(tr("最小值"), minEdit);
    formLayout->addRow(tr("最大值"), maxEdit);
    layout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    bool minOk = false;
    bool maxOk = false;
    const double parsedMin = minEdit->text().trimmed().toDouble(&minOk);
    const double parsedMax = maxEdit->text().trimmed().toDouble(&maxOk);
    if (!minOk || !maxOk || parsedMin >= parsedMax) {
        return false;
    }

    minValue = parsedMin;
    maxValue = parsedMax;
    return true;
}

// =============================================================================
// 信号槽连接
// =============================================================================

/// @brief 连接所有子控件信号到外部接口。
///
/// 信号分组：
///   1. 通道条 → channelToggled / channelDescriptionChanged / channelAddressChanged
///   2. 寄存器面板 → registerReadRequested / registerWriteRequested 等
///   3. 生成器面板 → generatorLinearStartRequested / generatorCosineStartRequested / generatorStopRequested
///   4. 采样配置 → sampleIntervalChanged / displayWindowChanged
///   5. Y 轴菜单 → yAxisAutoRequested / yAxisManualRequested
///   6. 面板切换按钮 → 控制折叠/覆盖窗口显隐
void ScopeBottomPanel::connectSignals() {
    // --- 8 个通道条信号转发 ---
    for (ScopeChannelStrip *channel : m_channels) {
        connect(channel, &ScopeChannelStrip::channelToggled, this, &ScopeBottomPanel::channelToggled);
        connect(channel, &ScopeChannelStrip::descriptionChanged, this, &ScopeBottomPanel::channelDescriptionChanged);
        connect(channel, &ScopeChannelStrip::addressChanged, this, &ScopeBottomPanel::channelAddressChanged);
    }

    // --- 寄存器面板信号转发 ---
    connect(m_registerPanel, &ScopeRegisterPanel::readRequested, this, &ScopeBottomPanel::registerReadRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::writeRequested, this, &ScopeBottomPanel::registerWriteRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::startRequested, this, &ScopeBottomPanel::registerStartRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::stopRequested, this, &ScopeBottomPanel::registerStopRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::clearPanelRequested, this, &ScopeBottomPanel::clearPanelRequested);
    connect(m_registerPanel, &ScopeRegisterPanel::loadParamsRequested, this, &ScopeBottomPanel::loadParamsRequested);

    // --- 生成器面板信号转发 ---
    connect(m_generatorPanel, &ScopeGeneratorPanel::linearStartRequested, this, &ScopeBottomPanel::generatorLinearStartRequested);
    connect(m_generatorPanel, &ScopeGeneratorPanel::cosineStartRequested, this, &ScopeBottomPanel::generatorCosineStartRequested);
    connect(m_generatorPanel, &ScopeGeneratorPanel::sawtoothStartRequested, this, &ScopeBottomPanel::generatorSawtoothStartRequested);
    connect(m_generatorPanel, &ScopeGeneratorPanel::stopRequested, this, &ScopeBottomPanel::generatorStopRequested);

    // --- 采样配置变更 ---
    connect(m_intervalCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::sampleIntervalChanged);
    connect(m_windowCombo, &QComboBox::currentTextChanged, this, &ScopeBottomPanel::displayWindowChanged);
    connect(m_noteEdit, &QLineEdit::textChanged, this, &ScopeBottomPanel::captureNoteChanged);

    // --- Y 轴菜单 ---
    connect(m_yAxisMenu, &QMenu::triggered, this, [this](QAction *action) {
        if (action == nullptr) {
            return;
        }

        if (action->text() == tr("Auto")) {
            m_yAxisAuto = true;
            refreshYAxisButton();
            emit yAxisAutoRequested();
            return;
        }

        // Manual: 弹出输入对话框
        double minValue = m_manualYMin;
        double maxValue = m_manualYMax;
        if (!promptManualYAxisRange(minValue, maxValue)) {
            return;
        }

        m_yAxisAuto = false;
        m_manualYMin = minValue;
        m_manualYMax = maxValue;
        refreshYAxisButton();
        emit yAxisManualRequested(minValue, maxValue);
    });

    // --- 面板切换按钮 ---
    connect(m_channelsToggleButton, &QPushButton::clicked, this, [this]() {
        m_channelsVisible = !m_channelsVisible;
        refreshPanels();
    });
    connect(m_registerToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_registerWindow->isVisible();
        if (visible && !m_registerWindow->property("scopeOverlayPositioned").toBool()) {
            centerOverlayOnHost(m_registerWindow);
            m_registerWindow->setProperty("scopeOverlayPositioned", true);
        }
        m_registerWindow->setVisible(visible);
        if (visible) {
            m_registerWindow->raise();
            m_registerWindow->activateWindow();
        }
        refreshPanels();
    });
    connect(m_generatorToggleButton, &QPushButton::clicked, this, [this]() {
        const bool visible = !m_generatorWindow->isVisible();
        if (visible && !m_generatorWindow->property("scopeOverlayPositioned").toBool()) {
            centerOverlayOnHost(m_generatorWindow);
            m_generatorWindow->setProperty("scopeOverlayPositioned", true);
        }
        m_generatorWindow->setVisible(visible);
        if (visible) {
            m_generatorWindow->raise();
            m_generatorWindow->activateWindow();
        }
        refreshPanels();
    });
}

// =============================================================================
// UI 构建
// =============================================================================

/// @brief 创建底部面板的完整 UI 结构。
///
/// 结构：rootLayout(VBox)
///   ├─ channelFrame(可折叠)
///   │   ├─ channelHeaderLayout: Interval/YAxis/Window/Note
///   │   └─ channelStripRow: 8 个通道条的占位容器
///   └─ buttonLayout: 切换按钮 + 跑马灯
void ScopeBottomPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeBottomPanel"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(6);
    rootLayout->setContentsMargins(8, 6, 8, 6);

    // ========== 通道配置区域（可折叠） ==========
    m_channelFrame = new QWidget(this);
    m_channelFrame->setObjectName(QStringLiteral("channelFrame"));
    auto *channelLayout = new QVBoxLayout(m_channelFrame);
    channelLayout->setObjectName(QStringLiteral("channelLayout"));
    channelLayout->setSpacing(6);
    channelLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->addWidget(m_channelFrame);

    // --- 通道配置头部行 ---
    auto *channelHeaderLayout = new QHBoxLayout();
    channelHeaderLayout->setObjectName(QStringLiteral("channelHeaderLayout"));
    channelHeaderLayout->setSpacing(6);
    channelLayout->addLayout(channelHeaderLayout);

    // 采样间隔选择
    auto *intervalLabel = new QLabel(m_channelFrame);
    intervalLabel->setObjectName(QStringLiteral("intervalLabel"));
    intervalLabel->setText(QStringLiteral("Sample Interval"));
    channelHeaderLayout->addWidget(intervalLabel);

    m_intervalCombo = new QComboBox(m_channelFrame);
    m_intervalCombo->setObjectName(QStringLiteral("intervalCombo"));
    m_intervalCombo->setMinimumSize(QSize(Style::Size::ScopeIntervalComboMinW, 0));
    m_intervalCombo->addItems(SamplingConfig::intervalLabels());
    channelHeaderLayout->addWidget(m_intervalCombo);

    // Y 轴模式按钮（弹出菜单）
    m_yAxisButton = new QToolButton(m_channelFrame);
    m_yAxisButton->setObjectName(QStringLiteral("yAxisButton"));
    m_yAxisButton->setMinimumSize(QSize(Style::Size::ScopeYAxisButtonMinW, 0));
    m_yAxisButton->setPopupMode(QToolButton::InstantPopup);
    m_yAxisButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_yAxisButton->setText(QStringLiteral("Y Axis: Auto"));
    channelHeaderLayout->addWidget(m_yAxisButton);

    // 显示窗口时长选择
    auto *windowLabel = new QLabel(m_channelFrame);
    windowLabel->setObjectName(QStringLiteral("windowLabel"));
    windowLabel->setText(QStringLiteral("Display Window"));
    channelHeaderLayout->addWidget(windowLabel);

    m_windowCombo = new QComboBox(m_channelFrame);
    m_windowCombo->setObjectName(QStringLiteral("windowCombo"));
    m_windowCombo->setMinimumSize(QSize(110, 0));
    m_windowCombo->addItems({QStringLiteral("50 ms"), QStringLiteral("200 ms"), QStringLiteral("500 ms"),
                             QStringLiteral("1000 ms"), QStringLiteral("2000 ms"), QStringLiteral("4000 ms")});
    channelHeaderLayout->addWidget(m_windowCombo);

    // 采集备注输入
    auto *noteLabel = new QLabel(m_channelFrame);
    noteLabel->setObjectName(QStringLiteral("noteLabel"));
    noteLabel->setText(QStringLiteral("Capture Note"));
    channelHeaderLayout->addWidget(noteLabel);

    m_noteEdit = new QLineEdit(m_channelFrame);
    m_noteEdit->setObjectName(QStringLiteral("noteEdit"));
    m_noteEdit->setMinimumSize(QSize(220, 0));
    m_noteEdit->setPlaceholderText(QStringLiteral("Add capture note"));
    channelHeaderLayout->addWidget(m_noteEdit);

    // --- 通道条行（8 个 ScopeChannelStrip 的占位容器） ---
    m_channelStripRow = new QWidget(m_channelFrame);
    m_channelStripRow->setObjectName(QStringLiteral("channelStripRow"));
    auto *channelStripRowLayout = new QHBoxLayout(m_channelStripRow);
    channelStripRowLayout->setObjectName(QStringLiteral("channelStripRowLayout"));
    channelStripRowLayout->setSpacing(4);
    channelStripRowLayout->setContentsMargins(0, 0, 0, 0);
    channelLayout->addWidget(m_channelStripRow);

    // ========== 底部按钮栏 ==========
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setObjectName(QStringLiteral("buttonLayout"));
    buttonLayout->setSpacing(6);
    rootLayout->addLayout(buttonLayout);
    buttonLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // 通道区域折叠按钮
    m_channelsToggleButton = new QPushButton(this);
    m_channelsToggleButton->setObjectName(QStringLiteral("channelsToggleButton"));
    m_channelsToggleButton->setProperty("buttonRole", QStringLiteral("toggle"));
    m_channelsToggleButton->setText(QStringLiteral("Hide Channels"));
    buttonLayout->addWidget(m_channelsToggleButton);

    // 寄存器面板切换按钮
    m_registerToggleButton = new QPushButton(this);
    m_registerToggleButton->setObjectName(QStringLiteral("registerToggleButton"));
    m_registerToggleButton->setProperty("buttonRole", QStringLiteral("toggle"));
    m_registerToggleButton->setText(QStringLiteral("Show Register"));
    buttonLayout->addWidget(m_registerToggleButton);

    // 生成器面板切换按钮
    m_generatorToggleButton = new QPushButton(this);
    m_generatorToggleButton->setObjectName(QStringLiteral("generatorToggleButton"));
    m_generatorToggleButton->setProperty("buttonRole", QStringLiteral("toggle"));
    m_generatorToggleButton->setText(QStringLiteral("Show Generator"));
    buttonLayout->addWidget(m_generatorToggleButton);

    // 跑马灯状态标签（占满剩余空间）
    m_marqueeLabel = new ScopeMarqueeLabel(this);
    m_marqueeLabel->setObjectName(QStringLiteral("marqueeLabel"));
    m_marqueeLabel->setText(QStringLiteral("Idle"));
    buttonLayout->addWidget(m_marqueeLabel, 1);
}
