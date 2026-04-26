// =============================================================================
// @file    fwflashtab.h
// @brief   固件烧录页面（占位）— 未来用于固件选择、烧录进度展示
//
// 当前为占位实现，仅包含侧边栏和主内容区的基础布局骨架。
// 后续将集成固件文件选择、烧录进度条、烧录日志等功能。
// =============================================================================
#pragma once

#include <QLabel>
#include <QWidget>

/// @brief 固件烧录页面（占位 Tab）
///
/// 提供固件烧录功能的 UI 占位，当前仅展示侧边栏标题和主区域提示文字。
/// 布局结构：左侧 Sidebar + 右侧主内容区。
class FwFlashTab : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造固件烧录页面
    /// @param parent 父控件指针
    explicit FwFlashTab(QWidget *parent = nullptr);
    ~FwFlashTab() override;

private:
    /// @brief 构建 UI 布局（侧边栏 + 主内容区）
    void setupUi();

    /// @brief 连接信号槽（当前为空）
    void connectSignals();

    QWidget *m_sidebarContent = nullptr;    ///< 侧边栏内容容器
    QWidget *m_mainContent = nullptr;       ///< 主内容区容器
    QLabel *m_sidebarLabel = nullptr;       ///< 侧边栏提示文字
    QLabel *m_placeholderLabel = nullptr;   ///< 主区域占位文字
};
