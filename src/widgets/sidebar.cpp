// =============================================================================
// @file    sidebar.cpp
// @brief   可折叠侧边栏实现 — 动画折叠/展开、内容控件嵌入
// =============================================================================
#include "widgets/sidebar.h"

#include "ui/style_constants.h"

#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

Sidebar::Sidebar(const QString &title, QWidget *contentWidget, QWidget *parent)
    : QWidget(parent) {
    setupUi(title);
    m_headerLabel->setContentsMargins(Style::Size::SidebarContentHorizontalPadding, 0,
                                        Style::Size::SidebarContentHorizontalPadding, 0);

    m_bodyWidth = Style::Size::SidebarWidth;
    setMinimumWidth(Style::Size::SidebarTotalWidth);
    setMaximumWidth(Style::Size::SidebarTotalWidth);

    // maximumWidth 动画用于平滑折叠/展开过渡
    m_animation = new QPropertyAnimation(this, "maximumWidth", this);
    m_animation->setDuration(Style::Size::SidebarAnimationMs);

    // 将占位控件替换为用户传入的内容控件
    auto *bodyLayout = qobject_cast<QVBoxLayout *>(m_bodyWidget->layout());
    m_contentWidget = contentWidget;
    if (bodyLayout != nullptr && m_contentWidget != nullptr) {
        bodyLayout->replaceWidget(m_contentPlaceholder, m_contentWidget);
        m_contentPlaceholder->deleteLater();
        m_contentWidget->setParent(m_bodyWidget);
    }

    connect(m_toggleButton, &QPushButton::clicked, this, &Sidebar::toggleCollapsed);

    // 动画过程中同步 minimumWidth，保证布局系统正确约束
    connect(m_animation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        setMinimumWidth(value.toInt());
    });
}

Sidebar::~Sidebar() = default;

// =============================================================================
// 属性查询
// =============================================================================

bool Sidebar::isCollapsed() const {
    return m_collapsed;
}

QWidget *Sidebar::contentWidget() const {
    return m_contentWidget;
}

void Sidebar::setTitle(const QString &title) {
    if (m_headerLabel != nullptr) {
        m_headerLabel->setText(title);
    }
}

// =============================================================================
// Body 宽度设置
// =============================================================================

void Sidebar::setBodyWidth(int width) {
    if (width <= 0 || m_bodyWidth == width) {
        return;
    }

    m_bodyWidth = width;
    m_bodyWidget->setFixedWidth(m_bodyWidth);
    if (!m_collapsed) {
        applyExpandedWidth();
    }
}

// =============================================================================
// 折叠/展开控制
// =============================================================================

void Sidebar::toggleCollapsed() {
    setCollapsed(!m_collapsed);
}

/// @brief 执行折叠/展开动画
///
/// 折叠：动画缩到 handle 宽度，结束后隐藏 body
/// 展开：先显示 body，再动画展开到完整宽度
void Sidebar::setCollapsed(bool collapsed) {
    if (m_collapsed == collapsed) {
        return;
    }

    const int startWidth = maximumWidth();
    const int targetWidth = collapsed ? Style::Size::SidebarHandleWidth
                                      : m_bodyWidth + Style::Size::SidebarHandleWidth;

    m_animation->stop();
    m_animation->disconnect(this);
    connect(m_animation, &QPropertyAnimation::valueChanged, this, [this](const QVariant &value) {
        setMinimumWidth(value.toInt());
    });
    connect(m_animation, &QPropertyAnimation::finished, this, [this, collapsed] {
        if (collapsed) {
            m_bodyWidget->setVisible(false); // 折叠完成后隐藏 body
        }
    });

    if (!collapsed) {
        m_bodyWidget->setVisible(true); // 展开前先显示 body
    }

    m_animation->setStartValue(startWidth);
    m_animation->setEndValue(targetWidth);
    m_animation->start();

    m_collapsed = collapsed;
    m_toggleButton->setText(m_collapsed ? QStringLiteral(">") : QStringLiteral("<"));
    emit collapseStateChanged(m_collapsed);
}

/// @brief 设置展开后的 min/max 宽度约束
void Sidebar::applyExpandedWidth() {
    const int totalWidth = m_bodyWidth + Style::Size::SidebarHandleWidth;
    setMinimumWidth(totalWidth);
    setMaximumWidth(totalWidth);
}

// =============================================================================
// UI 构建
// =============================================================================

void Sidebar::setupUi(const QString &title) {
    setObjectName(QStringLiteral("Sidebar"));

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // Body 区域：标题 + 内容占位
    m_bodyWidget = new QWidget(this);
    m_bodyWidget->setObjectName(QStringLiteral("bodyWidget"));
    m_bodyWidget->setMinimumSize(QSize(150, 0));
    m_bodyWidget->setMaximumSize(QSize(150, QWIDGETSIZE_MAX));
    auto *bodyLayout = new QVBoxLayout(m_bodyWidget);
    bodyLayout->setObjectName(QStringLiteral("bodyLayout"));
    bodyLayout->setSpacing(0);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(m_bodyWidget);

    m_headerLabel = new QLabel(m_bodyWidget);
    m_headerLabel->setObjectName(QStringLiteral("headerLabel"));
    m_headerLabel->setMinimumSize(QSize(0, 30));
    m_headerLabel->setMaximumSize(QSize(QWIDGETSIZE_MAX, 30));
    m_headerLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
    m_headerLabel->setText(title);
    bodyLayout->addWidget(m_headerLabel);

    // 占位控件（构造完成后被 contentWidget 替换）
    m_contentPlaceholder = new QWidget(m_bodyWidget);
    m_contentPlaceholder->setObjectName(QStringLiteral("contentPlaceholder"));
    bodyLayout->addWidget(m_contentPlaceholder);

    // 折叠手柄按钮
    m_toggleButton = new QPushButton(this);
    m_toggleButton->setObjectName(QStringLiteral("toggleButton"));
    m_toggleButton->setMinimumSize(QSize(Style::Size::SidebarHandleWidth, 0));
    m_toggleButton->setMaximumSize(QSize(Style::Size::SidebarHandleWidth, QWIDGETSIZE_MAX));
    m_toggleButton->setText(QStringLiteral("<"));
    rootLayout->addWidget(m_toggleButton);
}
