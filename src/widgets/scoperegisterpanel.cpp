// =============================================================================
// @file    scoperegisterpanel.cpp
// @brief   示波器寄存器辅助面板实现 — 8 行读写 UI、循环写入控制
//
// 布局结构（每行一组）：
//   [描述输入] [地址输入] [值输入] [R按钮] [W按钮]  × 8 行
//   [间隔标签] [间隔输入] [启动] [停止] ---弹簧--- [清除面板] [HEX/DEC]
//
// 本面板仅负责 UI 展示和用户交互信号发射，
// 实际的寄存器读写逻辑由外部（OscilloscpTab）通过信号槽连接处理。
// =============================================================================
#include "widgets/scoperegisterpanel.h"

#include "protocol/register_utils.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

ScopeRegisterPanel::ScopeRegisterPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
    setCyclicRunning(false);                            // 初始状态：循环写入停止
}

ScopeRegisterPanel::~ScopeRegisterPanel() = default;

// =============================================================================
// 公共查询接口
// =============================================================================

/// @brief 返回指定行的地址文本。
QString ScopeRegisterPanel::addressText(int row) const {
    return (row >= 0 && row < RowCount && m_addrEdits[row] != nullptr) ? m_addrEdits[row]->text() : QString();
}

/// @brief 返回指定行的值文本。
QString ScopeRegisterPanel::valueText(int row) const {
    return (row >= 0 && row < RowCount && m_valueEdits[row] != nullptr) ? m_valueEdits[row]->text() : QString();
}

/// @brief 返回循环写入间隔文本。
QString ScopeRegisterPanel::intervalText() const {
    return m_intervalEdit != nullptr ? m_intervalEdit->text() : QString();
}

// =============================================================================
// 公共设置接口
// =============================================================================

/// @brief 设置指定行的值输入框文本（原样写入，不做格式化）。
void ScopeRegisterPanel::setValueText(int row, const QString &text) {
    if (row < 0 || row >= RowCount || m_valueEdits[row] == nullptr) {
        return;
    }
    m_valueEdits[row]->setText(text);
}

/// @brief 按当前显示进制格式化并写入指定行的值（读回填用），同时清除该行红框。
void ScopeRegisterPanel::setValueNumeric(int row, qint16 value) {
    if (row < 0 || row >= RowCount || m_valueEdits[row] == nullptr) {
        return;
    }
    m_valueEdits[row]->setText(RegisterUtils::formatValue(value, m_hexMode));
    setValueError(row, false);
}

/// @brief 设置指定行的地址输入框错误状态。
///
/// 通过 QSS 动态属性 "hasError" 控制红色边框样式。
void ScopeRegisterPanel::setAddressError(int row, bool error) {
    if (row < 0 || row >= RowCount || m_addrEdits[row] == nullptr) {
        return;
    }

    m_addrEdits[row]->setProperty("hasError", error);
    MotorDev::UiUtil::repolish(m_addrEdits[row]);
}

/// @brief 设置指定行的值输入框错误状态（数值非法时红框，复用 hasError QSS）。
void ScopeRegisterPanel::setValueError(int row, bool error) {
    if (row < 0 || row >= RowCount || m_valueEdits[row] == nullptr) {
        return;
    }

    m_valueEdits[row]->setProperty("hasError", error);
    MotorDev::UiUtil::repolish(m_valueEdits[row]);
}

/// @brief 校验指定行数值：空文本清错；非空则解析，失败标红、成功清错。
void ScopeRegisterPanel::validateValueRow(int row) {
    if (row < 0 || row >= RowCount || m_valueEdits[row] == nullptr) {
        return;
    }

    const QString text = m_valueEdits[row]->text().trimmed();
    if (text.isEmpty()) {
        setValueError(row, false);
        return;
    }
    setValueError(row, !RegisterUtils::parseSignedValue(text, nullptr));
}

/// @brief 切换 HEX/DEC 显示模式：翻转标志 → 重排数值 → 刷新按钮文案。
void ScopeRegisterPanel::toggleValueFormat() {
    m_hexMode = !m_hexMode;
    reformatAllValues();
    refreshFormatButtonText();
}

/// @brief 按当前模式重排所有行数值：合法值重新格式化并清错，非法非空保留原文并标红。
void ScopeRegisterPanel::reformatAllValues() {
    for (int row = 0; row < RowCount; ++row) {
        if (m_valueEdits[row] == nullptr) {
            continue;
        }
        const QString text = m_valueEdits[row]->text().trimmed();
        if (text.isEmpty()) {
            setValueError(row, false);
            continue;
        }
        qint16 value = 0;
        if (RegisterUtils::parseSignedValue(text, &value)) {
            m_valueEdits[row]->setText(RegisterUtils::formatValue(value, m_hexMode));
            setValueError(row, false);
        } else {
            setValueError(row, true);
        }
    }
}

/// @brief 切换按钮文案显示当前显示模式。
void ScopeRegisterPanel::refreshFormatButtonText() {
    if (m_formatToggleButton != nullptr) {
        m_formatToggleButton->setText(m_hexMode ? QStringLiteral("HEX") : QStringLiteral("DEC"));
    }
}

/// @brief 设置指定行按钮的操作反馈状态（成功/失败闪烁）。
///
/// @param row     行号
/// @param isRead  true=读按钮，false=写按钮
/// @param state   反馈状态字符串（"ok" / "error" / 空字符串清除）
///
/// 非空状态在 600 ms 后自动清除，实现短暂的视觉反馈效果。
void ScopeRegisterPanel::setButtonFeedback(int row, bool isRead, const QString &state) {
    if (row < 0 || row >= RowCount) {
        return;
    }

    QPushButton *button = isRead ? m_readButtons[row] : m_writeButtons[row];
    if (button == nullptr) {
        return;
    }

    // 设置 QSS 动态属性触发样式变化
    button->setProperty("feedback", state);
    MotorDev::UiUtil::repolish(button);

    // 非空状态 600 ms 后自动清除
    if (!state.isEmpty()) {
        QTimer::singleShot(600, button, [button]() {
            button->setProperty("feedback", QString());
            MotorDev::UiUtil::repolish(button);
        });
    }
}

/// @brief 设置循环写入运行状态，更新启动/停止按钮的 active 属性。
void ScopeRegisterPanel::setCyclicRunning(bool running) {
    if (m_startButton != nullptr) {
        m_startButton->setProperty("active", running);
        MotorDev::UiUtil::repolish(m_startButton);
    }

    if (m_stopButton != nullptr) {
        m_stopButton->setProperty("active", !running);
        MotorDev::UiUtil::repolish(m_stopButton);
    }
}

/// @brief 清空所有行的描述、地址、值输入框和错误状态。
void ScopeRegisterPanel::clearAll() {
    for (int row = 0; row < RowCount; ++row) {
        if (m_descEdits[row] != nullptr) {
            m_descEdits[row]->clear();
        }
        if (m_addrEdits[row] != nullptr) {
            m_addrEdits[row]->clear();
            setAddressError(row, false);
        }
        if (m_valueEdits[row] != nullptr) {
            m_valueEdits[row]->clear();
            setValueError(row, false);
        }
    }

    if (m_intervalEdit != nullptr) {
        m_intervalEdit->clear();
    }
}

// =============================================================================
// 配置文件存取：8 行 desc/addr/val + 循环间隔
// =============================================================================

QJsonObject ScopeRegisterPanel::toJson() const {
    QJsonObject obj;
    QJsonArray rows;
    for (int row = 0; row < RowCount; ++row) {
        QJsonObject r;
        r.insert(QStringLiteral("desc"), m_descEdits[row] != nullptr ? m_descEdits[row]->text() : QString());
        r.insert(QStringLiteral("addr"), m_addrEdits[row] != nullptr ? m_addrEdits[row]->text() : QString());
        r.insert(QStringLiteral("val"), m_valueEdits[row] != nullptr ? m_valueEdits[row]->text() : QString());
        rows.append(r);
    }
    obj.insert(QStringLiteral("rows"), rows);
    obj.insert(QStringLiteral("interval"),
               m_intervalEdit != nullptr ? m_intervalEdit->text() : QString());
    return obj;
}

void ScopeRegisterPanel::fromJson(const QJsonObject &obj) {
    if (obj.contains(QStringLiteral("rows"))) {
        const QJsonArray rows = obj.value(QStringLiteral("rows")).toArray();
        const int count = qMin(rows.size(), RowCount);
        for (int row = 0; row < count; ++row) {
            const QJsonObject r = rows.at(row).toObject();
            if (m_descEdits[row] != nullptr && r.contains(QStringLiteral("desc")))
                m_descEdits[row]->setText(r.value(QStringLiteral("desc")).toString());
            if (m_addrEdits[row] != nullptr && r.contains(QStringLiteral("addr")))
                m_addrEdits[row]->setText(r.value(QStringLiteral("addr")).toString());
            if (m_valueEdits[row] != nullptr && r.contains(QStringLiteral("val")))
                m_valueEdits[row]->setText(r.value(QStringLiteral("val")).toString());
        }
    }
    if (obj.contains(QStringLiteral("interval")) && m_intervalEdit != nullptr) {
        m_intervalEdit->setText(obj.value(QStringLiteral("interval")).toString());
    }
}

// =============================================================================
// 信号槽连接
// =============================================================================

/// @brief 连接 8 行 R/W 按钮和底部控制按钮的信号。
void ScopeRegisterPanel::connectSignals() {
    // ---------- 8 行读写按钮 ----------
    for (int row = 0; row < RowCount; ++row) {
        // 读按钮：清除错误状态后发射 readRequested
        connect(m_readButtons[row], &QPushButton::clicked, this, [this, row]() {
            setAddressError(row, false);
            emit readRequested(row);
        });
        // 写按钮：清除错误状态后发射 writeRequested
        connect(m_writeButtons[row], &QPushButton::clicked, this, [this, row]() {
            setAddressError(row, false);
            emit writeRequested(row);
        });
        // 数值框：离开输入即校验，非法标红
        connect(m_valueEdits[row], &QLineEdit::editingFinished, this, [this, row]() {
            validateValueRow(row);
        });
    }

    // ---------- 底部控制按钮 ----------
    connect(m_startButton, &QPushButton::clicked, this, &ScopeRegisterPanel::startRequested);
    connect(m_stopButton, &QPushButton::clicked, this, &ScopeRegisterPanel::stopRequested);
    connect(m_clearButton, &QPushButton::clicked, this, &ScopeRegisterPanel::clearPanelRequested);
    connect(m_formatToggleButton, &QPushButton::clicked, this, &ScopeRegisterPanel::toggleValueFormat);
}

// =============================================================================
// UI 构建
// =============================================================================

/// @brief 创建 8 行寄存器输入控件和底部控制栏。
void ScopeRegisterPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeRegisterPanel"));
    setMinimumSize(QSize(Style::Size::ScopeRegPanelMinW, Style::Size::ScopeRegPanelMinH));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(4);
    rootLayout->setSizeConstraint(QLayout::SetMinimumSize);
    rootLayout->setContentsMargins(6, 6, 6, 6);

    // ---------- 8 行寄存器控件 ----------
    for (int row = 0; row < RowCount; ++row) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(QStringLiteral("row%1Layout").arg(row));
        rowLayout->setSpacing(8);
        // 列宽比例：描述(3) : 地址(2) : 值(2) : 按钮固定
        rowLayout->setStretch(0, 3);
        rowLayout->setStretch(1, 2);
        rowLayout->setStretch(2, 2);
        rootLayout->addLayout(rowLayout);

        // --- 描述输入框 ---
        m_descEdits[row] = new QLineEdit(this);
        m_descEdits[row]->setObjectName(QStringLiteral("descEdit%1").arg(row));
        m_descEdits[row]->setMinimumSize(QSize(Style::Size::ScopeRegDescWidth, Style::Size::ScopeRegRowHeight));
        m_descEdits[row]->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::ScopeRegRowHeight));
        m_descEdits[row]->setText(QStringLiteral("REG%1").arg(row + 1));
        m_descEdits[row]->setProperty("inputRole", QStringLiteral("scope-register"));
        rowLayout->addWidget(m_descEdits[row]);

        // --- 地址输入框：默认从 0x0020 开始递增 ---
        m_addrEdits[row] = new QLineEdit(this);
        m_addrEdits[row]->setObjectName(QStringLiteral("addrEdit%1").arg(row));
        m_addrEdits[row]->setMinimumSize(QSize(Style::Size::ScopeRegInputWidth, Style::Size::ScopeRegRowHeight));
        m_addrEdits[row]->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::ScopeRegRowHeight));
        m_addrEdits[row]->setText(QStringLiteral("0x%1").arg(0x0020 + row * 2, 4, 16, QLatin1Char('0')).toUpper());
        m_addrEdits[row]->setProperty("inputRole", QStringLiteral("scope-register"));
        rowLayout->addWidget(m_addrEdits[row]);

        // --- 值输入框 ---
        m_valueEdits[row] = new QLineEdit(this);
        m_valueEdits[row]->setObjectName(QStringLiteral("valueEdit%1").arg(row));
        m_valueEdits[row]->setMinimumSize(QSize(Style::Size::ScopeRegInputWidth, Style::Size::ScopeRegRowHeight));
        m_valueEdits[row]->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::ScopeRegRowHeight));
        m_valueEdits[row]->setText(QStringLiteral("0x0000"));
        m_valueEdits[row]->setProperty("inputRole", QStringLiteral("scope-register"));
        rowLayout->addWidget(m_valueEdits[row]);

        // --- 读按钮（R） ---
        m_readButtons[row] = new QPushButton(this);
        m_readButtons[row]->setObjectName(QStringLiteral("readButton%1").arg(row));
        m_readButtons[row]->setMinimumSize(QSize(Style::Size::ScopeRegButtonWidth, Style::Size::ScopeRegRowHeight));
        m_readButtons[row]->setMaximumSize(QSize(Style::Size::ScopeRegButtonWidth, Style::Size::ScopeRegRowHeight));
        m_readButtons[row]->setProperty("buttonRole", QStringLiteral("read"));
        m_readButtons[row]->setText(tr("读"));
        rowLayout->addWidget(m_readButtons[row]);

        // --- 写按钮（W） ---
        m_writeButtons[row] = new QPushButton(this);
        m_writeButtons[row]->setObjectName(QStringLiteral("writeButton%1").arg(row));
        m_writeButtons[row]->setMinimumSize(QSize(Style::Size::ScopeRegButtonWidth, Style::Size::ScopeRegRowHeight));
        m_writeButtons[row]->setMaximumSize(QSize(Style::Size::ScopeRegButtonWidth, Style::Size::ScopeRegRowHeight));
        m_writeButtons[row]->setProperty("buttonRole", QStringLiteral("write"));
        m_writeButtons[row]->setText(tr("写"));
        rowLayout->addWidget(m_writeButtons[row]);
    }

    // ---------- 底部控制栏：间隔 + 启停 + 清除/录入 ----------
    auto *intervalRow = new QHBoxLayout();
    intervalRow->setObjectName(QStringLiteral("intervalRow"));
    intervalRow->setSpacing(6);
    intervalRow->setContentsMargins(0, 10, 0, 0);       // 与上方行间留出间距
    rootLayout->addLayout(intervalRow);

    m_intervalLabel = new QLabel(this);
    m_intervalLabel->setObjectName(QStringLiteral("intervalLabel"));
    intervalRow->addWidget(m_intervalLabel);

    // --- 间隔输入框 ---
    m_intervalEdit = new QLineEdit(this);
    m_intervalEdit->setObjectName(QStringLiteral("intervalEdit"));
    m_intervalEdit->setMinimumSize(QSize(68, 0));
    m_intervalEdit->setMaximumSize(QSize(68, QWIDGETSIZE_MAX));
    m_intervalEdit->setPlaceholderText(QStringLiteral("100"));
    m_intervalEdit->setProperty("inputRole", QStringLiteral("scope-register"));
    intervalRow->addWidget(m_intervalEdit);

    // --- 启动按钮 ---
    m_startButton = new QPushButton(this);
    m_startButton->setObjectName(QStringLiteral("startButton"));
    m_startButton->setProperty("buttonRole", QStringLiteral("action-start"));
    m_startButton->setText(tr("启动"));
    intervalRow->addWidget(m_startButton);

    // --- 停止按钮 ---
    m_stopButton = new QPushButton(this);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_stopButton->setProperty("buttonRole", QStringLiteral("action-stop"));
    m_stopButton->setText(tr("停止"));
    intervalRow->addWidget(m_stopButton);

    intervalRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // --- 清除面板按钮 ---
    m_clearButton = new QPushButton(this);
    m_clearButton->setObjectName(QStringLiteral("clearButton"));
    m_clearButton->setProperty("buttonRole", QStringLiteral("action-clear"));
    m_clearButton->setText(tr("清除面板"));
    intervalRow->addWidget(m_clearButton);

    // --- HEX/DEC 显示切换按钮（替换原"录入参数"）---
    m_formatToggleButton = new QPushButton(this);
    m_formatToggleButton->setObjectName(QStringLiteral("formatToggleButton"));
    m_formatToggleButton->setProperty("buttonRole", QStringLiteral("action-format"));
    m_formatToggleButton->setText(QStringLiteral("HEX"));    // 默认 HEX 模式（与 m_hexMode 一致）
    intervalRow->addWidget(m_formatToggleButton);

    retranslateUi();
}

// =============================================================================
// 语言切换 / 文字重设
// =============================================================================

/// @brief 语言切换时刷新所有可见文字。
void ScopeRegisterPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

/// @brief 重设 8 行 R/W 按钮、间隔标签与底部控制按钮文字（HEX/DEC 为保留术语不译）。
void ScopeRegisterPanel::retranslateUi() {
    for (int row = 0; row < RowCount; ++row) {
        if (m_readButtons[row] != nullptr) {
            m_readButtons[row]->setText(tr("读"));
        }
        if (m_writeButtons[row] != nullptr) {
            m_writeButtons[row]->setText(tr("写"));
        }
    }
    if (m_intervalLabel != nullptr) {
        m_intervalLabel->setText(tr("下发时间间隔"));
    }
    if (m_startButton != nullptr) {
        m_startButton->setText(tr("启动"));
    }
    if (m_stopButton != nullptr) {
        m_stopButton->setText(tr("停止"));
    }
    if (m_clearButton != nullptr) {
        m_clearButton->setText(tr("清除面板"));
    }
}
