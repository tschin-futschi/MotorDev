// =============================================================================
// @file    scopegeneratorpanel.cpp
// @brief   波形生成器面板实现 — 线性/余弦参数验证、启停控制
//
// 本面板提供两种波形生成模式的参数输入界面：
//
// 1. 线性模式（Linear）：
//    - 参数：寄存器地址、最小值、最大值、步长、间隔(ms)
//    - 验证：max > min, step > 0, interval > 0
//    - 信号：linearStartRequested(addr, min, max, step, intervalMs)
//
// 2. 余弦模式（Cosine）：
//    - 全局参数：振幅、偏移、频率(Hz)
//    - 通道参数（最多 3 通道）：寄存器地址、相位(度)
//    - 默认相位偏移：CH1=0°, CH2=120°, CH3=240°（典型三相配置）
//    - 验证：amplitude > 0, frequency > 0, 至少一个通道有地址
//    - 信号：cosineStartRequested(amplitude, offset, freqHz, channels)
//
// UI 布局：
//   [标题: "Wave Generator"]
//   [模式选择: ○Linear  ○Cosine]
//   [QStackedWidget]
//     ├─ Page 0: 线性参数表单（QFormLayout）
//     └─ Page 1: 余弦参数网格（QGridLayout, 4 列）
//   [按钮行: ···弹簧··· [Start] [Stop]]
//
// 字段验证机制：
//   - 每个输入字段通过 hasError QSS 动态属性标记错误状态
//   - 点击 Start 时先 clearErrors()，再逐字段验证
//   - 验证失败的字段通过 setFieldError() 标红并 repolish
//   - 所有字段验证通过后才发射对应的 start 信号
// =============================================================================
#include "widgets/scopegeneratorpanel.h"

#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {

/// @brief 工厂函数：创建带统一 QSS 属性的生成器参数输入框。
///
/// @param parent       父控件
/// @param objectName   对象名（用于 QSS 选择器和调试）
/// @param placeholder  占位提示文本
/// @return 新创建的 QLineEdit 指针
QLineEdit *createGeneratorEdit(QWidget *parent, const QString &objectName, const QString &placeholder) {
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(objectName);
    edit->setProperty("inputRole", QStringLiteral("scope-generator"));
    edit->setPlaceholderText(placeholder);
    return edit;
}

}

// =============================================================================
// 构造 / 析构
// =============================================================================

/// @brief 构造生成器面板：搭建 UI → 连接信号 → 设置初始停止状态。
ScopeGeneratorPanel::ScopeGeneratorPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    connectSignals();
    setRunning(false);                                  // 初始状态：Start 可用，Stop 禁用
}

ScopeGeneratorPanel::~ScopeGeneratorPanel() = default;

// =============================================================================
// 公共接口
// =============================================================================

/// @brief 切换运行/停止状态，同步禁用/启用对应控件。
///
/// 运行时：禁用模式选择按钮和 Start，启用 Stop。
/// 停止时：启用模式选择按钮和 Start，禁用 Stop。
void ScopeGeneratorPanel::setRunning(bool running) {
    // 禁用/启用模式切换按钮
    if (m_modeGroup != nullptr) {
        const auto buttons = m_modeGroup->buttons();
        for (QAbstractButton *button : buttons) {
            if (button != nullptr) {
                button->setEnabled(!running);
            }
        }
    }

    if (m_startButton != nullptr) {
        m_startButton->setEnabled(!running);
    }
    if (m_stopButton != nullptr) {
        m_stopButton->setEnabled(running);
    }
}

// =============================================================================
// UI 构建
// =============================================================================

/// @brief 创建生成器面板的完整 UI 层级。
///
/// 结构：
///   QVBoxLayout(rootLayout, spacing=8, margins=10)
///     ├─ QLabel("Wave Generator")
///     ├─ QHBoxLayout(modeLayout)  → ○Linear ○Cosine
///     ├─ QStackedWidget(modeStack, stretch=1)
///     │    ├─ linearPage (QFormLayout, 5 行)
///     │    └─ cosinePage (QGridLayout, 5 行 × 4 列)
///     └─ QHBoxLayout(buttonLayout) → [弹簧] [Start] [Stop]
void ScopeGeneratorPanel::setupUi() {
    setObjectName(QStringLiteral("ScopeGeneratorPanel"));
    setMinimumSize(QSize(Style::Size::GeneratorPanelMinW, Style::Size::GeneratorPanelMinH));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(8);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    // --- 标题 ---
    auto *titleLabel = new QLabel(tr("Wave Generator"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    rootLayout->addWidget(titleLabel);

    // --- 模式选择（互斥 RadioButton 组） ---
    auto *modeLayout = new QHBoxLayout();
    modeLayout->setObjectName(QStringLiteral("modeLayout"));
    modeLayout->setSpacing(12);
    rootLayout->addLayout(modeLayout);

    m_modeGroup = new QButtonGroup(this);
    m_linearRadio = new QRadioButton(tr("Linear"), this);
    m_linearRadio->setObjectName(QStringLiteral("linearRadio"));
    m_linearRadio->setChecked(true);                    // 默认选中线性模式
    m_cosineRadio = new QRadioButton(tr("Cosine"), this);
    m_cosineRadio->setObjectName(QStringLiteral("cosineRadio"));
    m_sawtoothRadio = new QRadioButton(tr("Sawtooth"), this);
    m_sawtoothRadio->setObjectName(QStringLiteral("sawtoothRadio"));
    m_modeGroup->addButton(m_linearRadio, 0);           // ID 0 = Linear
    m_modeGroup->addButton(m_cosineRadio, 1);           // ID 1 = Cosine
    m_modeGroup->addButton(m_sawtoothRadio, 2);         // ID 2 = Sawtooth
    modeLayout->addWidget(m_linearRadio);
    modeLayout->addWidget(m_cosineRadio);
    modeLayout->addWidget(m_sawtoothRadio);
    modeLayout->addStretch(1);

    // --- 参数页面栈 ---
    m_modeStack = new QStackedWidget(this);
    m_modeStack->setObjectName(QStringLiteral("modeStack"));
    rootLayout->addWidget(m_modeStack, 1);              // stretch=1，占满剩余空间

    // ===== 线性模式参数页 =====
    auto *linearPage = new QWidget(m_modeStack);
    linearPage->setObjectName(QStringLiteral("linearPage"));
    auto *linearLayout = new QFormLayout(linearPage);
    linearLayout->setObjectName(QStringLiteral("linearLayout"));
    linearLayout->setSpacing(8);
    linearLayout->setContentsMargins(0, 0, 0, 0);

    m_linearAddrEdit = createGeneratorEdit(linearPage, QStringLiteral("linearAddrEdit"), QStringLiteral("0x0020"));
    m_linearMinEdit = createGeneratorEdit(linearPage, QStringLiteral("linearMinEdit"), QStringLiteral("-100"));
    m_linearMaxEdit = createGeneratorEdit(linearPage, QStringLiteral("linearMaxEdit"), QStringLiteral("100"));
    m_linearStepEdit = createGeneratorEdit(linearPage, QStringLiteral("linearStepEdit"), QStringLiteral("5"));
    m_linearIntervalEdit = createGeneratorEdit(linearPage, QStringLiteral("linearIntervalEdit"), QStringLiteral("100"));

    linearLayout->addRow(tr("Register Addr"), m_linearAddrEdit);
    linearLayout->addRow(tr("Min"), m_linearMinEdit);
    linearLayout->addRow(tr("Max"), m_linearMaxEdit);
    linearLayout->addRow(tr("Step"), m_linearStepEdit);
    linearLayout->addRow(tr("Interval (ms)"), m_linearIntervalEdit);
    m_modeStack->addWidget(linearPage);

    // ===== 余弦模式参数页 =====
    // 布局为 4 列网格：[标签] [输入] [标签] [输入]
    // 行 0：振幅 + 偏移（全局参数）
    // 行 1：频率（全局参数，仅占左半）
    // 行 2~4：CH1~CH3 的地址 + 相位（通道参数）
    auto *cosinePage = new QWidget(m_modeStack);
    cosinePage->setObjectName(QStringLiteral("cosinePage"));
    auto *cosineLayout = new QGridLayout(cosinePage);
    cosineLayout->setObjectName(QStringLiteral("cosineLayout"));
    cosineLayout->setSpacing(8);
    cosineLayout->setContentsMargins(0, 0, 0, 0);
    cosineLayout->setColumnStretch(1, 1);               // 左侧输入列弹性
    cosineLayout->setColumnStretch(3, 1);               // 右侧输入列弹性

    // 辅助 lambda：在一行中创建左右两组 [标签 + 输入框]
    auto addGlobalRow = [cosinePage, cosineLayout](int row, const QString &leftLabel, QLineEdit **leftEdit,
                                                   const QString &leftName, const QString &leftPlaceholder,
                                                   const QString &rightLabel, QLineEdit **rightEdit,
                                                   const QString &rightName, const QString &rightPlaceholder) {
        auto *lLabel = new QLabel(leftLabel, cosinePage);
        auto *rLabel = new QLabel(rightLabel, cosinePage);
        *leftEdit = createGeneratorEdit(cosinePage, leftName, leftPlaceholder);
        *rightEdit = createGeneratorEdit(cosinePage, rightName, rightPlaceholder);
        cosineLayout->addWidget(lLabel, row, 0);
        cosineLayout->addWidget(*leftEdit, row, 1);
        cosineLayout->addWidget(rLabel, row, 2);
        cosineLayout->addWidget(*rightEdit, row, 3);
    };

    // 行 0：振幅 + 偏移
    addGlobalRow(0, tr("Amplitude"), &m_cosineAmplitudeEdit, QStringLiteral("cosineAmplitudeEdit"), QStringLiteral("1000"),
                 tr("Offset"), &m_cosineOffsetEdit, QStringLiteral("cosineOffsetEdit"), QStringLiteral("0"));

    // 行 1：频率（仅左半列）
    auto *freqLabel = new QLabel(tr("Freq (Hz)"), cosinePage);
    m_cosineFrequencyEdit = createGeneratorEdit(cosinePage, QStringLiteral("cosineFrequencyEdit"), QStringLiteral("1.0"));
    cosineLayout->addWidget(freqLabel, 1, 0);
    cosineLayout->addWidget(m_cosineFrequencyEdit, 1, 1);

    // 行 2~4：3 个通道的地址 + 相位
    // 默认占位：CH1=0°, CH2=120°, CH3=240°（三相电机典型配置）
    for (int channel = 0; channel < 3; ++channel) {
        auto *addrLabel = new QLabel(tr("CH%1 Addr").arg(channel + 1), cosinePage);
        auto *phaseLabel = new QLabel(tr("CH%1 Phase (deg)").arg(channel + 1), cosinePage);
        m_cosineAddrEdits[channel] = createGeneratorEdit(
            cosinePage, QStringLiteral("cosineAddrEdit%1").arg(channel), channel == 0 ? QStringLiteral("0x0020") : QString());
        const QString phasePlaceholder = (channel == 0) ? QStringLiteral("0")
                                       : (channel == 1) ? QStringLiteral("120")
                                                        : QStringLiteral("240");
        m_cosinePhaseEdits[channel] = createGeneratorEdit(
            cosinePage, QStringLiteral("cosinePhaseEdit%1").arg(channel), phasePlaceholder);
        cosineLayout->addWidget(addrLabel, channel + 2, 0);
        cosineLayout->addWidget(m_cosineAddrEdits[channel], channel + 2, 1);
        cosineLayout->addWidget(phaseLabel, channel + 2, 2);
        cosineLayout->addWidget(m_cosinePhaseEdits[channel], channel + 2, 3);
    }
    m_modeStack->addWidget(cosinePage);

    // ===== 锯齿波测试模式参数页 =====
    auto *sawtoothPage = new QWidget(m_modeStack);
    sawtoothPage->setObjectName(QStringLiteral("sawtoothPage"));
    auto *sawtoothLayout = new QFormLayout(sawtoothPage);
    sawtoothLayout->setObjectName(QStringLiteral("sawtoothLayout"));
    sawtoothLayout->setSpacing(8);
    sawtoothLayout->setContentsMargins(0, 0, 0, 0);

    m_sawtoothAddrEdit = createGeneratorEdit(sawtoothPage, QStringLiteral("sawtoothAddrEdit"), QStringLiteral("0x0020"));
    m_sawtoothMinEdit = createGeneratorEdit(sawtoothPage, QStringLiteral("sawtoothMinEdit"), QStringLiteral("-100"));
    m_sawtoothMaxEdit = createGeneratorEdit(sawtoothPage, QStringLiteral("sawtoothMaxEdit"), QStringLiteral("100"));
    m_sawtoothStepEdit = createGeneratorEdit(sawtoothPage, QStringLiteral("sawtoothStepEdit"), QStringLiteral("1"));

    sawtoothLayout->addRow(tr("Register Addr"), m_sawtoothAddrEdit);
    sawtoothLayout->addRow(tr("Min"), m_sawtoothMinEdit);
    sawtoothLayout->addRow(tr("Max"), m_sawtoothMaxEdit);
    sawtoothLayout->addRow(tr("Step"), m_sawtoothStepEdit);
    m_modeStack->addWidget(sawtoothPage);

    // --- 底部按钮行 ---
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setObjectName(QStringLiteral("buttonLayout"));
    buttonLayout->addStretch(1);                        // 左侧弹簧，按钮靠右
    rootLayout->addLayout(buttonLayout);

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setObjectName(QStringLiteral("startButton"));
    m_startButton->setProperty("buttonRole", QStringLiteral("generator-start"));
    buttonLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_stopButton->setProperty("buttonRole", QStringLiteral("generator-stop"));
    buttonLayout->addWidget(m_stopButton);
}

// =============================================================================
// 信号槽连接
// =============================================================================

/// @brief 连接模式切换、启动和停止按钮的信号。
///
/// - Linear / Cosine RadioButton 切换 → QStackedWidget 翻页
/// - Start 按钮 → handleStartClicked（根据当前模式分发）
/// - Stop 按钮 → stopRequested 信号（直接转发）
void ScopeGeneratorPanel::connectSignals() {
    connect(m_linearRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_modeStack != nullptr) {
            m_modeStack->setCurrentIndex(0);            // 切换到线性参数页
        }
    });
    connect(m_cosineRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_modeStack != nullptr) {
            m_modeStack->setCurrentIndex(1);            // 切换到余弦参数页
        }
    });
    connect(m_sawtoothRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_modeStack != nullptr) {
            m_modeStack->setCurrentIndex(2);            // 切换到锯齿波参数页
        }
    });
    connect(m_startButton, &QPushButton::clicked, this, &ScopeGeneratorPanel::handleStartClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &ScopeGeneratorPanel::stopRequested);
}

// =============================================================================
// 字段验证工具
// =============================================================================

/// @brief 设置或清除输入字段的错误状态。
///
/// 通过 QSS 动态属性 "hasError" 控制字段样式（红色边框等），
/// 修改属性后调用 repolish 立即刷新样式。
void ScopeGeneratorPanel::setFieldError(QLineEdit *edit, bool error) {
    if (edit == nullptr) {
        return;
    }
    edit->setProperty("hasError", error);
    MotorDev::UiUtil::repolish(edit);
}

/// @brief 解析 uint16 字段（支持十进制和 0x 前缀十六进制）。
///
/// @param edit       输入控件
/// @param out        输出值指针
/// @param allowEmpty 是否允许空值（余弦模式中非必填通道使用）
/// @return 解析成功返回 true；失败时标记字段错误并返回 false
bool ScopeGeneratorPanel::parseUint16Field(QLineEdit *edit, quint16 *out, bool allowEmpty) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    if (trimmed.isEmpty()) {
        setFieldError(edit, !allowEmpty);
        return allowEmpty;                              // 空值：根据 allowEmpty 决定
    }

    bool ok = false;
    uint value = trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)
                     ? trimmed.mid(2).toUInt(&ok, 16)   // 十六进制：跳过 "0x" 前缀
                     : trimmed.toUInt(&ok, 10);          // 十进制
    const bool valid = ok && value <= 0xFFFFu;          // 范围检查：0~65535
    setFieldError(edit, !valid);
    if (valid) {
        *out = static_cast<quint16>(value);
    }
    return valid;
}

/// @brief 解析 int16 字段（十进制，范围 -32768~32767）。
bool ScopeGeneratorPanel::parseInt16Field(QLineEdit *edit, qint16 *out) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    bool ok = false;
    const int value = trimmed.toInt(&ok, 10);
    const bool valid = ok && value >= -32768 && value <= 32767;
    setFieldError(edit, !valid);
    if (valid) {
        *out = static_cast<qint16>(value);
    }
    return valid;
}

/// @brief 解析正整数字段（> 0，用于间隔等参数）。
bool ScopeGeneratorPanel::parsePositiveIntField(QLineEdit *edit, int *out) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    bool ok = false;
    const int value = trimmed.toInt(&ok, 10);
    const bool valid = ok && value > 0;
    setFieldError(edit, !valid);
    if (valid) {
        *out = value;
    }
    return valid;
}

/// @brief 解析浮点数字段。
///
/// @param edit           输入控件
/// @param out            输出值指针
/// @param mustBePositive 是否要求 > 0（频率字段使用，相位字段不使用）
/// @return 解析成功返回 true
bool ScopeGeneratorPanel::parseDoubleField(QLineEdit *edit, double *out, bool mustBePositive) {
    if (edit == nullptr || out == nullptr) {
        return false;
    }

    const QString trimmed = edit->text().trimmed();
    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    const bool valid = ok && (!mustBePositive || value > 0.0);
    setFieldError(edit, !valid);
    if (valid) {
        *out = value;
    }
    return valid;
}

// =============================================================================
// 启动分发
// =============================================================================

/// @brief Start 按钮点击处理：清除所有错误标记，根据当前模式分发到对应处理函数。
void ScopeGeneratorPanel::handleStartClicked() {
    clearErrors();
    if (m_linearRadio != nullptr && m_linearRadio->isChecked()) {
        handleLinearStart();
        return;
    }
    if (m_sawtoothRadio != nullptr && m_sawtoothRadio->isChecked()) {
        handleSawtoothStart();
        return;
    }
    handleCosineStart();
}

/// @brief 线性模式启动：解析并验证 5 个参数字段，通过后发射 linearStartRequested。
///
/// 验证规则：
///   1. 地址必须为有效 uint16
///   2. min / max / step 必须为有效 int16
///   3. interval 必须为正整数
///   4. max > min（否则同时标红 min 和 max）
///   5. step > 0（否则标红 step）
void ScopeGeneratorPanel::handleLinearStart() {
    quint16 addr = 0;
    qint16 minValue = 0;
    qint16 maxValue = 0;
    qint16 step = 0;
    int intervalMs = 0;

    // 逐字段解析（短路求值：首个失败即停止后续解析）
    const bool ok = parseUint16Field(m_linearAddrEdit, &addr)
                    && parseInt16Field(m_linearMinEdit, &minValue)
                    && parseInt16Field(m_linearMaxEdit, &maxValue)
                    && parseInt16Field(m_linearStepEdit, &step)
                    && parsePositiveIntField(m_linearIntervalEdit, &intervalMs);
    if (!ok) {
        return;
    }

    // 语义验证：max 必须大于 min
    if (maxValue <= minValue) {
        setFieldError(m_linearMinEdit, true);
        setFieldError(m_linearMaxEdit, true);
        return;
    }
    // 语义验证：步长必须为正
    if (step <= 0) {
        setFieldError(m_linearStepEdit, true);
        return;
    }

    emit linearStartRequested(addr, minValue, maxValue, step, intervalMs);
}

/// @brief 余弦模式启动：解析全局参数和通道参数，通过后发射 cosineStartRequested。
///
/// 验证规则：
///   1. 振幅 > 0
///   2. 频率 > 0
///   3. 至少一个通道填写了地址（空地址通道跳过）
///   4. 已填地址的通道，地址和相位都必须有效
void ScopeGeneratorPanel::handleCosineStart() {
    qint16 amplitude = 0;
    qint16 offset = 0;
    double frequencyHz = 0.0;

    // --- 全局参数验证 ---
    const bool globalsOk = parseInt16Field(m_cosineAmplitudeEdit, &amplitude)
                           && parseInt16Field(m_cosineOffsetEdit, &offset)
                           && parseDoubleField(m_cosineFrequencyEdit, &frequencyHz, true);
    if (!globalsOk) {
        return;
    }
    if (amplitude <= 0) {
        setFieldError(m_cosineAmplitudeEdit, true);
        return;
    }

    // --- 通道参数收集 ---
    QVector<ScopeGeneratorCosineChannel> channels;
    channels.reserve(3);
    bool hasChannel = false;                            ///< 至少一个通道有效
    bool valid = true;                                  ///< 所有已填通道均有效
    for (int i = 0; i < 3; ++i) {
        const QString addrText = m_cosineAddrEdits[i]->text().trimmed();
        const QString phaseText = m_cosinePhaseEdits[i]->text().trimmed();
        if (addrText.isEmpty()) {
            // 地址为空 → 此通道跳过，清除可能残留的错误标记
            setFieldError(m_cosineAddrEdits[i], false);
            setFieldError(m_cosinePhaseEdits[i], false);
            continue;
        }

        quint16 addr = 0;
        double phaseDeg = 0.0;
        const bool channelOk = parseUint16Field(m_cosineAddrEdits[i], &addr)
                               && parseDoubleField(m_cosinePhaseEdits[i], &phaseDeg, false);
        valid = valid && channelOk;
        if (channelOk) {
            hasChannel = true;
            channels.push_back({addr, phaseDeg});
        }
    }

    if (!valid || !hasChannel) {
        if (!hasChannel) {
            // 没有任何通道填写地址 → 标红 CH1 地址字段提示用户
            setFieldError(m_cosineAddrEdits[0], true);
        }
        return;
    }

    emit cosineStartRequested(amplitude, offset, frequencyHz, channels);
}

/// @brief 锯齿波测试模式启动：解析并验证 4 个参数字段，通过后发射 sawtoothStartRequested。
///
/// 验证规则：
///   1. 地址必须为有效 uint16
///   2. min / max / step 必须为有效 int16
///   3. max > min（否则同时标红 min 和 max）
///   4. step > 0（否则标红 step）
void ScopeGeneratorPanel::handleSawtoothStart() {
    quint16 addr = 0;
    qint16 minValue = 0;
    qint16 maxValue = 0;
    qint16 step = 0;

    const bool ok = parseUint16Field(m_sawtoothAddrEdit, &addr)
                    && parseInt16Field(m_sawtoothMinEdit, &minValue)
                    && parseInt16Field(m_sawtoothMaxEdit, &maxValue)
                    && parseInt16Field(m_sawtoothStepEdit, &step);
    if (!ok) {
        return;
    }

    if (maxValue <= minValue) {
        setFieldError(m_sawtoothMinEdit, true);
        setFieldError(m_sawtoothMaxEdit, true);
        return;
    }
    if (step <= 0) {
        setFieldError(m_sawtoothStepEdit, true);
        return;
    }

    emit sawtoothStartRequested(addr, minValue, maxValue, step);
}

// =============================================================================
// 错误清除
// =============================================================================

/// @brief 清除面板内所有 QLineEdit 的错误状态。
///
/// 通过 findChildren 遍历所有 QLineEdit 子控件，
/// 将 hasError 属性重置为 false 并 repolish。
/// 在每次点击 Start 时调用，确保上次的错误标记不残留。
void ScopeGeneratorPanel::clearErrors() {
    const QList<QLineEdit *> edits = findChildren<QLineEdit *>();
    for (QLineEdit *edit : edits) {
        setFieldError(edit, false);
    }
}
