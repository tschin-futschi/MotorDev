// =============================================================================
// @file    oscilloscoptab.h
// @brief   示波器页面 — 实时波形观测、通道配置、寄存器辅助读写、波形生成器
//
// OscilloscopTab 是应用主界面的"示波器"标签页，整合以下功能：
// - 实时波形绘制：最多 8 通道，支持 Overlay/Stacked 两种视图模式
// - 采样控制：启动/停止采样，配置采样间隔和显示窗口
// - 通道配置：启用/禁用通道、设置描述、地址映射
// - 样式面板：通道颜色、线宽、线型、数据点显示
// - 寄存器辅助面板：单行/循环读写寄存器
// - 波形生成器面板：线性/余弦波形生成（STM32 侧执行）
// - 全屏模式：双击切换波形图全屏显示
//
// 布局结构：
// ┌─────────────────────────────────────┬─────────────┐
// │ ScopePlotWidget（波形图）           │ StylePanel  │
// │                                     │ （可折叠）  │
// ├─────────────────────────────────────┤             │
// │ ScopeBottomPanel                    │             │
// │ (通道条|采样配置|寄存器|生成器)      │             │
// └─────────────────────────────────────┴─────────────┘
//
// 数据流：SerialManager → ScopeService → ScopePlotWidget
// 控制流：BottomPanel → OscilloscopTab → ScopeService / RegisterService
// =============================================================================
#pragma once

#include "ui/scopeviewmode.h"

#include <QKeyEvent>
#include <QString>
#include <QStringList>
#include <QWidget>

class CyclicWriteService;
class CommandDispatcher;
class GeneratorService;
class ScopeBottomPanel;
class ScopeChannelModel;
class ScopePlotWidget;
class RegisterService;
class ScopeService;
class ScopeStylePanel;
class SerialManager;
class QTimer;
class QSplitter;
class QVBoxLayout;

/// @brief 示波器页面 Tab — 实时波形观测与多功能控制
///
/// 协调 ScopeService（采样控制）、RegisterService（寄存器辅助）、
/// GeneratorService（波形生成）、CyclicWriteService（循环写入）四个服务，
/// 驱动 ScopePlotWidget（波形绘制）和 ScopeBottomPanel（底部控制面板）。
class OscilloscopTab : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造示波器页面
    /// @param serialManager 串口管理器（传给 ScopeService）
    /// @param dispatcher 命令分发器（传给各 Service）
    /// @param parent 父控件指针
    explicit OscilloscopTab(SerialManager *serialManager,
                            CommandDispatcher *dispatcher,
                            QWidget *parent = nullptr);
    ~OscilloscopTab() override;

signals:
    /// @brief 采样运行状态变化
    /// @param running true=采样中，false=已停止
    void runningChanged(bool running);

    /// @brief 视图模式变化（Overlay=0, Stacked=1）
    /// @param mode 模式索引
    void viewModeChanged(int mode);

public slots:
    /// @brief 设置调试流是否活跃（来自 SerialDebugTab 的模拟器信号）
    /// @param active true=模拟器正在产生流数据
    void setDebugStreamActive(bool active);

    /// @brief 注入调试流数据（来自 SerialDebugTab 的模拟器数据）
    /// @param channelMask 通道掩码
    /// @param samples 采样数据
    void ingestDebugStream(uint8_t channelMask, const QVector<int16_t> &samples);

    /// @brief 切换视图模式（由 TopBar 的 Overlay/Stacked 按钮触发）
    /// @param mode 0=Overlay, 1=Stacked
    void onViewModeChangeRequested(int mode);

    /// @brief 采样启停请求（由 PlotWidget 或 BottomPanel 触发）
    /// @param running true=启动采样，false=停止采样
    void onSamplingToggleRequested(bool running);

    /// @brief 样式面板折叠/展开切换（由 TopBar 触发）
    void onStyleToggleRequested();

protected:
    /// @brief 键盘事件处理 — Escape 退出全屏
    void keyPressEvent(QKeyEvent *event) override;

private:
    /// @brief 构建 UI 布局
    void setupUi();

    /// @brief 连接所有信号槽
    void connectSignals();

    /// @brief 从 ScopeChannelModel 读取通道配置，同步到 PlotWidget
    void refreshPlotData();

    /// @brief 切换样式面板显示/隐藏
    void toggleStylePanel();

    /// @brief 切换波形图全屏/普通模式
    void togglePlotFullscreen();

    /// @brief 退出全屏模式，将 PlotWidget 放回原布局
    void exitFullscreen();

    /// @brief 性能计时器回调（每秒打印绘制耗时、FPS、采样率等）
    void onPerfTimerTick();

    /// @brief 寄存器面板单行读取请求
    /// @param row 行索引
    void onRegisterReadRequested(int row);

    /// @brief 寄存器面板单行写入请求
    /// @param row 行索引
    void onRegisterWriteRequested(int row);

    /// @brief 启动循环写入
    void onRegisterStartRequested();

    /// @brief 停止循环写入
    void onRegisterStopRequested();

    /// @brief 清除寄存器面板所有数据并停止循环写入
    void onRegisterClearRequested();

    /// @brief 刷新底部跑马灯状态文字（采样 + 循环写入 + 生成器）
    void refreshMarqueeStatus();

    /// @brief int → ScopeViewMode 转换
    static ScopeViewMode viewModeFromInt(int mode);

    /// @brief ScopeViewMode → int 转换
    static int viewModeToInt(ScopeViewMode mode);

    // --- 核心依赖 ---
    SerialManager *m_serialManager = nullptr;           ///< 串口管理器
    CommandDispatcher *m_dispatcher = nullptr;           ///< 命令分发器
    ScopeChannelModel *m_channelModel = nullptr;        ///< 8 通道配置模型
    ScopeService *m_service = nullptr;                  ///< 示波器采样服务
    RegisterService *m_regService = nullptr;            ///< 寄存器读写服务（辅助面板用）
    GeneratorService *m_generatorService = nullptr;     ///< 波形生成器服务
    CyclicWriteService *m_cyclicWriteService = nullptr; ///< 循环写入服务

    // --- UI 控件 ---
    QSplitter *m_splitter = nullptr;                    ///< 水平分割器（波形区 | 样式面板）
    QWidget *m_plotArea = nullptr;                      ///< 波形区容器（PlotWidget + BottomPanel）
    QVBoxLayout *m_plotLayout = nullptr;                ///< 波形区垂直布局
    ScopePlotWidget *m_plotWidget = nullptr;            ///< 波形绘制控件
    QWidget *m_bottomPanelHost = nullptr;               ///< 底部面板宿主容器
    ScopeStylePanel *m_stylePanel = nullptr;            ///< 样式配置面板（可折叠）
    ScopeBottomPanel *m_bottomPanel = nullptr;          ///< 底部控制面板

    // --- 状态 ---
    ScopeViewMode m_viewMode = ScopeViewMode::Overlay;  ///< 当前视图模式
    QTimer *m_perfTimer = nullptr;                      ///< 性能统计定时器（1秒间隔）
    QWidget *m_fullscreenWindow = nullptr;              ///< 全屏窗口（null=非全屏）
    int m_plotLayoutIndex = -1;                         ///< PlotWidget 在布局中的原始索引（全屏恢复用）
    bool m_scopeRunning = false;                        ///< 采样运行状态
    QString m_sampleIntervalText = QStringLiteral("1000 us"); ///< 采样间隔显示文本

    // --- 采样配置 ---
    uint8_t m_sampleIntervalIndex = 0x04;               ///< 采样间隔索引（协议值）
    int m_displayWindowMs = 50;                          ///< 显示窗口宽度（毫秒）
};
