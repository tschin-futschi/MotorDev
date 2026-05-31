// =============================================================================
// @file    activitybar.h
// @brief   活动栏 — 左侧垂直导航按钮栏，用于切换主界面页面
//
// ActivityBar 位于主窗口最左侧，提供 5 个页面导航按钮：
// 配置 → 读写 → 烧录 → 示波 → 调试，底部为设置按钮。
// 当前选中的按钮通过 "active" 动态属性驱动 QSS 高亮样式。
// =============================================================================
#pragma once

#include <QWidget>
class QEvent;
class QPushButton;

/// @brief 左侧活动栏控件 — 垂直排列的页面导航按钮
///
/// 固定宽度 44px，按钮 34x34px。点击按钮时发出 pageSelected 信号，
/// 并更新 "active" 属性触发 QSS 样式切换。
class ActivityBar : public QWidget {
    Q_OBJECT

public:
    /// @brief 页面索引枚举
    enum Page {
        ConfigPage = 0,         ///< 配置页
        RegisterPage = 1,       ///< 寄存器读写页
        FlashPage = 2,          ///< 固件烧录页（AW IC ISP）
        ScopePage = 3,          ///< 示波器页
        FlashStoragePage = 4,   ///< STM32 自身 FLASH 文件存储页（协议 v2.10）
        DebugPage = 5,          ///< 串口调试页（浮动窗口，不占 ContentStack）
    };

    /// @brief 构造活动栏，默认选中 ConfigPage
    explicit ActivityBar(QWidget *parent = nullptr);
    ~ActivityBar() override;

    /// @brief 设置指定页面按钮的启用/禁用状态
    /// @param page 页面索引（Page 枚举值）
    /// @param enabled true=启用，false=禁用（灰色不可点击）
    void setPageEnabled(int page, bool enabled);

signals:
    /// @brief 用户点击页面按钮时发出
    /// @param index 选中的页面索引（Page 枚举值）
    void pageSelected(int index);

protected:
    /// @brief 语言切换（QEvent::LanguageChange）时刷新所有导航按钮文字
    void changeEvent(QEvent *event) override;

private:
    /// @brief 构建 UI 布局（垂直排列 5 个导航按钮 + 底部设置按钮）
    void setupUi();

    /// @brief 连接按钮点击信号到 pageSelected
    void connectSignals();

    /// @brief 重设所有导航按钮文字（用字面量 tr 以便 lupdate 提取 + 语言切换刷新）
    void retranslateUi();

    /// @brief 更新按钮 "active" 属性并触发 QSS 重绘
    /// @param index 当前活跃页面索引
    void setActivePage(int index);

    QPushButton *m_configButton = nullptr;      ///< "配置"按钮
    QPushButton *m_registerButton = nullptr;    ///< "读写"按钮
    QPushButton *m_flashButton = nullptr;       ///< "烧录"按钮
    QPushButton *m_scopeButton = nullptr;       ///< "示波"按钮
    QPushButton *m_storageButton = nullptr;     ///< "存储"按钮（FLASH 文件存储）
    QPushButton *m_debugButton = nullptr;       ///< "调试"按钮
    QPushButton *m_settingsButton = nullptr;    ///< "设置"按钮（底部）
};
