// =============================================================================
// @file    topbar.h
// @brief   顶部栏 — 应用标题、串口状态、视图模式切换、语言选择
//
// TopBar 位于主窗口顶部，固定高度 36px，展示以下内容：
// - 左侧：Logo + "MotorDev" 标题 + 串口状态（端口/波特率 + 连接指示灯）
// - 中间：示波器视图模式切换（Overlay/Stacked）+ 样式面板开关
// - 右侧：语言选择下拉框
//
// 示波器相关控件仅在示波器页面可见（通过 setScopeControlsVisible 控制）。
// =============================================================================
#pragma once

#include <QtTypes>
#include <QWidget>
class QComboBox;
class QFrame;
class QLabel;
class QToolButton;
class QSvgWidget;

/// @brief 顶部栏控件 — 应用标题栏 + 全局状态展示
class TopBar : public QWidget {
    Q_OBJECT

public:
    explicit TopBar(QWidget *parent = nullptr);
    ~TopBar() override;

signals:
    /// @brief 视图模式切换（0=Overlay, 1=Stacked）
    void viewModeChanged(int mode);

    /// @brief 样式面板折叠/展开请求
    void styleToggleRequested();

    /// @brief 十字光标启用/禁用请求
    /// @param enabled true=启用十字光标，false=禁用
    void crosshairToggleRequested(bool enabled);

public slots:
    /// @brief 串口连接成功 → 更新端口/波特率文字和指示灯
    void onSerialConnected(const QString &port, qint32 baudRate);

    /// @brief 串口断开 → 恢复默认文字和指示灯
    void onSerialDisconnected();

    /// @brief 设置示波器相关控件的可见性
    /// @param visible true=显示 Overlay/Style 按钮
    void setScopeControlsVisible(bool visible);

    /// @brief 设置当前视图模式（同步按钮文字，不发信号）
    /// @param mode 0=Overlay, 1=Stacked
    void setViewMode(int mode);

    /// @brief 设置十字光标按钮状态（同步按钮文字与按下态，不发信号）
    /// @param enabled true=开启，false=关闭
    void setCrosshairEnabled(bool enabled);

private:
    /// @brief 构建 UI 布局
    void setupUi();

    /// @brief 连接信号槽
    void connectSignals();

    QSvgWidget *m_logo = nullptr;               ///< SVG Logo（22x22）
    QLabel *m_portValueLabel = nullptr;         ///< 串口/波特率文字（如 "COM3 / 115200"）
    QLabel *m_connectionIndicator = nullptr;    ///< 连接状态圆点（7x7，QSS 驱动颜色）
    QLabel *m_connectionLabel = nullptr;        ///< 连接状态文字（"已连接"/"未连接"）
    QToolButton *m_viewModeButton = nullptr;    ///< Overlay/Stacked 切换按钮
    QToolButton *m_styleButton = nullptr;       ///< 样式面板开关按钮
    QToolButton *m_crosshairButton = nullptr;   ///< 十字光标启用/禁用按钮
    QComboBox *m_languageCombo = nullptr;       ///< 语言选择下拉框
    int m_viewMode = 0;                         ///< 当前视图模式（0=Overlay, 1=Stacked）
};
