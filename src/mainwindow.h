// =============================================================================
// @file    mainwindow.h
// @brief   主窗口类声明
//
// MainWindow 是应用程序的顶层窗口，负责：
// - 持有并组合所有顶层 UI 组件（TopBar、ActivityBar、ContentStack、LogPanel）
// - 管理核心服务实例（SerialManager、CommandDispatcher、DeviceContext）
// - 协调页面切换和串口连接状态对各 Tab 的启用/禁用
// - 桥接 TopBar 示波器控件与 OscilloscopTab 之间的信号
//
// 线程安全：MainWindow 及其所有 UI 子组件只在主线程操作。
// =============================================================================

#pragma once

#include <QMainWindow>

class ActivityBar;
class AppConfigService;
class CommandDispatcher;
class ConfigTab;
class DeviceContext;
class Dw9786OisResetService;
class FwFlashTab;
class FlashStorageTab;
class LogPanel;
class OscilloscopTab;
class QEvent;
class QLabel;
class QStackedWidget;
class QPushButton;
class RegisterRwTab;
class SerialDebugTab;
class SerialManager;
class TopBar;
class QWidget;
class QTranslator;

/// @brief 应用程序主窗口
///
/// 采用 TopBar + ActivityBar + ContentStack + LogPanel + StatusBar 的经典布局。
/// 串口连接成功后启用寄存器/烧录/示波器页面；断开后禁用相关页面。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// @brief 构造主窗口，初始化所有服务和 UI 组件
    /// @param parent  父控件，默认为 nullptr（顶层窗口）
    explicit MainWindow(QWidget *parent = nullptr);

    /// @brief 析构函数，释放 SerialManager（需在子线程清理前手动删除）
    ~MainWindow() override;

protected:
    /// @brief 语言切换（QEvent::LanguageChange）时刷新主窗口自身可见文字（状态栏等）
    void changeEvent(QEvent *event) override;

private:
    /// @brief 构建 UI 布局和所有子控件
    void setupUi();

    /// @brief 重设主窗口自身可见文字（窗口标题、状态栏固件标签、日志切换按钮）
    void retranslateUi();

    /// @brief 连接所有信号槽（页面切换、串口状态、日志面板等）
    void connectSignals();

    /// @brief 切换界面语言（0=中文，1=English）：装/卸 QTranslator 触发即时刷新 + QSettings 记忆
    void applyLanguage(int index);

    // --- 核心服务 ---
    SerialManager *m_serialManager = nullptr;       ///< 串口管理器（独立线程运行）
    CommandDispatcher *m_dispatcher = nullptr;       ///< 命令分发器（命令队列 + 超时管理）
    DeviceContext *m_deviceContext = nullptr;        ///< 设备上下文（IC 类型 + 从机地址）
    Dw9786OisResetService *m_oisResetService = nullptr;  ///< DW9786 上电 OISReset 服务
    AppConfigService *m_appConfigService = nullptr;  ///< 统一配置文件存取服务（各页面功能参数）
    QTranslator *m_enTranslator = nullptr;           ///< English 翻译器（中文为源语言，不装则显示中文）

    // --- 顶层 UI 组件 ---
    TopBar *m_topBar = nullptr;                     ///< 顶栏（Logo、连接状态、示波器控件）
    ActivityBar *m_activityBar = nullptr;            ///< 左侧活动栏（页面切换按钮）
    QStackedWidget *m_contentStack = nullptr;        ///< 内容区域堆叠容器（各 Tab 页面）
    LogPanel *m_logPanel = nullptr;                  ///< 底部日志面板
    QLabel *m_firmwareLabel = nullptr;               ///< 状态栏固件版本标签（"固件 v…"）
    QPushButton *m_logToggleButton = nullptr;        ///< 状态栏上的日志面板展开/收起按钮

    // --- Tab 页面 ---
    ConfigTab *m_configTab = nullptr;                ///< 配置 Tab（串口/IC/PMIC）
    RegisterRwTab *m_registerTab = nullptr;          ///< 寄存器读写 Tab
    FwFlashTab *m_fwFlashTab = nullptr;              ///< 固件烧录 Tab（AW IC ISP）
    OscilloscopTab *m_scopeTab = nullptr;            ///< 示波器 Tab
    FlashStorageTab *m_flashStorageTab = nullptr;    ///< STM32 自身 FLASH 文件存储 Tab（协议 v2.10）
    SerialDebugTab *m_debugTab = nullptr;            ///< 串口调试模拟器（浮动窗口）

    /// @brief 是否已对当前会话中的 INIT_FAIL_* 弹过提示框（防重弹；串口断开重置）
    bool m_mcuFailureNotified = false;
};
