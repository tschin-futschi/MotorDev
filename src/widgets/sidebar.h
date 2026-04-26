// =============================================================================
// @file    sidebar.h
// @brief   可折叠侧边栏 — 带标题、内容区和折叠动画的通用侧边栏控件
//
// Sidebar 由三部分组成：
// - Body（主体区域）：标题标签 + 用户传入的内容控件
// - Toggle Handle（折叠手柄）：窄按钮，点击触发折叠/展开
// - 动画：通过 QPropertyAnimation 平滑过渡 maximumWidth
//
// 布局结构：
// ┌────────────────┬──┐
// │ Header Label   │< │  ← Toggle Handle (18px)
// ├────────────────┤  │
// │ Content Widget │  │
// │ (用户传入)     │  │
// └────────────────┴──┘
// =============================================================================
#pragma once

#include <QWidget>

class QPropertyAnimation;
class QLabel;
class QPushButton;

/// @brief 可折叠侧边栏控件
///
/// 接收标题和内容控件，提供折叠/展开动画。
/// 折叠时仅显示 Toggle Handle（18px），展开时显示完整内容区。
class Sidebar : public QWidget {
    Q_OBJECT

public:
    /// @brief 构造侧边栏
    /// @param title 标题文字（显示在顶部）
    /// @param contentWidget 用户提供的内容控件（会被嵌入到 body 区域）
    /// @param parent 父控件指针
    explicit Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent = nullptr);
    ~Sidebar() override;

    /// @brief 当前是否处于折叠状态
    bool isCollapsed() const;

    /// @brief 获取嵌入的内容控件
    QWidget *contentWidget() const;

    /// @brief 设置 body 区域宽度（展开时生效）
    /// @param width 像素宽度（不含 handle 宽度）
    void setBodyWidth(int width);

public slots:
    /// @brief 切换折叠/展开状态
    void toggleCollapsed();

    /// @brief 设置折叠状态
    /// @param collapsed true=折叠，false=展开
    void setCollapsed(bool collapsed);

signals:
    /// @brief 折叠状态变化
    /// @param collapsed true=已折叠
    void collapseStateChanged(bool collapsed);

private:
    /// @brief 应用展开后的总宽度（body + handle）
    void applyExpandedWidth();

    /// @brief 构建 UI 布局
    void setupUi(const QString &title);

    QWidget *m_bodyWidget = nullptr;            ///< 主体容器（标题 + 内容）
    QLabel *m_headerLabel = nullptr;            ///< 标题标签
    QWidget *m_contentPlaceholder = nullptr;    ///< 占位控件（构造时被 contentWidget 替换）
    QPushButton *m_toggleButton = nullptr;      ///< 折叠/展开手柄按钮
    QWidget *m_contentWidget = nullptr;         ///< 用户传入的内容控件
    QPropertyAnimation *m_animation = nullptr;  ///< maximumWidth 动画
    bool m_collapsed = false;                   ///< 当前折叠状态
    int m_bodyWidth = 0;                        ///< body 区域宽度（不含 handle）
};
