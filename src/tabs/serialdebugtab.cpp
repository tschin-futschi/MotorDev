#include "tabs/serialdebugtab.h"

#include "services/simulatorservice.h"
#include "services/simulatorserial.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"

#include <QByteArray>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString formatByte(uint8_t value) { return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(); }
}

SerialDebugTab::SerialDebugTab(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowCloseButtonHint)
{
    setWindowTitle(tr("串口调试模拟器"));
    resize(820, 560);

    m_service = new SimulatorService(this);
    setupUi();

    m_portCombo->setPlaceholderText(tr("选择端口"));
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_delaySpinBox->setValue(0);

    connectSignals();
    refreshPortList();
    setConnectedState(false);
}

SerialDebugTab::~SerialDebugTab() = default;

void SerialDebugTab::connectSignals() {
    // UI → Service
    connect(m_scanButton, &QPushButton::clicked, this, &SerialDebugTab::refreshPortList);
    connect(m_clearLogButton, &QPushButton::clicked, m_logEdit, &QTextEdit::clear);
    connect(m_regReadValueEdit, &QLineEdit::textChanged, this, [this]() {
        m_service->setRegisterReadValue(m_regReadValueEdit->text());
    });
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
    connect(m_delaySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_service->setResponseDelay(value);
    });
    connect(m_scanAddrEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_service->setScanAddresses(text);
    });
    connect(m_icAddrResultCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_service->setIcAddrResultSuccess(index == 0);
    });
    connect(m_writeResultCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_service->setWriteResultSuccess(index == 0);
    });

    // Service → UI
    connect(m_service, &SimulatorService::connected, this, [this]() { setConnectedState(true); });
    connect(m_service, &SimulatorService::disconnected, this, [this]() { setConnectedState(false); });
    connect(m_service, &SimulatorService::sysLogEntry, this, [this](const QString &message, bool isError) {
        appendSysLog(message, isError);
    });
    connect(m_service, &SimulatorService::logEntry, this, [this](const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note) {
        appendLog(dir, cmd, seq, data, note);
    });

    // Service → external signals (forwarded to OscilloscopTab via MainWindow)
    connect(m_service, &SimulatorService::debugStreamGenerated, this, &SerialDebugTab::debugStreamGenerated);
    connect(m_service, &SimulatorService::debugStreamActiveChanged, this, &SerialDebugTab::debugStreamActiveChanged);
}

void SerialDebugTab::refreshPortList() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = SimulatorSerial::availablePorts();
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) m_portCombo->setCurrentIndex(currentIndex);
}

void SerialDebugTab::setConnectedState(bool connected) {
    m_connectButton->setText(connected ? tr("断开") : tr("连接"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_statusBadge->setText(connected ? tr("● 已连接") : tr("● 未连接"));
    m_statusBadge->setProperty("connected", connected);
    UiUtil::repolish(m_statusBadge);
}

void SerialDebugTab::appendSysLog(const QString &message, bool isError) {
    const QString color = isError ? Style::Color::LogError.name() : Style::Color::MutedText.name();
    const QString line = QStringLiteral("[%1] SYS  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), message);
    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

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

void SerialDebugTab::setupUi() {
    setObjectName(QStringLiteral("SerialDebugTab"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(16);
    rootLayout->setContentsMargins(24, 24, 24, 24);

    auto *connectionBar = new QFrame(this);
    connectionBar->setObjectName(QStringLiteral("connectionBar"));
    connectionBar->setMinimumSize(QSize(0, 36));
    connectionBar->setMaximumSize(QSize(QWIDGETSIZE_MAX, 36));
    auto *connectionLayout = new QHBoxLayout(connectionBar);
    connectionLayout->setObjectName(QStringLiteral("connectionLayout"));
    connectionLayout->setSpacing(10);
    connectionLayout->setContentsMargins(0, 0, 0, 1);
    rootLayout->addWidget(connectionBar);

    auto *portLabel = new QLabel(connectionBar);
    portLabel->setObjectName(QStringLiteral("portLabel"));
    portLabel->setText(QStringLiteral("Port:"));
    portLabel->setProperty("labelRole", QStringLiteral("form"));
    connectionLayout->addWidget(portLabel);

    m_portCombo = new QComboBox(connectionBar);
    m_portCombo->setObjectName(QStringLiteral("portCombo"));
    m_portCombo->setProperty("inputRole", QStringLiteral("form"));
    connectionLayout->addWidget(m_portCombo);

    auto *baudLabel = new QLabel(connectionBar);
    baudLabel->setObjectName(QStringLiteral("baudLabel"));
    baudLabel->setText(tr("波特率:"));
    baudLabel->setProperty("labelRole", QStringLiteral("form"));
    connectionLayout->addWidget(baudLabel);

    m_baudCombo = new QComboBox(connectionBar);
    m_baudCombo->setObjectName(QStringLiteral("baudCombo"));
    m_baudCombo->setProperty("inputRole", QStringLiteral("form"));
    m_baudCombo->addItems({QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"),
                           QStringLiteral("115200"), QStringLiteral("230400"), QStringLiteral("460800"), QStringLiteral("921600")});
    connectionLayout->addWidget(m_baudCombo);

    m_scanButton = new QPushButton(connectionBar);
    m_scanButton->setObjectName(QStringLiteral("scanButton"));
    m_scanButton->setMinimumSize(QSize(0, 32));
    m_scanButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_scanButton->setText(tr("刷新"));
    m_scanButton->setProperty("buttonRole", QStringLiteral("secondary"));
    connectionLayout->addWidget(m_scanButton);

    m_connectButton = new QPushButton(connectionBar);
    m_connectButton->setObjectName(QStringLiteral("connectButton"));
    m_connectButton->setMinimumSize(QSize(0, 32));
    m_connectButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_connectButton->setText(tr("连接"));
    m_connectButton->setProperty("buttonRole", QStringLiteral("primary"));
    connectionLayout->addWidget(m_connectButton);

    m_statusBadge = new QLabel(connectionBar);
    m_statusBadge->setObjectName(QStringLiteral("statusBadge"));
    m_statusBadge->setText(tr("● 未连接"));
    m_statusBadge->setProperty("connected", false);
    connectionLayout->addWidget(m_statusBadge);
    connectionLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setObjectName(QStringLiteral("mainSplitter"));
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(8);
    rootLayout->addWidget(m_mainSplitter);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);

    auto *leftPanel = new QWidget(m_mainSplitter);
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    leftPanel->setMinimumSize(QSize(260, 0));
    leftPanel->setMaximumSize(QSize(260, QWIDGETSIZE_MAX));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setObjectName(QStringLiteral("leftLayout"));
    leftLayout->setSpacing(16);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto *responseTitle = new QLabel(leftPanel);
    responseTitle->setObjectName(QStringLiteral("responseTitle"));
    responseTitle->setText(tr("应答配置"));
    leftLayout->addWidget(responseTitle);

    auto *responseForm = new QFormLayout();
    responseForm->setObjectName(QStringLiteral("responseForm"));
    responseForm->setHorizontalSpacing(10);
    responseForm->setVerticalSpacing(10);
    leftLayout->addLayout(responseForm);

    auto addFormLabel = [leftPanel, responseForm](const QString &objectName, const QString &text, int row) {
        auto *label = new QLabel(leftPanel);
        label->setObjectName(objectName);
        label->setText(text);
        label->setProperty("labelRole", QStringLiteral("form"));
        responseForm->setWidget(row, QFormLayout::LabelRole, label);
    };

    addFormLabel(QStringLiteral("scanAddrLabel"), tr("I2C扫描地址"), 0);
    m_scanAddrEdit = new QLineEdit(leftPanel);
    m_scanAddrEdit->setObjectName(QStringLiteral("scanAddrEdit"));
    m_scanAddrEdit->setText(QStringLiteral("0x5A"));
    m_scanAddrEdit->setProperty("inputRole", QStringLiteral("form"));
    responseForm->setWidget(0, QFormLayout::FieldRole, m_scanAddrEdit);

    addFormLabel(QStringLiteral("icAddrResultLabel"), tr("IC连接"), 1);
    m_icAddrResultCombo = new QComboBox(leftPanel);
    m_icAddrResultCombo->setObjectName(QStringLiteral("icAddrResultCombo"));
    m_icAddrResultCombo->setProperty("inputRole", QStringLiteral("form"));
    m_icAddrResultCombo->addItem(tr("成功"));
    m_icAddrResultCombo->addItem(tr("失败"));
    responseForm->setWidget(1, QFormLayout::FieldRole, m_icAddrResultCombo);

    addFormLabel(QStringLiteral("regReadValueLabel"), tr("寄存器读返回值"), 2);
    m_regReadValueEdit = new QLineEdit(leftPanel);
    m_regReadValueEdit->setObjectName(QStringLiteral("regReadValueEdit"));
    m_regReadValueEdit->setText(QStringLiteral("0x0000"));
    m_regReadValueEdit->setProperty("inputRole", QStringLiteral("form"));
    responseForm->setWidget(2, QFormLayout::FieldRole, m_regReadValueEdit);

    addFormLabel(QStringLiteral("writeResultLabel"), tr("寄存器写"), 3);
    m_writeResultCombo = new QComboBox(leftPanel);
    m_writeResultCombo->setObjectName(QStringLiteral("writeResultCombo"));
    m_writeResultCombo->setProperty("inputRole", QStringLiteral("form"));
    m_writeResultCombo->addItem(QStringLiteral("ACK"));
    m_writeResultCombo->addItem(tr("失败"));
    responseForm->setWidget(3, QFormLayout::FieldRole, m_writeResultCombo);

    addFormLabel(QStringLiteral("delayLabel"), tr("响应延迟(ms)"), 4);
    m_delaySpinBox = new QSpinBox(leftPanel);
    m_delaySpinBox->setObjectName(QStringLiteral("delaySpinBox"));
    m_delaySpinBox->setProperty("inputRole", QStringLiteral("form"));
    m_delaySpinBox->setMaximum(500);
    responseForm->setWidget(4, QFormLayout::FieldRole, m_delaySpinBox);
    leftLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    auto *logPanel = new QWidget(m_mainSplitter);
    logPanel->setObjectName(QStringLiteral("logPanel"));
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setObjectName(QStringLiteral("logLayout"));
    logLayout->setSpacing(10);
    logLayout->setContentsMargins(0, 0, 0, 0);

    auto *logHeader = new QHBoxLayout();
    logHeader->setObjectName(QStringLiteral("logHeader"));
    logHeader->setSpacing(10);
    logHeader->setContentsMargins(0, 0, 0, 0);
    logLayout->addLayout(logHeader);

    auto *logTitle = new QLabel(logPanel);
    logTitle->setObjectName(QStringLiteral("logTitle"));
    logTitle->setText(tr("活动日志"));
    logHeader->addWidget(logTitle);
    logHeader->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_clearLogButton = new QPushButton(logPanel);
    m_clearLogButton->setObjectName(QStringLiteral("clearLogButton"));
    m_clearLogButton->setMinimumSize(QSize(0, 32));
    m_clearLogButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_clearLogButton->setText(tr("清除"));
    m_clearLogButton->setProperty("buttonRole", QStringLiteral("secondary"));
    logHeader->addWidget(m_clearLogButton);

    m_logEdit = new QTextEdit(logPanel);
    m_logEdit->setObjectName(QStringLiteral("logEdit"));
    m_logEdit->setReadOnly(true);
    logLayout->addWidget(m_logEdit);
}
