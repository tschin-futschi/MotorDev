// =============================================================================
// @file    fwflashtab.cpp
// @brief   固件烧录页面实现 — 占位布局
// =============================================================================
#include "tabs/fwflashtab.h"

#include "widgets/sidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QVBoxLayout>

FwFlashTab::FwFlashTab(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

FwFlashTab::~FwFlashTab() = default;

void FwFlashTab::connectSignals() {
    // 当前无信号槽连接，后续实现烧录功能时补充
}

// -----------------------------------------------------------------------------
// UI 构建
// -----------------------------------------------------------------------------

void FwFlashTab::setupUi() {
    setObjectName(QStringLiteral("FwFlashTab"));

    // 顶层水平布局：侧边栏 | 主内容区
    auto *topLayout = new QHBoxLayout(this);
    topLayout->setObjectName(QStringLiteral("topLayout"));
    topLayout->setSpacing(0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    // --- 侧边栏 ---
    m_sidebarContent = new QWidget(this);
    m_sidebarContent->setObjectName(QStringLiteral("sidebarContent"));
    auto *sidebarLayout = new QVBoxLayout(m_sidebarContent);
    sidebarLayout->setObjectName(QStringLiteral("sidebarLayout"));
    sidebarLayout->setSpacing(8);
    sidebarLayout->setContentsMargins(10, 8, 10, 8);

    m_sidebarLabel = new QLabel(m_sidebarContent);
    m_sidebarLabel->setObjectName(QStringLiteral("sidebarLabel"));
    m_sidebarLabel->setWordWrap(true);
    m_sidebarLabel->setText(tr("烧录侧边栏"));
    sidebarLayout->addWidget(m_sidebarLabel);
    sidebarLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    auto *sidebar = new Sidebar(tr("烧录"), m_sidebarContent, this);
    topLayout->addWidget(sidebar);

    // --- 主内容区 ---
    m_mainContent = new QWidget(this);
    m_mainContent->setObjectName(QStringLiteral("mainContent"));
    auto *mainLayout = new QVBoxLayout(m_mainContent);
    mainLayout->setObjectName(QStringLiteral("mainLayout"));
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    m_placeholderLabel = new QLabel(m_mainContent);
    m_placeholderLabel->setObjectName(QStringLiteral("placeholderLabel"));
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setText(tr("固件烧录页面"));
    mainLayout->addWidget(m_placeholderLabel);
    topLayout->addWidget(m_mainContent);
}
