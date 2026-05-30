// =============================================================================
// @file    scopebottompanel.h
// @brief   示波器底部面板 — 通道条、采样配置、寄存器/生成器浮窗、跑马灯
//
// ScopeBottomPanel 是示波器页面底部的控制面板，包含以下区域：
// - 通道配置条：8 通道的启用/描述/地址（可折叠）
// - 采样配置：间隔选择、显示窗口、Y 轴模式
// - 功能浮窗切换：寄存器面板 / 波形生成器面板（覆盖在波形图上方）
// - 跑马灯：滚动显示当前活跃状态
// - 备注输入框
//
// 寄存器面板和生成器面板以覆盖窗口（overlay window）形式显示在 overlayHost 上方，
// 不占据底部面板自身的布局空间。
// =============================================================================
#pragma once

#include "widgets/scopegeneratorpanel.h"

#include <QWidget>

class ScopeChannelStrip;
class QComboBox;
class QEvent;
class QLineEdit;
class QMenu;
class QPushButton;
class ScopeMarqueeLabel;
class ScopeRegisterPanel;
class QToolButton;
class QWidget;

/// @brief 示波器底部控制面板
///
/// 聚合通道条、采样配置、功能浮窗和跑马灯状态栏。
/// 通过信号将用户操作传递给 OscilloscopTab 协调各 Service。
class ScopeBottomPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造底部面板
    /// @param overlayHost 覆盖窗口的宿主控件（通常是 plotArea）
    /// @param parent 父控件
    explicit ScopeBottomPanel(QWidget *overlayHost, QWidget *parent = nullptr);
    ~ScopeBottomPanel() override;

    /// @brief 设置采样运行状态（影响控件可用性）
    void setRunning(bool running);

    /// @brief 获取寄存器辅助面板
    ScopeRegisterPanel *registerPanel() const;

    /// @brief 获取波形生成器面板
    ScopeGeneratorPanel *generatorPanel() const;

    /// @brief 获取跑马灯标签
    ScopeMarqueeLabel *marqueeLabel() const;

signals:
    // --- 寄存器面板信号 ---
    void registerReadRequested(int row);
    void registerWriteRequested(int row);
    void registerStartRequested();
    void registerStopRequested();
    void clearPanelRequested();

    // --- 生成器面板信号 ---
    void generatorLinearStartRequested(quint16 addr, qint16 min, qint16 max, qint16 step, int intervalMs);
    void generatorCosineStartRequested(qint16 amplitude, qint16 offset, double frequencyHz,
                                       const QVector<ScopeGeneratorCosineChannel> &channels);
    void generatorSawtoothStartRequested(quint16 addr, qint16 min, qint16 max, qint16 step);
    void generatorStopRequested();

    // --- 通道配置信号 ---
    void channelToggled(int index, bool enabled);
    void channelDescriptionChanged(int index, const QString &text);
    void channelAddressChanged(int index, const QString &text);

    // --- 采样配置信号 ---
    void sampleIntervalChanged(const QString &text);
    void displayWindowChanged(const QString &text);
    void runningToggled(bool running);
    void captureNoteChanged(const QString &text);

    // --- Y 轴控制信号 ---
    void yAxisAutoRequested();
    void yAxisManualRequested(double minValue, double maxValue);

private:
    /// @brief 事件过滤器 — 监听覆盖窗口外部点击以自动关闭
    bool eventFilter(QObject *watched, QEvent *event) override;

    void setupUi();
    void connectSignals();

    /// @brief 创建覆盖窗口（标题栏 + 内容 + 关闭按钮）
    QWidget *createOverlayWindow(const QString &title, QWidget *content, const QSize &size);

    /// @brief 将覆盖窗口居中放置在 overlayHost（或顶级窗口兜底）上
    void centerOverlayOnHost(QWidget *window);

    /// @brief 刷新覆盖面板的显示/隐藏状态
    void refreshPanels();

    /// @brief 刷新 Y 轴按钮文字
    void refreshYAxisButton();

    /// @brief 弹出手动 Y 轴范围输入对话框
    bool promptManualYAxisRange(double &minValue, double &maxValue);

    // --- 宿主与覆盖窗口 ---
    QWidget *m_overlayHost = nullptr;               ///< 覆盖窗口的宿主控件
    QWidget *m_channelFrame = nullptr;              ///< 通道条容器框架

    // --- 采样配置控件 ---
    QComboBox *m_intervalCombo = nullptr;            ///< 采样间隔下拉框
    QToolButton *m_yAxisButton = nullptr;            ///< Y 轴模式按钮
    QComboBox *m_windowCombo = nullptr;              ///< 显示窗口下拉框
    QLineEdit *m_noteEdit = nullptr;                 ///< 备注输入框

    // --- 通道条 ---
    QWidget *m_channelStripRow = nullptr;            ///< 通道条行容器
    QPushButton *m_channelsToggleButton = nullptr;   ///< 通道条折叠切换按钮
    QPushButton *m_registerToggleButton = nullptr;   ///< 寄存器面板切换按钮
    QPushButton *m_generatorToggleButton = nullptr;  ///< 生成器面板切换按钮

    // --- 子面板 ---
    ScopeMarqueeLabel *m_marqueeLabel = nullptr;     ///< 跑马灯状态标签
    ScopeRegisterPanel *m_registerPanel = nullptr;   ///< 寄存器辅助面板
    ScopeGeneratorPanel *m_generatorPanel = nullptr; ///< 波形生成器面板
    ScopeChannelStrip *m_channels[8] = {};           ///< 8 通道配置条

    // --- 覆盖窗口 ---
    QWidget *m_registerWindow = nullptr;             ///< 寄存器面板覆盖窗口
    QWidget *m_generatorWindow = nullptr;            ///< 生成器面板覆盖窗口
    QMenu *m_yAxisMenu = nullptr;                    ///< Y 轴模式弹出菜单

    // --- 状态 ---
    bool m_channelsVisible = false;                  ///< 通道条是否展开
    bool m_running = false;                          ///< 采样运行状态
    bool m_yAxisAuto = true;                         ///< Y 轴是否自动
    double m_manualYMin = -1000.0;                   ///< 手动 Y 轴最小值
    double m_manualYMax = 1000.0;                    ///< 手动 Y 轴最大值
};
