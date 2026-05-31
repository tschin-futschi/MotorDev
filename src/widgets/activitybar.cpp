// =============================================================================
// @file    activitybar.cpp
// @brief   活动栏实现 — 按钮创建、页面切换、QSS 属性驱动
// =============================================================================
#include "widgets/activitybar.h"

#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

using namespace MotorDev;

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
    connect(m_storageButton, &QPushButton::clicked, this, [this] {
        setActivePage(FlashStoragePage);
        emit pageSelected(FlashStoragePage);
    });
    connect(m_debugButton, &QPushButton::clicked, this, [this] {
        setActivePage(DebugPage);
        emit pageSelected(DebugPage);
    });
    // 「关于」非页面切换，不改 active 态，仅请求弹出对话框
    connect(m_aboutButton, &QPushButton::clicked, this, &ActivityBar::aboutRequested);
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
    case FlashStoragePage:
        m_storageButton->setEnabled(enabled);
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
        m_storageButton,
        m_debugButton,
    };

    for (auto *button : buttons) {
        const bool active = (button == m_configButton && index == ConfigPage)
            || (button == m_registerButton && index == RegisterPage)
            || (button == m_flashButton && index == FlashPage)
            || (button == m_scopeButton && index == ScopePage)
            || (button == m_storageButton && index == FlashStoragePage)
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
    resize(Style::Size::ActivityBarWidth, 640);
    setMinimumSize(QSize(Style::Size::ActivityBarWidth, 0));
    setMaximumSize(QSize(Style::Size::ActivityBarWidth, QWIDGETSIZE_MAX));

    auto *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    verticalLayout->setSpacing(6);
    verticalLayout->setContentsMargins(4, 8, 4, 8);

    // 6 个导航按钮：配置 / 读写 / 烧录 / 示波 / 存储 / 调试
    const struct {
        QPushButton **button;
        const char *objectName;
        const char *text;
    } specs[] = {
        {&m_configButton, "configButton", "配置"},
        {&m_registerButton, "registerButton", "读写"},
        {&m_flashButton, "flashButton", "烧录"},
        {&m_scopeButton, "scopeButton", "示波"},
        {&m_storageButton, "storageButton", "存储"},
        {&m_debugButton, "debugButton", "调试"},
    };

    for (const auto &spec : specs) {
        // 每个按钮用水平布局居中包裹
        auto *wrapper = new QHBoxLayout();
        wrapper->setContentsMargins(0, 0, 0, 0);
        wrapper->addStretch();
        *spec.button = new QPushButton(this);
        (*spec.button)->setObjectName(QString::fromLatin1(spec.objectName));
        (*spec.button)->setMinimumSize(QSize(Style::Size::ActivityButtonSize, Style::Size::ActivityButtonSize));
        (*spec.button)->setMaximumSize(QSize(Style::Size::ActivityButtonSize, Style::Size::ActivityButtonSize));
        (*spec.button)->setText(tr(spec.text));
        (*spec.button)->setProperty("active", false);
        wrapper->addWidget(*spec.button);
        wrapper->addStretch();
        verticalLayout->addLayout(wrapper);
    }

    // 弹性间距 → 导航按钮顶部对齐，「关于」按钮推到底部
    verticalLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // 底部「关于」按钮（非页面切换，点击弹出关于对话框）
    auto *aboutWrapper = new QHBoxLayout();
    aboutWrapper->setContentsMargins(0, 0, 0, 0);
    aboutWrapper->addStretch();
    m_aboutButton = new QPushButton(this);
    m_aboutButton->setObjectName(QStringLiteral("aboutButton"));
    m_aboutButton->setMinimumSize(QSize(Style::Size::ActivityButtonSize, Style::Size::ActivityButtonSize));
    m_aboutButton->setMaximumSize(QSize(Style::Size::ActivityButtonSize, Style::Size::ActivityButtonSize));
    m_aboutButton->setProperty("active", false);
    aboutWrapper->addWidget(m_aboutButton);
    aboutWrapper->addStretch();
    verticalLayout->addLayout(aboutWrapper);

    retranslateUi();
}

// =============================================================================
// 语言切换 / 文字重设
// =============================================================================

void ActivityBar::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

/// @brief 重设所有导航按钮文字，并在英文模式下补 tooltip。
///
/// 注：setupUi 中按钮经 tr(spec.text) 设过初值，但 spec.text 是运行时 const char*，
/// lupdate 无法静态提取；此处用字面量 tr() 既保证可提取入 .ts，又支持语言即时切换。
///
/// 侧边栏按钮为 34px 方块（按 2 个汉字设计）。英文词较长会超出按钮被截断，故在英文
/// 模式下给每个按钮设置 tooltip = 完整英文词，鼠标悬停即可看到全称；中文 2 字可完整
/// 显示，不设 tooltip（清空）。以「tr 结果是否相对源串变化」判断当前是否英文模式。
void ActivityBar::retranslateUi() {
    const bool isEnglish = (tr("配置") != QStringLiteral("配置"));
    auto applyLabel = [isEnglish](QPushButton *button, const QString &text) {
        if (button == nullptr) {
            return;
        }
        button->setText(text);
        button->setToolTip(isEnglish ? text : QString());
    };
    applyLabel(m_configButton, tr("配置"));
    applyLabel(m_registerButton, tr("读写"));
    applyLabel(m_flashButton, tr("烧录"));
    applyLabel(m_scopeButton, tr("示波"));
    applyLabel(m_storageButton, tr("存储"));
    applyLabel(m_debugButton, tr("调试"));
    applyLabel(m_aboutButton, tr("关于"));
}
