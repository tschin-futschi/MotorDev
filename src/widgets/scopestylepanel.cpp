// =============================================================================
// @file    scopestylepanel.cpp
// @brief   示波器样式面板实现 — 颜色选择器、线宽/线型配置、恢复默认
//
// 面板布局（每通道一行）：
//   [Channel Style 标题]
//   [Default Setting 按钮]
//   [CH1 标签] [颜色按钮] [线宽 SpinBox] [线型 ComboBox]
//   [CH2 标签] [颜色按钮] [线宽 SpinBox] [线型 ComboBox]
//   ...
//   [弹簧]
//
// 线型选项：Solid / Dash / Dot / DashDot / SolidDot
// 其中 SolidDot 表示实线 + 数据点圆点显示（通过 dataPointsChanged 信号通知）。
// =============================================================================
#include "widgets/scopestylepanel.h"

#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace {
/// @brief 将 Qt::PenStyle 映射为 ComboBox 索引。
int styleIndexFor(Qt::PenStyle style) {
    switch (style) {
    case Qt::DashLine:
        return 1;
    case Qt::DotLine:
        return 2;
    case Qt::DashDotLine:
        return 3;
    case Qt::SolidLine:
    default:
        return 0;
    }
}
}

// =============================================================================
// 构造 / 析构
// =============================================================================

ScopeStylePanel::ScopeStylePanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
}

ScopeStylePanel::~ScopeStylePanel() = default;

// =============================================================================
// 公共设置接口
// =============================================================================

/// @brief 设置指定通道的颜色，同步更新颜色按钮背景。
void ScopeStylePanel::setChannelColor(int index, const QColor &color) {
    if (index < 0 || index >= kChannelCount || !color.isValid()) {
        return;
    }

    m_rows[index].currentColor = color;
    updateColorButton(index);
}

/// @brief 设置指定通道的线宽（使用 QSignalBlocker 防止循环信号）。
void ScopeStylePanel::setChannelLineWidth(int index, int width) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    QSignalBlocker blocker(m_rows[index].widthSpin);
    m_rows[index].widthSpin->setValue(width);
}

/// @brief 设置指定通道的线型（使用 QSignalBlocker 防止循环信号）。
void ScopeStylePanel::setChannelLineStyle(int index, Qt::PenStyle style) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    QSignalBlocker blocker(m_rows[index].styleCombo);
    m_rows[index].styleCombo->setCurrentIndex(styleIndexFor(style));
}

/// @brief 设置指定通道是否显示数据点。
///
/// 数据点模式对应 ComboBox 索引 4（SolidDot）。
/// 若关闭数据点且当前恰好是 SolidDot，回退到 Solid。
void ScopeStylePanel::setChannelShowDataPoints(int index, bool show) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    QSignalBlocker blocker(m_rows[index].styleCombo);
    if (show) {
        m_rows[index].styleCombo->setCurrentIndex(4);   // SolidDot
    } else if (m_rows[index].styleCombo->currentIndex() == 4) {
        m_rows[index].styleCombo->setCurrentIndex(0);   // 回退到 Solid
    }
}

// =============================================================================
// 信号槽连接
// =============================================================================

/// @brief 连接恢复默认按钮和 8 个通道的颜色/线宽/线型变更信号。
void ScopeStylePanel::connectSignals() {
    // 恢复默认设置
    connect(m_defaultButton, &QPushButton::clicked, this, [this]() {
        emit defaultSettingsRequested();
    });

    for (int index = 0; index < kChannelCount; ++index) {
        // --- 颜色选择 ---
        connect(m_rows[index].colorButton, &QPushButton::clicked, this, [this, index]() {
            const QColor selected = QColorDialog::getColor(
                m_rows[index].currentColor,
                this,
                tr("Select channel color"));
            if (!selected.isValid()) {
                return;                                 // 用户取消
            }

            m_rows[index].currentColor = selected;
            updateColorButton(index);
            emit colorChanged(index, selected);
        });

        // --- 线宽变更 ---
        connect(m_rows[index].widthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this, index](int width) {
            emit lineWidthChanged(index, width);
        });

        // --- 线型变更（含 SolidDot 数据点模式） ---
        connect(m_rows[index].styleCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index](int comboIndex) {
            Qt::PenStyle style = Qt::SolidLine;
            bool dots = false;
            switch (comboIndex) {
            case 1: style = Qt::DashLine; break;
            case 2: style = Qt::DotLine; break;
            case 3: style = Qt::DashDotLine; break;
            case 4: style = Qt::SolidLine; dots = true; break;  // SolidDot：实线+数据点
            default: break;
            }
            emit lineStyleChanged(index, style);
            emit dataPointsChanged(index, dots);
        });
    }
}

// =============================================================================
// 内部方法
// =============================================================================

/// @brief 同步颜���按钮的背景色样式（运行时选中的颜色无法用静态 QSS 表达）。
void ScopeStylePanel::updateColorButton(int index) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }

    // Runtime-selected color cannot be expressed in static QSS.
    m_rows[index].colorButton->setStyleSheet(QStringLiteral("background:%1;").arg(m_rows[index].currentColor.name()));
}

// =============================================================================
// UI 构建
// =============================================================================

/// @brief 创建样式面板：标题 → 恢复默认按钮 → 8 行通道控件 → 弹簧。
void ScopeStylePanel::setupUi() {
    setObjectName(QStringLiteral("ScopeStylePanel"));
    setMinimumSize(QSize(200, 0));
    setMaximumSize(QSize(200, QWIDGETSIZE_MAX));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(2);
    rootLayout->setContentsMargins(8, 8, 8, 8);

    // 标题
    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setText(tr("Channel Style"));
    rootLayout->addWidget(titleLabel);

    // 恢复默认按钮
    m_defaultButton = new QPushButton(this);
    m_defaultButton->setObjectName(QStringLiteral("defaultButton"));
    m_defaultButton->setProperty("buttonRole", QStringLiteral("default"));
    m_defaultButton->setText(tr("Default Setting"));
    rootLayout->addWidget(m_defaultButton);

    // ---------- 8 行通道样式控件 ----------
    const QStringList comboItems = {tr("Solid"), tr("Dash"), tr("Dot"), tr("DashDot"), tr("SolidDot")};
    for (int index = 0; index < kChannelCount; ++index) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(QStringLiteral("row%1Layout").arg(index));
        rowLayout->setSpacing(2);

        // 通道标签
        auto *label = new QLabel(this);
        label->setObjectName(QStringLiteral("ch%1Label").arg(index));
        label->setMinimumSize(QSize(30, 0));
        label->setMaximumSize(QSize(30, QWIDGETSIZE_MAX));
        label->setText(QStringLiteral("CH%1").arg(index + 1));
        rowLayout->addWidget(label);

        // 颜色选择按钮（20×20 小方块）
        m_rows[index].colorButton = new QPushButton(this);
        m_rows[index].colorButton->setObjectName(QStringLiteral("colorButton%1").arg(index));
        m_rows[index].colorButton->setMinimumSize(QSize(20, 20));
        m_rows[index].colorButton->setMaximumSize(QSize(20, 20));
        m_rows[index].colorButton->setProperty("buttonRole", QStringLiteral("color"));
        m_rows[index].colorButton->setText(QString());
        rowLayout->addWidget(m_rows[index].colorButton);

        // 线宽 SpinBox（1~8 px）
        m_rows[index].widthSpin = new QSpinBox(this);
        m_rows[index].widthSpin->setObjectName(QStringLiteral("widthSpin%1").arg(index));
        m_rows[index].widthSpin->setMinimumSize(QSize(92, 24));
        m_rows[index].widthSpin->setMaximumSize(QSize(92, 24));
        m_rows[index].widthSpin->setRange(1, 8);
        m_rows[index].widthSpin->setValue(4);
        m_rows[index].widthSpin->setSuffix(QStringLiteral("px"));
        rowLayout->addWidget(m_rows[index].widthSpin);

        // 线型 ComboBox
        m_rows[index].styleCombo = new QComboBox(this);
        m_rows[index].styleCombo->setObjectName(QStringLiteral("styleCombo%1").arg(index));
        m_rows[index].styleCombo->addItems(comboItems);
        rowLayout->addWidget(m_rows[index].styleCombo);

        rootLayout->addLayout(rowLayout);
    }

    // 底部弹簧
    rootLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
