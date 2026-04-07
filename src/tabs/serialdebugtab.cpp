#include "tabs/serialdebugtab.h"

#include "frameparser.h"
#include "protocol/motor_protocol.h"
#include "services/simulatorserial.h"
#include "ui/style_constants.h"

#include <QByteArray>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextEdit>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString makePrimaryButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
               "QPushButton:hover { background:%5; }"
               "QPushButton:disabled { background:%6; border:%2px solid %7; color:%8; }"
               "QPushButton:pressed { background:%9; padding:1px 12px 0 12px; }")
        .arg(Style::Color::LightGreen.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::PrimaryGreen.name())
        .arg(Style::Color::PrimaryGreen.name())
        .arg(Style::Color::PrimaryButtonHover.name())
        .arg(Style::Color::DisabledBackground.name())
        .arg(Style::Color::DisabledBorder.name())
        .arg(Style::Color::DisabledText.name())
        .arg(Style::Color::BorderGreen.name());
}

QString makeSecondaryButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
               "QPushButton:hover { background:%5; }"
               "QPushButton:disabled { background:%6; border:%2px solid %7; color:%8; }"
               "QPushButton:pressed { background:%9; padding:1px 12px 0 12px; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(Style::Color::SecondaryButtonHover.name())
        .arg(Style::Color::DisabledBackground.name())
        .arg(Style::Color::DisabledBorder.name())
        .arg(Style::Color::DisabledText.name())
        .arg(Style::Color::SecondaryButtonPressed.name());
}

QString makeComboStyle() {
    return QStringLiteral(
               "QComboBox { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; min-height:%5px; }"
               "QComboBox QAbstractItemView { background:%1; border:%2px solid %3; color:%4; selection-background-color:%6; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::InputBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(Style::Size::SidebarComboMinHeight)
        .arg(Style::Color::LightGreen.name());
}

QString makeLineEditStyle() {
    return QStringLiteral(
               "QLineEdit { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; min-height:%5px; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::InputBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(Style::Size::SidebarComboMinHeight);
}

QString makeSpinBoxStyle() {
    return QStringLiteral(
               "QSpinBox { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; min-height:%5px; }"
               "QSpinBox::up-button, QSpinBox::down-button { width:%6px; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::InputBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(Style::Size::SidebarComboMinHeight)
        .arg(Style::Size::SidebarHandleWidth);
}

QString makeSectionTitleStyle() {
    return QStringLiteral("color:%1; font-size:13px; font-weight:500;")
        .arg(Style::Color::SidebarLabelText.name());
}

QString makeFormLabelStyle() {
    return QStringLiteral("color:%1; font-size:13px;")
        .arg(Style::Color::TopBarValueText.name());
}

QString makeStatusBadgeStyle(const QColor &color) {
    return QStringLiteral("color:%1; font-size:12px;")
        .arg(color.name());
}

QString formatByte(uint8_t value) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString formatWord(quint16 value) {
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

QByteArray encodeWord(quint16 value) {
    QByteArray data;
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>(value & 0xFF));
    return data;
}
} // namespace

SerialDebugTab::SerialDebugTab(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowCloseButtonHint) {
    setWindowTitle(tr("串口调试模拟器"));
    resize(820, 560);
    qRegisterMetaType<uint8_t>("uint8_t");

    m_simulator = new SimulatorSerial();

    setupUi();
    connectSignals();
    refreshPortList();
    setConnectedState(false);
}

SerialDebugTab::~SerialDebugTab() {
    delete m_simulator;
    m_simulator = nullptr;
}

void SerialDebugTab::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    root->setSpacing(Style::Size::ContentSpacing);

    root->addWidget(createConnectionBar());

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(Style::Size::SplitterHandleWidth);
    splitter->setStyleSheet(QStringLiteral("QSplitter::handle { background:%1; }")
                                .arg(Style::Color::WindowBackground.name()));

    auto *leftPanel = createLeftPanel();
    leftPanel->setFixedWidth(Style::Size::SimulatorLeftPanelWidth);

    splitter->addWidget(leftPanel);
    splitter->addWidget(createLogPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter, 1);
}

QWidget *SerialDebugTab::createConnectionBar() {
    auto *bar = new QFrame(this);
    bar->setFixedHeight(Style::Size::TopBarHeight);
    bar->setStyleSheet(QStringLiteral("QFrame { border-bottom:%1px solid %2; background:%3; }")
                           .arg(Style::Size::BorderThin)
                           .arg(Style::Color::DefaultBorder.name())
                           .arg(Style::Color::Transparent.name()));

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, Style::Size::BorderThin);
    layout->setSpacing(Style::Size::FormSpacing);

    auto *portLabel = new QLabel(tr("Port:"), bar);
    portLabel->setStyleSheet(makeFormLabelStyle());
    layout->addWidget(portLabel);

    m_portCombo = new QComboBox(bar);
    m_portCombo->setPlaceholderText(tr("选择端口"));
    m_portCombo->setStyleSheet(makeComboStyle());
    layout->addWidget(m_portCombo);

    auto *baudLabel = new QLabel(tr("波特率:"), bar);
    baudLabel->setStyleSheet(makeFormLabelStyle());
    layout->addWidget(baudLabel);

    m_baudCombo = new QComboBox(bar);
    m_baudCombo->addItems(
        {QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"),
         QStringLiteral("115200"), QStringLiteral("230400"), QStringLiteral("460800"), QStringLiteral("921600")});
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_baudCombo->setStyleSheet(makeComboStyle());
    layout->addWidget(m_baudCombo);

    m_scanButton = new QPushButton(tr("刷新"), bar);
    m_scanButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_scanButton->setStyleSheet(makeSecondaryButtonStyle());
    layout->addWidget(m_scanButton);

    m_connectButton = new QPushButton(tr("连接"), bar);
    m_connectButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_connectButton->setStyleSheet(makePrimaryButtonStyle());
    layout->addWidget(m_connectButton);

    m_statusBadge = new QLabel(tr("● 未连接"), bar);
    m_statusBadge->setStyleSheet(makeStatusBadgeStyle(Style::Color::DisconnectedIndicator));
    layout->addWidget(m_statusBadge);
    layout->addStretch();

    return bar;
}

QWidget *SerialDebugTab::createLeftPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Style::Size::ContentSpacing);

    auto *responseTitle = new QLabel(tr("应答配置"), panel);
    responseTitle->setStyleSheet(makeSectionTitleStyle());
    layout->addWidget(responseTitle);

    auto *responseForm = new QFormLayout;
    responseForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    responseForm->setHorizontalSpacing(Style::Size::FormSpacing);
    responseForm->setVerticalSpacing(Style::Size::FormSpacing);

    m_scanAddrEdit = new QLineEdit(QStringLiteral("0x5A"), panel);
    m_scanAddrEdit->setStyleSheet(makeLineEditStyle());
    responseForm->addRow(new QLabel(tr("I2C扫描地址"), panel), m_scanAddrEdit);

    m_icAddrResultCombo = new QComboBox(panel);
    m_icAddrResultCombo->addItems({tr("成功"), tr("失败")});
    m_icAddrResultCombo->setStyleSheet(makeComboStyle());
    responseForm->addRow(new QLabel(tr("IC连接"), panel), m_icAddrResultCombo);

    m_regReadValueEdit = new QLineEdit(QStringLiteral("0x0000"), panel);
    m_regReadValueEdit->setPlaceholderText(QStringLiteral("0x0000"));
    m_regReadValueEdit->setStyleSheet(makeLineEditStyle());
    responseForm->addRow(new QLabel(tr("寄存器读返回值"), panel), m_regReadValueEdit);

    m_writeResultCombo = new QComboBox(panel);
    m_writeResultCombo->addItems({QStringLiteral("ACK"), tr("失败")});
    m_writeResultCombo->setStyleSheet(makeComboStyle());
    responseForm->addRow(new QLabel(tr("寄存器写"), panel), m_writeResultCombo);

    m_delaySpinBox = new QSpinBox(panel);
    m_delaySpinBox->setRange(0, 500);
    m_delaySpinBox->setValue(0);
    m_delaySpinBox->setStyleSheet(makeSpinBoxStyle());
    responseForm->addRow(new QLabel(tr("响应延迟(ms)"), panel), m_delaySpinBox);

    for (int row = 0; row < responseForm->rowCount(); ++row) {
        auto *labelItem = responseForm->itemAt(row, QFormLayout::LabelRole);
        if (labelItem == nullptr) {
            continue;
        }
        auto *label = qobject_cast<QLabel *>(labelItem->widget());
        if (label != nullptr) {
            label->setStyleSheet(makeFormLabelStyle());
        }
    }
    layout->addLayout(responseForm);

    layout->addStretch();

    return panel;
}

QWidget *SerialDebugTab::createLogPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Style::Size::FormSpacing);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(Style::Size::FormSpacing);

    auto *title = new QLabel(tr("活动日志"), panel);
    title->setStyleSheet(makeSectionTitleStyle());
    headerRow->addWidget(title);
    headerRow->addStretch();

    m_clearLogButton = new QPushButton(tr("清除"), panel);
    m_clearLogButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_clearLogButton->setStyleSheet(makeSecondaryButtonStyle());
    headerRow->addWidget(m_clearLogButton);
    layout->addLayout(headerRow);

    m_logEdit = new QTextEdit(panel);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(QStringLiteral(
        "QTextEdit { background:%1; border:none; color:%2; font-family:'Consolas'; font-size:11px; }")
                                 .arg(Style::Color::PanelBackground.name())
                                 .arg(Style::Color::AppText.name()));
    layout->addWidget(m_logEdit, 1);

    return panel;
}

QString SerialDebugTab::describeIncoming(uint8_t cmd, const QByteArray &data) {
    switch (cmd) {
    case 0x00:
        return QStringLiteral("HEARTBEAT");
    case MotorProtocol::CmdI2cBusScan:
        return QStringLiteral("I2C_SCAN");
    case MotorProtocol::CmdSetMotorIcAddr:
        if (!data.isEmpty()) {
            return QStringLiteral("SET_IC_ADDR addr=%1")
                .arg(formatByte(static_cast<uint8_t>(data.at(0))));
        }
        return QStringLiteral("SET_IC_ADDR");
    case MotorProtocol::CmdReadRegister:
        if (data.size() >= 2) {
            const quint16 addr = static_cast<quint16>(
                (static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
            return QStringLiteral("READ addr=%1").arg(formatWord(addr));
        }
        return QStringLiteral("READ");
    case MotorProtocol::CmdWriteRegister:
        if (data.size() >= 4) {
            const quint16 addr = static_cast<quint16>(
                (static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
            const quint16 val = static_cast<quint16>(
                (static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3)));
            return QStringLiteral("WRITE addr=%1 val=%2")
                .arg(formatWord(addr), formatWord(val));
        }
        return QStringLiteral("WRITE");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

void SerialDebugTab::connectSignals() {
    connect(m_scanButton, &QPushButton::clicked, this, &SerialDebugTab::refreshPortList);
    connect(m_clearLogButton, &QPushButton::clicked, m_logEdit, &QTextEdit::clear);

    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_isConnected) {
            QMetaObject::invokeMethod(m_simulator, "closePort", Qt::QueuedConnection);
            return;
        }

        const QString portName = m_portCombo->currentText().trimmed();
        if (portName.isEmpty()) {
            appendSysLog(tr("未选择串口"), true);
            return;
        }

        QMetaObject::invokeMethod(
            m_simulator,
            "openPort",
            Qt::QueuedConnection,
            Q_ARG(QString, portName),
            Q_ARG(qint32, m_baudCombo->currentText().toInt()));
    });

    connect(m_simulator, &SimulatorSerial::connected, this, [this]() {
        setConnectedState(true);
        appendSysLog(tr("模拟器已连接"));
    });
    connect(m_simulator, &SimulatorSerial::disconnected, this, [this]() {
        setConnectedState(false);
        appendSysLog(tr("模拟器已断开"));
    });
    connect(m_simulator, &SimulatorSerial::errorOccurred, this, [this](const QString &message) {
        appendSysLog(message, true);
    });
    connect(m_simulator, &SimulatorSerial::frameReceived, this, &SerialDebugTab::onFrameReceived);
}

void SerialDebugTab::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    appendLog(QStringLiteral("RX"), cmd, seq, data, describeIncoming(cmd, data));
    dispatchWithDelay(cmd, seq, data);
}

void SerialDebugTab::dispatchWithDelay(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    if (cmd == 0x00) {
        handleHeartbeat(seq);
        return;
    }

    const auto dispatch = [this, cmd, seq, data]() {
        switch (cmd) {
        case MotorProtocol::CmdI2cBusScan:
            handleI2cScan(seq, data);
            break;
        case MotorProtocol::CmdSetMotorIcAddr:
            handleSetIcAddr(seq, data);
            break;
        case MotorProtocol::CmdReadRegister:
            handleReadRegister(seq, data);
            break;
        case MotorProtocol::CmdWriteRegister:
            handleWriteRegister(seq, data);
            break;
        default:
            handleUnknownCommand(cmd, seq);
            break;
        }
    };

    const int delayMs = m_delaySpinBox->value();
    if (delayMs > 0) {
        QTimer::singleShot(delayMs, this, dispatch);
        return;
    }

    dispatch();
}

void SerialDebugTab::handleHeartbeat(uint8_t seq) {
    sendResponseFrame(seq, 0x00, {});
    appendLog(QStringLiteral("TX"), 0x00, seq, {}, QStringLiteral("HEARTBEAT echo"));
}

void SerialDebugTab::handleI2cScan(uint8_t seq, const QByteArray &data) {
    Q_UNUSED(data);

    QByteArray response;
    QStringList accepted;
    const QStringList tokens = m_scanAddrEdit->text().split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        QString text = token.trimmed();
        if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            text = text.mid(2);
        }

        bool ok = false;
        const uint value = text.toUInt(&ok, 16);
        if (!ok || value > 0x7F) {
            continue;
        }

        response.append(static_cast<char>(value));
        accepted.append(QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
    }

    response.prepend(static_cast<char>(response.size()));
    sendResponseFrame(seq, MotorProtocol::CmdI2cBusScan, response);
    const QString scanNote = accepted.isEmpty()
        ? QStringLiteral("I2C_SCAN → (空)")
        : QStringLiteral("I2C_SCAN → %1").arg(accepted.join(QStringLiteral(", ")));
    appendLog(QStringLiteral("TX"), MotorProtocol::CmdI2cBusScan, seq, response, scanNote);
}

void SerialDebugTab::handleSetIcAddr(uint8_t seq, const QByteArray &data) {
    if (m_icAddrResultCombo->currentIndex() == 0) {
        sendResponseFrame(seq, MotorProtocol::CmdSetMotorIcAddr, {});
        appendLog(QStringLiteral("TX"), MotorProtocol::CmdSetMotorIcAddr, seq, {}, QStringLiteral("SET_IC_ADDR → 成功"));
        return;
    }

    sendErrorFrame(seq, 0x03);
}

void SerialDebugTab::handleReadRegister(uint8_t seq, const QByteArray &data) {
    if (data.size() < 2) {
        sendErrorFrame(seq, 0x03);
        return;
    }

    const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
    const qint16 value = registerValueAt(addr);
    const QByteArray response = encodeWord(static_cast<quint16>(value));
    sendResponseFrame(seq, MotorProtocol::CmdReadRegister, response);
    appendLog(QStringLiteral("TX"), MotorProtocol::CmdReadRegister, seq, response,
              QStringLiteral("READ → %1").arg(formatWord(static_cast<quint16>(value))));
}

void SerialDebugTab::handleWriteRegister(uint8_t seq, const QByteArray &data) {
    if (data.size() < 4) {
        sendErrorFrame(seq, 0x03);
        return;
    }

    if (m_writeResultCombo->currentText() != QStringLiteral("ACK")) {
        sendErrorFrame(seq, 0x03);
        return;
    }

    const quint16 addr = static_cast<quint16>((static_cast<quint8>(data.at(0)) << 8) | static_cast<quint8>(data.at(1)));
    const quint16 value = static_cast<quint16>((static_cast<quint8>(data.at(2)) << 8) | static_cast<quint8>(data.at(3)));

    Q_UNUSED(addr);
    Q_UNUSED(value);

    sendResponseFrame(seq, MotorProtocol::CmdWriteRegister, {});
    appendLog(QStringLiteral("TX"), MotorProtocol::CmdWriteRegister, seq, {}, QStringLiteral("WRITE → ACK"));
}

void SerialDebugTab::handleUnknownCommand(uint8_t cmd, uint8_t seq) {
    sendErrorFrame(seq, 0x02);
    appendLog(QStringLiteral("TX"), cmd, seq, {}, tr("未知命令"));
}

void SerialDebugTab::sendResponseFrame(uint8_t seq, uint8_t cmd, const QByteArray &data) {
    const QByteArray frame = FrameParser::encodeControlFrame(seq, cmd, data);
    QMetaObject::invokeMethod(
        m_simulator,
        "sendRawFrame",
        Qt::QueuedConnection,
        Q_ARG(QByteArray, frame));
}

void SerialDebugTab::sendErrorFrame(uint8_t seq, uint8_t errorCode) {
    const QByteArray data(1, static_cast<char>(errorCode));
    sendResponseFrame(seq, MotorProtocol::CmdErrorResponse, data);
    appendLog(QStringLiteral("TX"), MotorProtocol::CmdErrorResponse, seq, data,
              QStringLiteral("ERROR %1").arg(formatByte(errorCode)));
}

void SerialDebugTab::appendSysLog(const QString &message, bool isError) {
    const QString color = isError ? Style::Color::LogError.name()
                                  : Style::Color::MutedText.name();
    const QString line = QStringLiteral("[%1] SYS  %2")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                  message);
    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                          .arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

void SerialDebugTab::appendLog(const QString &dir, uint8_t cmd, uint8_t seq, const QByteArray &data, const QString &note) {
    QString color = Style::Color::AppText.name();
    if (dir == QStringLiteral("TX")) {
        color = (cmd == MotorProtocol::CmdErrorResponse) ? Style::Color::LogError.name()
                                                         : Style::Color::ReadButtonForeground.name();
    }

    QString dataText;
    if (!data.isEmpty()) {
        QStringList bytes;
        bytes.reserve(data.size());
        for (char byte : data) {
            bytes.append(formatByte(static_cast<uint8_t>(byte)));
        }
        dataText = bytes.join(QLatin1Char(' '));
    }

    QString line = QStringLiteral("[%1] %2 cmd=%3 seq=%4")
                       .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")))
                       .arg(dir)
                       .arg(formatByte(cmd))
                       .arg(formatByte(seq));

    if (!dataText.isEmpty()) {
        line.append(QStringLiteral("  %1").arg(dataText));
    }
    if (!note.isEmpty()) {
        line.append(QStringLiteral("  (%1)").arg(note));
    }

    m_logEdit->append(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                          .arg(color, line.toHtmlEscaped()));
    m_logEdit->moveCursor(QTextCursor::End);
}

void SerialDebugTab::refreshPortList() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = SimulatorSerial::availablePorts();

    const QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();
    m_portCombo->addItems(ports);

    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) {
        m_portCombo->setCurrentIndex(currentIndex);
    }
}

void SerialDebugTab::setConnectedState(bool connected) {
    m_isConnected = connected;
    m_connectButton->setText(connected ? tr("断开") : tr("连接"));
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_statusBadge->setText(connected ? tr("● 已连接") : tr("● 未连接"));
    m_statusBadge->setStyleSheet(makeStatusBadgeStyle(
        connected ? Style::Color::ConnectedIndicator
                  : Style::Color::DisconnectedIndicator));
}

qint16 SerialDebugTab::registerValueAt(quint16 addr) const {
    Q_UNUSED(addr);
    bool ok = false;
    QString text = m_regReadValueEdit->text().trimmed();
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text = text.mid(2);
    }
    const quint16 raw = text.toUShort(&ok, 16);
    return ok ? static_cast<qint16>(raw) : 0;
}
