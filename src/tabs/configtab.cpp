#include "tabs/configtab.h"

#include "devicecontext.h"
#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"
#include "ui_configtab.h"
#include "widgets/sidebar.h"

#include <QByteArray>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QDebug>
#include <utility>

using namespace MotorDev;

namespace {
class GroupHoverFilter : public QObject {
public:
    explicit GroupHoverFilter(QObject *parent = nullptr)
        : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        if (event->type() == QEvent::HoverEnter) {
            widget->setProperty("hovered", true);
            UiUtil::repolish(widget);
        } else if (event->type() == QEvent::HoverLeave) {
            widget->setProperty("hovered", false);
            UiUtil::repolish(widget);
        }

        return QObject::eventFilter(watched, event);
    }
};

void applyPanelShadow(QWidget *widget) {
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setOffset(0, 1);
    effect->setBlurRadius(6);
    effect->setColor(Style::Color::PanelShadow);
    widget->setGraphicsEffect(effect);
}

MotorIcType motorIcTypeFromIndex(int index) {
    switch (index) {
    case 1: return MotorIcType::DW9786;
    case 2: return MotorIcType::DW9788;
    case 0:
    default: return MotorIcType::AW86006;
    }
}

int indexFromMotorIcType(MotorIcType type) {
    switch (type) {
    case MotorIcType::DW9786: return 1;
    case MotorIcType::DW9788: return 2;
    case MotorIcType::AW86006:
    default: return 0;
    }
}

QString motorIcTypeToString(MotorIcType type) {
    switch (type) {
    case MotorIcType::AW86006: return QStringLiteral("AW86006");
    case MotorIcType::DW9786: return QStringLiteral("DW9786");
    case MotorIcType::DW9788: return QStringLiteral("DW9788");
    }
    return QStringLiteral("Unknown");
}
}

ConfigTab::ConfigTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ConfigTab>())
    , m_serialManager(serialManager)
    , m_deviceContext(deviceContext) {
    qRegisterMetaType<uint8_t>("uint8_t");
    ui->setupUi(this);
    for (int column = 0; column < 4; ++column) {
        ui->upperLayout->setColumnStretch(column, 1);
    }
    for (int row = 0; row < 3; ++row) {
        ui->upperLayout->setRowStretch(row, 1);
    }

    auto *topLayout = qobject_cast<QHBoxLayout *>(layout());
    if (topLayout != nullptr) {
        topLayout->removeWidget(ui->sidebarContent);
        auto *sidebar = new Sidebar(tr("配置"), ui->sidebarContent, this);
        topLayout->insertWidget(0, sidebar);
    }

    ui->mainSplitter->setStretchFactor(0, 4);
    ui->mainSplitter->setStretchFactor(1, 1);
    for (auto *group : {ui->icGroup, ui->serialGroup, ui->pmicGroup}) {
        group->setAttribute(Qt::WA_Hover, true);
        group->setProperty("hovered", false);
        group->installEventFilter(new GroupHoverFilter(group));
        applyPanelShadow(group);
    }

    m_icCombo = ui->icCombo;
    m_slaveIdCombo = ui->slaveIdCombo;
    m_icScanButton = ui->icScanButton;
    m_icConnectButton = ui->icConnectButton;
    m_portCombo = ui->portCombo;
    m_baudRateCombo = ui->baudRateCombo;
    m_scanButton = ui->scanButton;
    m_connectButton = ui->connectButton;
    m_drvddSpin = ui->drvddSpin;
    m_vcmvddSpin = ui->vcmvddSpin;
    m_iovddSpin = ui->iovddSpin;
    m_pmicConfigButton = ui->pmicConfigButton;
    m_fileCombo = ui->fileCombo;
    m_browseButton = ui->browseButton;
    m_writeButton = ui->writeButton;
    m_readButton = ui->readButton;

    m_slaveIdCombo->setPlaceholderText(QStringLiteral("Scan first"));
    m_slaveIdCombo->setCurrentIndex(-1);
    m_portCombo->setPlaceholderText(tr("Select COM"));
    m_baudRateCombo->setCurrentText(QStringLiteral("115200"));
    for (auto *spin : {m_drvddSpin, m_vcmvddSpin, m_iovddSpin}) {
        spin->setRange(0.0, 10.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.1);
    }
    m_fileCombo->setInsertPolicy(QComboBox::NoInsert);
    m_fileCombo->setPlaceholderText(tr("Select config file"));

    connectSignals();
}

ConfigTab::~ConfigTab() = default;

void ConfigTab::connectSignals() {
    if (m_serialManager == nullptr || m_deviceContext == nullptr) {
        qWarning() << "ConfigTab dependencies are not fully initialized";
        return;
    }

    refreshAvailablePorts();
    setSerialControlsConnected(false);
    {
        const QSignalBlocker comboBlocker(m_icCombo);
        m_icCombo->setCurrentIndex(indexFromMotorIcType(m_deviceContext->icType()));
    }

    connect(m_scanButton, &QPushButton::clicked, this, [this]() { qDebug() << "Scan started"; refreshAvailablePorts(); });
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_serialManager == nullptr) return;
        if (m_isSerialConnected) { qDebug() << "Disconnecting..."; QMetaObject::invokeMethod(m_serialManager, "closePort", Qt::QueuedConnection); return; }
        const QString portName = m_portCombo->currentText().trimmed();
        if (portName.isEmpty()) { qWarning() << "Connect failed: no port selected"; return; }
        const qint32 baudRate = m_baudRateCombo->currentText().toInt();
        qDebug().noquote() << QStringLiteral("Connecting to %1 at %2...").arg(portName).arg(baudRate);
        QMetaObject::invokeMethod(m_serialManager, "openPort", Qt::QueuedConnection, Q_ARG(QString, portName), Q_ARG(qint32, baudRate));
    });
    connect(m_serialManager, &SerialManager::connected, this, [this]() { qDebug() << "Serial connected"; setSerialControlsConnected(true); });
    connect(m_serialManager, &SerialManager::disconnected, this, [this]() { qDebug() << "Serial disconnected"; setSerialControlsConnected(false); });
    connect(m_serialManager, &SerialManager::errorOccurred, this, [](const QString &message) { qWarning().noquote() << QStringLiteral("Serial error: %1").arg(message); });
    connect(m_serialManager, &SerialManager::frameReceived, this, &ConfigTab::onFrameReceived);
    connect(m_icCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) { const MotorIcType type = motorIcTypeFromIndex(index); qDebug().noquote() << QStringLiteral("IC type changed to %1").arg(motorIcTypeToString(type)); m_deviceContext->setIcType(type); });
    connect(m_deviceContext, &DeviceContext::icTypeChanged, this, [this](MotorIcType type) { const QSignalBlocker blocker(m_icCombo); m_icCombo->setCurrentIndex(indexFromMotorIcType(type)); });
    connect(m_icScanButton, &QPushButton::clicked, this, [this]() {
        if (m_serialManager == nullptr || !m_isSerialConnected) { qWarning() << "I2C scan failed: serial not connected"; return; }
        qDebug() << "I2C scan started on bus I2C2";
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection, Q_ARG(uint8_t, MotorProtocol::CmdI2cBusScan), Q_ARG(QByteArray, MotorProtocol::encodeI2cBusScan()));
    });
    connect(m_icConnectButton, &QPushButton::clicked, this, [this]() {
        if (m_serialManager == nullptr || !m_isSerialConnected) { qWarning() << "IC connect failed: serial not connected"; return; }
        const QString addrText = m_slaveIdCombo->currentText().trimmed();
        if (addrText.isEmpty()) { qWarning() << "IC connect failed: no slave address selected"; return; }
        bool ok = false; const uint addr = addrText.toUInt(&ok, 16);
        if (!ok || addr == 0 || addr > 0x7F) { qWarning().noquote() << QStringLiteral("IC connect failed: invalid address %1").arg(addrText); return; }
        qDebug().noquote() << QStringLiteral("Setting motor IC address to 0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper();
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection, Q_ARG(uint8_t, MotorProtocol::CmdSetMotorIcAddr), Q_ARG(QByteArray, MotorProtocol::encodeSetMotorIcAddr(static_cast<uint8_t>(addr))));
    });
    connect(m_slaveIdCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text.isEmpty()) return;
        bool ok = false; const uint value = text.toUInt(&ok, 16);
        if (ok && value <= 0x7F) { qDebug().noquote() << QStringLiteral("Slave ID set to 0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(); m_deviceContext->setSlaveId(static_cast<uint8_t>(value)); return; }
        if (!ok || value > 0x7F) qWarning().noquote() << QStringLiteral("Invalid slave ID: %1").arg(text);
    });
    connect(m_deviceContext, &DeviceContext::slaveIdChanged, this, [this](uint8_t id) {
        const QSignalBlocker blocker(m_slaveIdCombo);
        if (id == 0x00) { m_slaveIdCombo->setCurrentIndex(-1); return; }
        const QString text = QStringLiteral("0x%1").arg(id, 2, 16, QLatin1Char('0')).toUpper();
        const int index = m_slaveIdCombo->findText(text);
        if (index >= 0) m_slaveIdCombo->setCurrentIndex(index);
    });
}

void ConfigTab::refreshAvailablePorts() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = SerialManager::availablePorts();
    const QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) m_portCombo->setCurrentIndex(currentIndex);
    if (ports.isEmpty()) { qDebug() << "Scan found 0 ports"; return; }
    qDebug().noquote() << QStringLiteral("Scan found %1 ports: %2").arg(ports.size()).arg(ports.join(QStringLiteral(", ")));
}

void ConfigTab::setSerialControlsConnected(bool connected) {
    m_isSerialConnected = connected;
    m_connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_portCombo->setEnabled(!connected);
    m_baudRateCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_icScanButton->setEnabled(connected);
    m_icConnectButton->setEnabled(connected);
    if (connected) emit serialConnected(m_portCombo->currentText().trimmed(), m_baudRateCombo->currentText().toInt());
    else emit serialDisconnected();
}

void ConfigTab::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);
    switch (cmd) {
    case MotorProtocol::CmdI2cBusScan: handleScanResponse(data); break;
    case MotorProtocol::CmdSetMotorIcAddr: handleSetAddrResponse(); break;
    case MotorProtocol::CmdErrorResponse: handleErrorResponse(data); break;
    default: qDebug().noquote() << QStringLiteral("Unhandled frame: cmd=0x%1").arg(cmd, 2, 16, QLatin1Char('0')).toUpper(); break;
    }
}

void ConfigTab::handleScanResponse(const QByteArray &data) {
    if (data.isEmpty()) { qWarning() << "I2C scan response: empty data"; return; }
    const QList<uint8_t> addresses = MotorProtocol::decodeI2cScanResponse(data);
    qDebug().noquote() << QStringLiteral("I2C scan found %1 devices").arg(addresses.size());
    const QSignalBlocker blocker(m_slaveIdCombo);
    m_slaveIdCombo->clear();
    for (uint8_t addr : addresses) {
        const QString addrStr = QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper();
        m_slaveIdCombo->addItem(addrStr);
        qDebug().noquote() << QStringLiteral("  Device: %1").arg(addrStr);
    }
    if (addresses.isEmpty()) qDebug() << "No I2C devices found";
    else if (m_slaveIdCombo->count() > 0) m_slaveIdCombo->setCurrentIndex(0);
}

void ConfigTab::handleSetAddrResponse() {
    const QString addrText = m_slaveIdCombo->currentText().trimmed();
    bool ok = false;
    const uint addr = addrText.toUInt(&ok, 16);
    if (ok && addr <= 0x7F) m_deviceContext->setSlaveId(static_cast<uint8_t>(addr));
    qDebug().noquote() << QStringLiteral("Motor IC address set to %1").arg(addrText);
}

void ConfigTab::handleErrorResponse(const QByteArray &data) {
    const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
    QString errorName;
    switch (errorCode) {
    case 0x01: errorName = QStringLiteral("CRC check failed"); break;
    case 0x02: errorName = QStringLiteral("Unknown command"); break;
    case 0x03: errorName = QStringLiteral("Execution failed"); break;
    default: errorName = QStringLiteral("Unknown error"); break;
    }
    qWarning().noquote() << QStringLiteral("Error response: code=0x%1 (%2)").arg(errorCode, 2, 16, QLatin1Char('0')).toUpper().arg(errorName);
}
