// =============================================================================
// @file    scopechannelstrip.cpp
// @brief   示波器通道条实现 — UI 构建、信号槽连接
// =============================================================================
#include "widgets/scopechannelstrip.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QSizePolicy>
#include <QVBoxLayout>

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造通道条，根据 @p index 设置默认文本与地址。
///
/// - CH1~CH4 默认启用，并设置预设描述（Speed / Torque / Temp / Current）
/// - 地址按 0x0010 + index*2 自动递增
ScopeChannelStrip::ScopeChannelStrip(int index, QWidget *parent)
    : QWidget(parent)
    , m_index(index) {
    setupUi();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // ---------- 根据通道序号填充默认值 ----------
    m_checkBox->setText(QStringLiteral("CH%1").arg(m_index + 1));
    m_checkBox->setChecked(m_index < 4);               // 前 4 通道默认启用

    // 预设通道描述：CH1=Speed, CH2=Torque, CH3=Temp, CH4=Current
    m_descEdit->setText(m_index == 0 ? QStringLiteral("Speed")
                     : m_index == 1 ? QStringLiteral("Torque")
                     : m_index == 2 ? QStringLiteral("Temp")
                     : m_index == 3 ? QStringLiteral("Current")
                                     : QString());

    // 地址从 0x0010 开始，每通道偏移 2 字节
    m_addrEdit->setText(QStringLiteral("0x%1").arg(0x0010 + m_index * 2, 4, 16, QLatin1Char('0')).toUpper());
    connectSignals();
}

ScopeChannelStrip::~ScopeChannelStrip() = default;

// =============================================================================
// 公共查询接口
// =============================================================================

/// @brief 返回当前通道是否启用（CheckBox 选中状态）。
bool ScopeChannelStrip::isChannelEnabled() const {
    return m_checkBox->isChecked();
}

// =============================================================================
// UI 构建
// =============================================================================

/// @brief 创建垂直布局：CheckBox → 描述输入 → 地址输入。
void ScopeChannelStrip::setupUi() {
    setObjectName(QStringLiteral("ScopeChannelStrip"));
    setMinimumSize(QSize(104, 0));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(4);
    rootLayout->setContentsMargins(6, 6, 6, 6);

    // --- 通道启用复选框 ---
    m_checkBox = new QCheckBox(this);
    m_checkBox->setObjectName(QStringLiteral("checkBox"));
    m_checkBox->setText(tr("CH1"));                     // 占位文本，构造函数中会覆盖
    rootLayout->addWidget(m_checkBox);

    // --- 通道描述输入框 ---
    m_descEdit = new QLineEdit(this);
    m_descEdit->setObjectName(QStringLiteral("descEdit"));
    m_descEdit->setMinimumSize(QSize(0, 22));
    m_descEdit->setMaximumSize(QSize(QWIDGETSIZE_MAX, 22));
    m_descEdit->setPlaceholderText(tr("Description"));
    rootLayout->addWidget(m_descEdit);

    // --- 寄存器地址输入框 ---
    m_addrEdit = new QLineEdit(this);
    m_addrEdit->setObjectName(QStringLiteral("addrEdit"));
    m_addrEdit->setMinimumSize(QSize(0, 22));
    m_addrEdit->setMaximumSize(QSize(QWIDGETSIZE_MAX, 22));
    m_addrEdit->setPlaceholderText(tr("0x0000"));
    rootLayout->addWidget(m_addrEdit);
}

// =============================================================================
// 信号槽连接
// =============================================================================

/// @brief 将子控件信号转发为携带通道 index 的外部信号。
///
/// 所有信号都附带 m_index，使外部可区分来自哪个通道条。
void ScopeChannelStrip::connectSignals() {
    // 通道启用/禁用
    connect(m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit channelToggled(m_index, checked);
    });
    // 描述文本变化
    connect(m_descEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit descriptionChanged(m_index, text);
    });
    // 地址文本变化
    connect(m_addrEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit addressChanged(m_index, text);
    });
}
