// =============================================================================
// @file    serialdebugtab.cpp
// @brief   串口调试模拟器页面实现 — UI 构建、信号槽连接、日志格式化
// =============================================================================
#include "tabs/serialdebugtab.h"

#include "services/simulatorservice.h"
#include "services/simulatorserial.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QByteArray>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
/// @brief 格式化单字节为 "0x##" 大写十六进制字符串
QString formatByte(uint8_t value) { return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(); }
}

// =============================================================================
// 构造 / 析构
// =============================================================================

SerialDebugTab::SerialDebugTab(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowCloseButtonHint)
{
    setWindowTitle(tr("串口调试模拟器"));
    resize(820, 560);

    m_service = new SimulatorService(this);
    setupUi();

    // 初始化控件默认值
    m_portCombo->setPlaceholderText(tr("选择端口"));
    m_baudCombo->setCurrentText(QLatin1String(Style::Serial::DefaultBaudRate));
    m_delaySpinBox->setValue(0);

    connectSignals();
    refreshPortList();
    setConnectedState(false);
}

SerialDebugTab::~SerialDebugTab() = default;

// =============================================================================
// 信号槽连接
// =============================================================================

void SerialDebugTab::connectSignals() {
    // -------------------------------------------------------------------------
    // UI → Service：用户操作
    // -------------------------------------------------------------------------

    connect(m_scanButton, &QPushButton::clicked, this, &SerialDebugTab::refreshPortList);
    connect(m_clearLogButton, &QPushButton::clicked, m_logEdit, &QTextEdit::clear);

    // 寄存器读返回值变化 → 实时同步到 Service
    connect(m_regReadValueEdit, &QLineEdit::textChanged, this, [this]() {
        m_service->setRegisterReadValue(m_regReadValueEdit->text());
    });

    // 连接/断开串口
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_service->isConnected()) {
            m_service->disconnectPort();
            return;
        }
        const QString portName = m_portCombo->currentText().trimmed();
        if (portName.isEmpty()) {
            appendSysLog(tr("未选择串口"), true);
            return;
        }
        m_service->connectToPort(portName, m_baudCombo->currentText().toInt());
    });

    // 响应延迟 → Service
    connect(m_delaySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_service->setResponseDelay(value);
    });

    // 模拟参数配置 → Service
    connect(m_scanAddrEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_service->setScanAddresses(text);
    });
    connect(m_icAddrResultCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_service->setIcAddrResultSuccess(index == 0);
    });
    connect(m_writeResultCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_service->setWriteResultSuccess(index == 0);
    });

    // -------------------------------------------------------------------------
    // Service → UI：状态和日志回调
    // -------------------------------------------------------------------------

    connect(m_service, &SimulatorService::connected, this, [this]() { setConnectedState(true); });
    connect(m_service, &SimulatorService::disconnected, this, [this]() { setConnectedState(false); });
    connect(m_service, &SimulatorService::sysLogEntry, this, [this](const QString &message, bool isError) {
        appendSysLog(message, isError);
    });
    connect(m_service, &SimulatorService::logEntry, this, [this](const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note) {
        appendLog(dir, cmd, seq, data, note);
    });

    // -------------------------------------------------------------------------
    // Service → 外部：模拟波形数据转发给示波器（经 MainWindow 中继）
    // -------------------------------------------------------------------------

    connect(m_service, &SimulatorService::debugStreamGenerated, this, &SerialDebugTab::debugStreamGenerated);
    connect(m_service, &SimulatorService::debugStreamActiveChanged, this, &SerialDebugTab::debugStreamActiveChanged);
}

// =============================================================================
// 辅助方法
// =============================================================================

/// @brief 刷新串口列表，保留当前选中项
void SerialDebugTab::refreshPortList() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = SimulatorSerial::availablePorts();
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) {
        m_portCombo->setCurrentIndex(currentIndex);
    } else if (!currentPort.trimmed().isEmpty()) {
        m_portCombo->setCurrentText(currentPort);
    }
}

/// @brief 切换连接/未连接状态的控件外观和可用性
void SerialDebugTab::setConnectedState(bool connected) {
    m_connectButton->setText(connected ? tr("断开") : tr("连接"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_statusBadge->setText(connected ? tr("● 已连接") : tr("● 未连接"));
    m_statusBadge->setProperty("connected", connected);
    UiUtil::repolish(m_statusBadge);
}

/// @brief 追加系统日志行（带时间戳和颜色）
void SerialDebugTab::appendSysLog(const QString &message, bool isError) {
    const QString color = isError ? Style::Color::LogError.name() : Style::Color::MutedText.name();
    const QString line = QStringLiteral("[%1] SYS  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), message);
    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

/// @brief 追加帧日志行（格式：[时间] 方向 cmd=0x## seq=0x## 数据 (备注)）
///
/// TX 帧：蓝色（正常响应）或红色（错误帧 cmd=0xFE）
/// RX 帧：默认文本色
void SerialDebugTab::appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note) {
    QString color = Style::Color::AppText.name();
    if (dir == QStringLiteral("TX")) color = (cmd == 0xFE) ? Style::Color::LogError.name() : Style::Color::ReadButtonForeground.name();
    QString dataText;
    if (!data.isEmpty()) { QStringList bytes; bytes.reserve(data.size()); for (char byte : data) bytes.append(formatByte(static_cast<uint8_t>(byte))); dataText = bytes.join(QLatin1Char(' ')); }
    QString line = QStringLiteral("[%1] %2 cmd=%3 seq=%4").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"))).arg(dir).arg(formatByte(cmd)).arg(formatByte(seq));
    if (!dataText.isEmpty()) line.append(QStringLiteral("  %1").arg(dataText));
    if (!note.isEmpty()) line.append(QStringLiteral("  (%1)").arg(note));
    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

// =============================================================================
// UI 构建
// =============================================================================

void SerialDebugTab::setupUi() {
    setObjectName(QStringLiteral("SerialDebugTab"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(Style::Size::ContentSpacing);
    rootLayout->setContentsMargins(Style::Size::ContentPadding, Style::Size::ContentPadding,
                                   Style::Size::ContentPadding, Style::Size::ContentPadding);

    // =========================================================================
    // 顶部连接栏：Port | BaudRate | 刷新 | 连接 | 状态标签
    // =========================================================================
    auto *connectionBar = new QFrame(this);
    connectionBar->setObjectName(QStringLiteral("connectionBar"));
    connectionBar->setMinimumSize(QSize(0, Style::Size::SimulatorConnectionBarHeight));
    connectionBar->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::SimulatorConnectionBarHeight));
    auto *connectionLayout = new QHBoxLayout(connectionBar);
    connectionLayout->setObjectName(QStringLiteral("connectionLayout"));
    connectionLayout->setSpacing(10);
    connectionLayout->setContentsMargins(0, 0, 0, 1);
    rootLayout->addWidget(connectionBar);

    m_portLabel = new QLabel(connectionBar);
    m_portLabel->setObjectName(QStringLiteral("portLabel"));
    m_portLabel->setProperty("labelRole", QStringLiteral("form"));
    connectionLayout->addWidget(m_portLabel);

    m_portCombo = new QComboBox(connectionBar);
    m_portCombo->setObjectName(QStringLiteral("portCombo"));
    m_portCombo->setProperty("inputRole", QStringLiteral("form"));
    m_portCombo->setEditable(true);
    m_portCombo->setInsertPolicy(QComboBox::NoInsert);
    connectionLayout->addWidget(m_portCombo);

    m_baudLabel = new QLabel(connectionBar);
    m_baudLabel->setObjectName(QStringLiteral("baudLabel"));
    m_baudLabel->setProperty("labelRole", QStringLiteral("form"));
    connectionLayout->addWidget(m_baudLabel);

    m_baudCombo = new QComboBox(connectionBar);
    m_baudCombo->setObjectName(QStringLiteral("baudCombo"));
    m_baudCombo->setProperty("inputRole", QStringLiteral("form"));
    m_baudCombo->addItems(Style::Serial::baudRateLabels());
    connectionLayout->addWidget(m_baudCombo);

    m_scanButton = new QPushButton(connectionBar);
    m_scanButton->setObjectName(QStringLiteral("scanButton"));
    m_scanButton->setMinimumSize(QSize(0, Style::Size::SidebarButtonHeight));
    m_scanButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::SidebarButtonHeight));
    m_scanButton->setText(tr("刷新"));
    m_scanButton->setProperty("buttonRole", QStringLiteral("secondary"));
    connectionLayout->addWidget(m_scanButton);

    m_connectButton = new QPushButton(connectionBar);
    m_connectButton->setObjectName(QStringLiteral("connectButton"));
    m_connectButton->setMinimumSize(QSize(0, Style::Size::SidebarButtonHeight));
    m_connectButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::SidebarButtonHeight));
    m_connectButton->setText(tr("连接"));
    m_connectButton->setProperty("buttonRole", QStringLiteral("primary"));
    connectionLayout->addWidget(m_connectButton);

    m_statusBadge = new QLabel(connectionBar);
    m_statusBadge->setObjectName(QStringLiteral("statusBadge"));
    m_statusBadge->setText(tr("● 未连接"));
    m_statusBadge->setProperty("connected", false);
    connectionLayout->addWidget(m_statusBadge);
    connectionLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // =========================================================================
    // 主区域水平分割器：左侧应答配置 | 右侧活动日志
    // =========================================================================
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setObjectName(QStringLiteral("mainSplitter"));
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(Style::Size::SplitterHandleWidth);
    rootLayout->addWidget(m_mainSplitter);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);

    // --- 左侧应答配置面板 ---
    auto *leftPanel = new QWidget(m_mainSplitter);
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    leftPanel->setMinimumSize(QSize(Style::Size::SimulatorLeftPanelWidth, 0));
    leftPanel->setMaximumSize(QSize(Style::Size::SimulatorLeftPanelWidth, QWIDGETSIZE_MAX));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setObjectName(QStringLiteral("leftLayout"));
    leftLayout->setSpacing(Style::Size::ContentSpacing);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_responseTitle = new QLabel(leftPanel);
    m_responseTitle->setObjectName(QStringLiteral("responseTitle"));
    leftLayout->addWidget(m_responseTitle);

    m_responseForm = new QFormLayout();
    QFormLayout *responseForm = m_responseForm;
    responseForm->setObjectName(QStringLiteral("responseForm"));
    responseForm->setHorizontalSpacing(10);
    responseForm->setVerticalSpacing(10);
    leftLayout->addLayout(responseForm);

    // 表单标签工厂
    auto addFormLabel = [leftPanel, responseForm](const QString &objectName, const QString &text, int row) {
        auto *label = new QLabel(leftPanel);
        label->setObjectName(objectName);
        label->setText(text);
        label->setProperty("labelRole", QStringLiteral("form"));
        responseForm->setWidget(row, QFormLayout::LabelRole, label);
    };

    // I2C 扫描地址输入
    addFormLabel(QStringLiteral("scanAddrLabel"), tr("I2C扫描地址"), 0);
    m_scanAddrEdit = new QLineEdit(leftPanel);
    m_scanAddrEdit->setObjectName(QStringLiteral("scanAddrEdit"));
    m_scanAddrEdit->setText(QStringLiteral("0x5A"));
    m_scanAddrEdit->setProperty("inputRole", QStringLiteral("form"));
    responseForm->setWidget(0, QFormLayout::FieldRole, m_scanAddrEdit);

    // IC 连接结果（成功/失败）
    addFormLabel(QStringLiteral("icAddrResultLabel"), tr("IC连接"), 1);
    m_icAddrResultCombo = new QComboBox(leftPanel);
    m_icAddrResultCombo->setObjectName(QStringLiteral("icAddrResultCombo"));
    m_icAddrResultCombo->setProperty("inputRole", QStringLiteral("form"));
    m_icAddrResultCombo->addItem(tr("成功"));
    m_icAddrResultCombo->addItem(tr("失败"));
    responseForm->setWidget(1, QFormLayout::FieldRole, m_icAddrResultCombo);

    // 寄存器读返回值
    addFormLabel(QStringLiteral("regReadValueLabel"), tr("寄存器读返回值"), 2);
    m_regReadValueEdit = new QLineEdit(leftPanel);
    m_regReadValueEdit->setObjectName(QStringLiteral("regReadValueEdit"));
    m_regReadValueEdit->setText(QStringLiteral("0x0000"));
    m_regReadValueEdit->setProperty("inputRole", QStringLiteral("form"));
    responseForm->setWidget(2, QFormLayout::FieldRole, m_regReadValueEdit);

    // 寄存器写结果（ACK/失败）
    addFormLabel(QStringLiteral("writeResultLabel"), tr("寄存器写"), 3);
    m_writeResultCombo = new QComboBox(leftPanel);
    m_writeResultCombo->setObjectName(QStringLiteral("writeResultCombo"));
    m_writeResultCombo->setProperty("inputRole", QStringLiteral("form"));
    m_writeResultCombo->addItem(QStringLiteral("ACK"));
    m_writeResultCombo->addItem(tr("失败"));
    responseForm->setWidget(3, QFormLayout::FieldRole, m_writeResultCombo);

    // 响应延迟（0~500ms）
    addFormLabel(QStringLiteral("delayLabel"), tr("响应延迟(ms)"), 4);
    m_delaySpinBox = new QSpinBox(leftPanel);
    m_delaySpinBox->setObjectName(QStringLiteral("delaySpinBox"));
    m_delaySpinBox->setProperty("inputRole", QStringLiteral("form"));
    m_delaySpinBox->setMaximum(500);
    responseForm->setWidget(4, QFormLayout::FieldRole, m_delaySpinBox);
    leftLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // --- 右侧活动日志面板 ---
    auto *logPanel = new QWidget(m_mainSplitter);
    logPanel->setObjectName(QStringLiteral("logPanel"));
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setObjectName(QStringLiteral("logLayout"));
    logLayout->setSpacing(10);
    logLayout->setContentsMargins(0, 0, 0, 0);

    // 日志标题栏（标题 + 清除按钮）
    auto *logHeader = new QHBoxLayout();
    logHeader->setObjectName(QStringLiteral("logHeader"));
    logHeader->setSpacing(10);
    logHeader->setContentsMargins(0, 0, 0, 0);
    logLayout->addLayout(logHeader);

    m_logTitle = new QLabel(logPanel);
    m_logTitle->setObjectName(QStringLiteral("logTitle"));
    logHeader->addWidget(m_logTitle);
    logHeader->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_clearLogButton = new QPushButton(logPanel);
    m_clearLogButton->setObjectName(QStringLiteral("clearLogButton"));
    m_clearLogButton->setMinimumSize(QSize(0, 32));
    m_clearLogButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_clearLogButton->setText(tr("清除"));
    m_clearLogButton->setProperty("buttonRole", QStringLiteral("secondary"));
    logHeader->addWidget(m_clearLogButton);

    // 日志文本区（只读 HTML 富文本）
    m_logEdit = new QTextEdit(logPanel);
    m_logEdit->setObjectName(QStringLiteral("logEdit"));
    m_logEdit->setReadOnly(true);
    logLayout->addWidget(m_logEdit);

    retranslateUi();
}

// =============================================================================
// 语言切换 / 文字重设
// =============================================================================

void SerialDebugTab::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

/// @brief 重设窗口标题、连接栏、应答配置表单与日志区所有可见文字。
///
/// 应答配置表单标签经 QFormLayout::labelForField 取得后重设；两个下拉框按当前语言
/// 重排选项并保留选中项；连接按钮与状态徽标的动态文字交由 setConnectedState 按当前
/// 连接态重设（ACK 为保留术语不译）。
void SerialDebugTab::retranslateUi() {
    setWindowTitle(tr("串口调试模拟器"));

    if (m_portLabel != nullptr)     m_portLabel->setText(tr("端口："));
    if (m_baudLabel != nullptr)     m_baudLabel->setText(tr("波特率:"));
    if (m_scanButton != nullptr)    m_scanButton->setText(tr("刷新"));
    if (m_portCombo != nullptr)     m_portCombo->setPlaceholderText(tr("选择端口"));
    if (m_responseTitle != nullptr) m_responseTitle->setText(tr("应答配置"));
    if (m_logTitle != nullptr)      m_logTitle->setText(tr("活动日志"));
    if (m_clearLogButton != nullptr) m_clearLogButton->setText(tr("清除"));

    // 应答配置表单标签（经 labelForField 重设，避免逐个持有标签成员）
    auto setFormLabel = [this](QWidget *field, const QString &text) {
        if (m_responseForm == nullptr) {
            return;
        }
        if (auto *label = qobject_cast<QLabel *>(m_responseForm->labelForField(field))) {
            label->setText(text);
        }
    };
    setFormLabel(m_scanAddrEdit, tr("I2C扫描地址"));
    setFormLabel(m_icAddrResultCombo, tr("IC连接"));
    setFormLabel(m_regReadValueEdit, tr("寄存器读返回值"));
    setFormLabel(m_writeResultCombo, tr("寄存器写"));
    setFormLabel(m_delaySpinBox, tr("响应延迟(ms)"));

    // IC 连接结果下拉：成功 / 失败（重排保留选中项）
    if (m_icAddrResultCombo != nullptr) {
        const QSignalBlocker blocker(m_icAddrResultCombo);
        const int current = m_icAddrResultCombo->currentIndex();
        m_icAddrResultCombo->clear();
        m_icAddrResultCombo->addItem(tr("成功"));
        m_icAddrResultCombo->addItem(tr("失败"));
        m_icAddrResultCombo->setCurrentIndex(current < 0 ? 0 : current);
    }
    // 寄存器写结果下拉：ACK（保留术语）/ 失败
    if (m_writeResultCombo != nullptr) {
        const QSignalBlocker blocker(m_writeResultCombo);
        const int current = m_writeResultCombo->currentIndex();
        m_writeResultCombo->clear();
        m_writeResultCombo->addItem(QStringLiteral("ACK"));
        m_writeResultCombo->addItem(tr("失败"));
        m_writeResultCombo->setCurrentIndex(current < 0 ? 0 : current);
    }

    // 连接按钮 + 状态徽标的动态文字按当前连接态重设
    setConnectedState(m_service != nullptr && m_service->isConnected());
}
