// =============================================================================
// @file    activitybar.cpp
// @brief   活动栏实现 — 按钮创建、页面切换、QSS 属性驱动
// =============================================================================
#include "widgets/activitybar.h"

#include "ui/repolish.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

// =============================================================================
// 构造 / 析构
// =============================================================================

ActivityBar::ActivityBar(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
    setActivePage(ConfigPage);
}

ActivityBar::~ActivityBar() = default;

// =============================================================================
// 信号槽连接
// =============================================================================

void ActivityBar::connectSignals() {
    // 每个按钮点击时：更新活跃状态 + 发出 pageSelected 信号
    connect(m_configButton, &QPushButton::clicked, this, [this] {
        setActivePage(ConfigPage);
        emit pageSelected(ConfigPage);
    });
    connect(m_registerButton, &QPushButton::clicked, this, [this] {
        setActivePage(RegisterPage);
        emit pageSelected(RegisterPage);
    });
    connect(m_flashButton, &QPushButton::clicked, this, [this] {
        setActivePage(FlashPage);
        emit pageSelected(FlashPage);
    });
    connect(m_scopeButton, &QPushButton::clicked, this, [this] {
        setActivePage(ScopePage);
        emit pageSelected(ScopePage);
    });
    connect(m_debugButton, &QPushButton::clicked, this, [this] {
        setActivePage(DebugPage);
        emit pageSelected(DebugPage);
    });
}

// =============================================================================
// 页面状态管理
// =============================================================================

/// @brief 启用/禁用指定页面的导航按钮（ConfigPage 和 DebugPage 始终可用）
void ActivityBar::setPageEnabled(int page, bool enabled) {
    switch (page) {
    case RegisterPage:
        m_registerButton->setEnabled(enabled);
        break;
    case FlashPage:
        m_flashButton->setEnabled(enabled);
        break;
    case ScopePage:
        m_scopeButton->setEnabled(enabled);
        break;
    case DebugPage:
        break; // 调试按钮始终可用
    default:
        break;
    }
}

/// @brief 遍历所有按钮，将匹配索引的按钮设为 active=true，其余 active=false
void ActivityBar::setActivePage(int index) {
    const QList<QPushButton *> buttons = {
        m_configButton,
        m_registerButton,
        m_flashButton,
        m_scopeButton,
        m_debugButton,
        m_settingsButton,
    };

    for (auto *button : buttons) {
        const bool active = (button == m_configButton && index == ConfigPage)
            || (button == m_registerButton && index == RegisterPage)
            || (button == m_flashButton && index == FlashPage)
            || (button == m_scopeButton && index == ScopePage)
            || (button == m_debugButton && index == DebugPage);
        button->setProperty("active", active);
        MotorDev::UiUtil::repolish(button);
    }
}

// =============================================================================
// UI 构建
// =============================================================================

void ActivityBar::setupUi() {
    setObjectName(QStringLiteral("ActivityBar"));
    resize(44, 640);
    setMinimumSize(QSize(44, 0));
    setMaximumSize(QSize(44, QWIDGETSIZE_MAX));

    auto *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    verticalLayout->setSpacing(6);
    verticalLayout->setContentsMargins(4, 8, 4, 8);

    // 5 个导航按钮：配置 / 读写 / 烧录 / 示波 / 调试
    const struct {
        QPushButton **button;
        const char *objectName;
        const char *text;
    } specs[] = {
        {&m_configButton, "configButton", "配置"},
        {&m_registerButton, "registerButton", "读写"},
        {&m_flashButton, "flashButton", "烧录"},
        {&m_scopeButton, "scopeButton", "示波"},
        {&m_debugButton, "debugButton", "调试"},
    };

    for (const auto &spec : specs) {
        // 每个按钮用水平布局居中包裹
        auto *wrapper = new QHBoxLayout();
        wrapper->setContentsMargins(0, 0, 0, 0);
        wrapper->addStretch();
        *spec.button = new QPushButton(this);
        (*spec.button)->setObjectName(QString::fromLatin1(spec.objectName));
        (*spec.button)->setMinimumSize(QSize(34, 34));
        (*spec.button)->setMaximumSize(QSize(34, 34));
        (*spec.button)->setText(tr(spec.text));
        (*spec.button)->setProperty("active", false);
        wrapper->addWidget(*spec.button);
        wrapper->addStretch();
        verticalLayout->addLayout(wrapper);
    }

    // 弹性间距 → 设置按钮推到底部
    verticalLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // 底部设置按钮
    auto *settingsWrapper = new QHBoxLayout();
    settingsWrapper->setContentsMargins(0, 0, 0, 0);
    settingsWrapper->addStretch();
    m_settingsButton = new QPushButton(this);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    m_settingsButton->setMinimumSize(QSize(34, 34));
    m_settingsButton->setMaximumSize(QSize(34, 34));
    m_settingsButton->setText(tr("设置"));
    m_settingsButton->setProperty("active", false);
    settingsWrapper->addWidget(m_settingsButton);
    settingsWrapper->addStretch();
    verticalLayout->addLayout(settingsWrapper);
}
