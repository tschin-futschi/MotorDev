#include "tabs/configtab.h"

#include "devicecontext.h"
#include "services/commanddispatcher.h"
#include "services/configservice.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"
#include "widgets/sidebar.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpacerItem>
#include <QStyle>
#include <QVBoxLayout>
#include <QDebug>

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
}

ConfigTab::ConfigTab(SerialManager *serialManager,
                     CommandDispatcher *dispatcher,
                     DeviceContext *deviceContext,
                     QWidget *parent)
    : QWidget(parent)
    , m_deviceContext(deviceContext) {
    qRegisterMetaType<uint8_t>("uint8_t");
    m_service = new ConfigService(serialManager, dispatcher, deviceContext, this);
    setupUi();
    for (auto *group : {m_icGroup, m_serialGroup, m_pmicGroup}) {
        group->setAttribute(Qt::WA_Hover, true);
        group->setProperty("hovered", false);
        group->installEventFilter(new GroupHoverFilter(group));
        applyPanelShadow(group);
    }

    m_slaveIdCombo->setPlaceholderText(QStringLiteral("Scan first"));
    m_slaveIdCombo->setCurrentIndex(-1);
    m_portCombo->setPlaceholderText(tr("Select COM"));
    m_baudRateCombo->setCurrentText(QLatin1String(Style::Serial::DefaultBaudRate));
    for (auto *spin : {m_drvddSpin, m_vcmvddSpin, m_iovddSpin}) {
        spin->setRange(0.60, 3.77);
        spin->setDecimals(2);
        spin->setSingleStep(0.10);
    }
    m_drvddSpin->setValue(1.80);
    m_iovddSpin->setValue(2.80);
    m_vcmvddSpin->setValue(3.20);
    m_fileCombo->setInsertPolicy(QComboBox::NoInsert);
    m_fileCombo->setPlaceholderText(tr("Select config file"));
    m_resetButton->setEnabled(false);
    m_motorTestButton->setEnabled(false);
    m_pmicDisableButton->setEnabled(false);
    m_browseButton->setEnabled(false);
    m_browseButton->setToolTip(tr("功能开发中"));
    m_writeButton->setEnabled(false);
    m_writeButton->setToolTip(tr("功能开发中"));
    m_readButton->setEnabled(false);
    m_readButton->setToolTip(tr("功能开发中"));
    m_fileCombo->setEnabled(false);
    m_fileCombo->setToolTip(tr("功能开发中"));

    connectSignals();
}

ConfigTab::~ConfigTab() = default;

void ConfigTab::connectSignals() {
    if (m_service == nullptr || m_deviceContext == nullptr) {
        qWarning() << "ConfigTab dependencies are not fully initialized";
        return;
    }

    refreshAvailablePorts();
    setSerialControlsConnected(false);
    {
        const QSignalBlocker comboBlocker(m_icCombo);
        m_icCombo->setCurrentIndex(DeviceContext::indexFromMotorIcType(m_deviceContext->icType()));
    }

    // UI → Service
    connect(m_scanButton, &QPushButton::clicked, this, [this]() {
        qDebug() << "Scan started";
        refreshAvailablePorts();
    });
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        m_connectButton->setEnabled(false);
        if (m_service->isConnected()) {
            m_service->disconnectPort();
            return;
        }
        const QString portName = m_portCombo->currentText().trimmed();
        if (portName.isEmpty()) {
            m_connectButton->setEnabled(true);
            qWarning() << "Connect failed: no port selected";
            return;
        }
        const qint32 baudRate = m_baudRateCombo->currentText().toInt();
        m_service->connectToPort(portName, baudRate);
    });
    connect(m_icScanButton, &QPushButton::clicked, this, [this]() {
        m_icScanButton->setEnabled(false);
        m_service->startI2cScan();
    });
    connect(m_icConnectButton, &QPushButton::clicked, this, [this]() {
        m_icConnectButton->setEnabled(false);
        const QString addrText = m_slaveIdCombo->currentText().trimmed();
        if (addrText.isEmpty()) {
            m_icConnectButton->setEnabled(true);
            qWarning() << "IC connect failed: no slave address selected";
            return;
        }
        bool ok = false;
        const uint addr = addrText.toUInt(&ok, 16);
        if (!ok || !DeviceContext::isValidSlaveAddress(addr)) {
            m_icConnectButton->setEnabled(true);
            qWarning().noquote() << QStringLiteral("IC connect failed: invalid address %1").arg(addrText);
            return;
        }
        m_service->setMotorIcAddress(static_cast<uint8_t>(addr));
    });
    connect(m_pmicConfigButton, &QPushButton::clicked, this, [this]() {
        m_pmicConfigButton->setEnabled(false);
        m_pmicDisableButton->setEnabled(false);
        m_service->configurePmic(m_drvddSpin->value(), m_iovddSpin->value(), m_vcmvddSpin->value());
    });
    connect(m_pmicDisableButton, &QPushButton::clicked, this, [this]() {
        m_pmicConfigButton->setEnabled(false);
        m_pmicDisableButton->setEnabled(false);
        m_service->disablePmic();
    });
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        m_resetButton->setEnabled(false);
        m_service->resetDevice();
    });
    connect(m_motorTestButton, &QPushButton::clicked, this, [this]() {
        m_motorTestButton->setEnabled(false);
        m_service->testMotor();
    });

    // IC type combo → DeviceContext
    connect(m_icCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        const MotorIcType type = DeviceContext::motorIcTypeFromIndex(index);
        qDebug().noquote() << QStringLiteral("IC type changed to %1").arg(DeviceContext::motorIcTypeToString(type));
        m_deviceContext->setIcType(type);
    });
    connect(m_deviceContext, &DeviceContext::icTypeChanged, this, [this](MotorIcType type) {
        const QSignalBlocker blocker(m_icCombo);
        m_icCombo->setCurrentIndex(DeviceContext::indexFromMotorIcType(type));
    });

    // Slave ID combo → DeviceContext
    connect(m_slaveIdCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text.isEmpty()) return;
        bool ok = false;
        const uint value = text.toUInt(&ok, 16);
        if (ok && DeviceContext::isValidSlaveAddress(value)) {
            qDebug().noquote() << QStringLiteral("Slave ID set to 0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
            m_deviceContext->setSlaveId(static_cast<uint8_t>(value));
            return;
        }
        if (!ok || !DeviceContext::isValidSlaveAddress(value)) qWarning().noquote() << QStringLiteral("Invalid slave ID: %1").arg(text);
    });
    connect(m_deviceContext, &DeviceContext::slaveIdChanged, this, [this](uint8_t id) {
        const QSignalBlocker blocker(m_slaveIdCombo);
        if (id == 0x00) { m_slaveIdCombo->setCurrentIndex(-1); return; }
        const QString text = QStringLiteral("0x%1").arg(id, 2, 16, QLatin1Char('0')).toUpper();
        const int index = m_slaveIdCombo->findText(text);
        if (index >= 0) m_slaveIdCombo->setCurrentIndex(index);
    });

    // Service → UI
    connect(m_service, &ConfigService::serialConnected, this, [this](const QString &port, qint32 baudRate) {
        m_connectButton->setEnabled(true);
        setSerialControlsConnected(true);
        emit serialConnected(port, baudRate);
    });
    connect(m_service, &ConfigService::serialDisconnected, this, [this]() {
        m_connectButton->setEnabled(true);
        setSerialControlsConnected(false);
        emit serialDisconnected();
    });
    connect(m_service, &ConfigService::serialError, this, [this](const QString &message) {
        Q_UNUSED(message);
        m_connectButton->setEnabled(true);
    });
    connect(m_service, &ConfigService::i2cScanResult, this, [this](const QList<uint8_t> &addresses) {
        m_icScanButton->setEnabled(true);
        const QSignalBlocker blocker(m_slaveIdCombo);
        m_slaveIdCombo->clear();
        for (uint8_t addr : addresses) {
            const QString addrStr = QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper();
            m_slaveIdCombo->addItem(addrStr);
        }
        if (m_slaveIdCombo->count() > 0) m_slaveIdCombo->setCurrentIndex(0);
    });
    connect(m_service, &ConfigService::setAddrSuccess, this, [this]() {
        m_icConnectButton->setEnabled(true);
        const QString addrText = m_slaveIdCombo->currentText().trimmed();
        bool ok = false;
        const uint addr = addrText.toUInt(&ok, 16);
        if (ok && DeviceContext::isValidSlaveAddress(addr)) m_deviceContext->setSlaveId(static_cast<uint8_t>(addr));
        qDebug().noquote() << QStringLiteral("Motor IC address set to %1").arg(addrText);
    });
    connect(m_service, &ConfigService::protocolError, this, [this](uint8_t errorCode) {
        Q_UNUSED(errorCode);
        m_icScanButton->setEnabled(m_service->isConnected());
        m_icConnectButton->setEnabled(m_service->isConnected());
        m_pmicConfigButton->setEnabled(true);
        m_pmicDisableButton->setEnabled(m_service->isConnected());
        m_resetButton->setEnabled(m_service->isConnected());
        m_motorTestButton->setEnabled(m_service->isConnected());
    });
    connect(m_service, &ConfigService::pmicConfigSuccess, this, [this]() {
        m_pmicConfigButton->setEnabled(true);
        m_pmicDisableButton->setEnabled(true);
        qDebug() << "PMIC configured successfully";
    });
    connect(m_service, &ConfigService::pmicConfigFailed, this, [this](const QString &reason) {
        m_pmicConfigButton->setEnabled(true);
        m_pmicDisableButton->setEnabled(true);
        qWarning().noquote() << QStringLiteral("PMIC configuration failed: %1").arg(reason);
    });
    connect(m_service, &ConfigService::pmicDisableSuccess, this, [this]() {
        m_pmicConfigButton->setEnabled(true);
        m_pmicDisableButton->setEnabled(true);
        qDebug() << "PMIC disabled successfully";
    });
    connect(m_service, &ConfigService::pmicDisableFailed, this, [this](const QString &reason) {
        m_pmicConfigButton->setEnabled(true);
        m_pmicDisableButton->setEnabled(true);
        qWarning().noquote() << QStringLiteral("PMIC disable failed: %1").arg(reason);
    });
    connect(m_service, &ConfigService::resetSuccess, this, [this]() {
        m_resetButton->setEnabled(true);
        qDebug() << "Device reset successful";
    });
    connect(m_service, &ConfigService::resetFailed, this, [this](const QString &reason) {
        m_resetButton->setEnabled(true);
        qWarning().noquote() << QStringLiteral("Device reset failed: %1").arg(reason);
    });
    connect(m_service, &ConfigService::motorTestSuccess, this, [this]() {
        m_motorTestButton->setEnabled(true);
        qDebug() << "Motor test successful";
    });
    connect(m_service, &ConfigService::motorTestFailed, this, [this](const QString &reason) {
        m_motorTestButton->setEnabled(true);
        qWarning().noquote() << QStringLiteral("Motor test failed: %1").arg(reason);
    });
}

void ConfigTab::refreshAvailablePorts() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = m_service->availablePorts();
    const QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) m_portCombo->setCurrentIndex(currentIndex);
    if (ports.isEmpty()) { qDebug() << "Scan found 0 ports"; return; }
    qDebug().noquote() << QStringLiteral("Scan found %1 ports: %2").arg(ports.size()).arg(ports.join(QStringLiteral(", ")));
}

void ConfigTab::setSerialControlsConnected(bool connected) {
    m_connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_portCombo->setEnabled(!connected);
    m_baudRateCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_icScanButton->setEnabled(connected);
    m_icConnectButton->setEnabled(connected);
    m_pmicConfigButton->setEnabled(connected);
    m_pmicDisableButton->setEnabled(connected);
    m_resetButton->setEnabled(connected);
    m_motorTestButton->setEnabled(connected);
}

void ConfigTab::setupUi() {
    setObjectName(QStringLiteral("ConfigTab"));

    auto *topLayout = new QHBoxLayout(this);
    topLayout->setObjectName(QStringLiteral("topLayout"));
    topLayout->setSpacing(0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    m_sidebarContent = new QWidget(this);
    m_sidebarContent->setObjectName(QStringLiteral("sidebarContent"));
    auto *sidebarLayout = new QVBoxLayout(m_sidebarContent);
    sidebarLayout->setObjectName(QStringLiteral("sidebarLayout"));
    sidebarLayout->setSpacing(8);
    sidebarLayout->setContentsMargins(10, 8, 10, 8);
    sidebarLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    auto *sidebar = new Sidebar(tr("配置"), m_sidebarContent, this);
    topLayout->addWidget(sidebar);

    m_mainContent = new QWidget(this);
    m_mainContent->setObjectName(QStringLiteral("mainContent"));
    auto *mainContentLayout = new QVBoxLayout(m_mainContent);
    mainContentLayout->setObjectName(QStringLiteral("mainContentLayout"));
    mainContentLayout->setSpacing(16);
    mainContentLayout->setContentsMargins(24, 24, 24, 24);
    topLayout->addWidget(m_mainContent);

    m_mainSplitter = new QSplitter(Qt::Vertical, m_mainContent);
    m_mainSplitter->setObjectName(QStringLiteral("mainSplitter"));
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(8);
    mainContentLayout->addWidget(m_mainSplitter);

    auto *upperArea = new QWidget(m_mainSplitter);
    upperArea->setObjectName(QStringLiteral("upperArea"));
    auto *upperLayout = new QGridLayout(upperArea);
    upperLayout->setObjectName(QStringLiteral("upperLayout"));
    upperLayout->setHorizontalSpacing(16);
    upperLayout->setVerticalSpacing(16);
    upperLayout->setContentsMargins(0, 0, 0, 0);

    auto makeCard = [&](QGroupBox *&group, const QString &objectName, const QString &title) {
        group = new QGroupBox(upperArea);
        group->setObjectName(objectName);
        group->setTitle(title);
        group->setProperty("panelRole", QStringLiteral("card"));
    };

    makeCard(m_icGroup, QStringLiteral("icGroup"), QStringLiteral("IC"));
    upperLayout->addWidget(m_icGroup, 0, 0);
    auto *icGroupLayout = new QVBoxLayout(m_icGroup);
    icGroupLayout->setObjectName(QStringLiteral("icGroupLayout"));
    icGroupLayout->setSpacing(10);
    icGroupLayout->setContentsMargins(24, 8, 24, 24);
    auto *icFormLayout = new QFormLayout();
    icFormLayout->setObjectName(QStringLiteral("icFormLayout"));
    icFormLayout->setHorizontalSpacing(16);
    icFormLayout->setVerticalSpacing(10);
    icGroupLayout->addLayout(icFormLayout);
    auto *icLabel = new QLabel(m_icGroup);
    icLabel->setObjectName(QStringLiteral("icLabel"));
    icLabel->setText(QStringLiteral("Select IC"));
    icFormLayout->setWidget(0, QFormLayout::LabelRole, icLabel);
    m_icCombo = new QComboBox(m_icGroup);
    m_icCombo->setObjectName(QStringLiteral("icCombo"));
    m_icCombo->setProperty("inputRole", QStringLiteral("form"));
    m_icCombo->addItems({QStringLiteral("AW86006"), QStringLiteral("DW9786"), QStringLiteral("DW9788")});
    icFormLayout->setWidget(0, QFormLayout::FieldRole, m_icCombo);
    auto *slaveIdLabel = new QLabel(m_icGroup);
    slaveIdLabel->setObjectName(QStringLiteral("slaveIdLabel"));
    slaveIdLabel->setText(QStringLiteral("Slave ID"));
    icFormLayout->setWidget(1, QFormLayout::LabelRole, slaveIdLabel);
    m_slaveIdCombo = new QComboBox(m_icGroup);
    m_slaveIdCombo->setObjectName(QStringLiteral("slaveIdCombo"));
    m_slaveIdCombo->setProperty("inputRole", QStringLiteral("form"));
    icFormLayout->setWidget(1, QFormLayout::FieldRole, m_slaveIdCombo);
    icGroupLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    auto *icButtonRow = new QHBoxLayout();
    icButtonRow->setObjectName(QStringLiteral("icButtonRow"));
    icButtonRow->setSpacing(10);
    icGroupLayout->addLayout(icButtonRow);
    m_icScanButton = new QPushButton(m_icGroup);
    m_icScanButton->setObjectName(QStringLiteral("icScanButton"));
    m_icScanButton->setMinimumSize(QSize(0, 32));
    m_icScanButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_icScanButton->setText(QStringLiteral("Scan"));
    m_icScanButton->setProperty("buttonRole", QStringLiteral("primary"));
    icButtonRow->addWidget(m_icScanButton);
    m_icConnectButton = new QPushButton(m_icGroup);
    m_icConnectButton->setObjectName(QStringLiteral("icConnectButton"));
    m_icConnectButton->setMinimumSize(QSize(0, 32));
    m_icConnectButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_icConnectButton->setText(QStringLiteral("Connect"));
    m_icConnectButton->setProperty("buttonRole", QStringLiteral("primary"));
    icButtonRow->addWidget(m_icConnectButton);
    icButtonRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    makeCard(m_serialGroup, QStringLiteral("serialGroup"), QStringLiteral("Serial"));
    upperLayout->addWidget(m_serialGroup, 0, 1);
    auto *serialGroupLayout = new QVBoxLayout(m_serialGroup);
    serialGroupLayout->setObjectName(QStringLiteral("serialGroupLayout"));
    serialGroupLayout->setSpacing(10);
    serialGroupLayout->setContentsMargins(24, 8, 24, 24);
    auto *serialFormLayout = new QFormLayout();
    serialFormLayout->setObjectName(QStringLiteral("serialFormLayout"));
    serialFormLayout->setHorizontalSpacing(16);
    serialFormLayout->setVerticalSpacing(10);
    serialGroupLayout->addLayout(serialFormLayout);
    auto *portLabel = new QLabel(m_serialGroup);
    portLabel->setObjectName(QStringLiteral("portLabel"));
    portLabel->setText(QStringLiteral("Port"));
    serialFormLayout->setWidget(0, QFormLayout::LabelRole, portLabel);
    m_portCombo = new QComboBox(m_serialGroup);
    m_portCombo->setObjectName(QStringLiteral("portCombo"));
    m_portCombo->setProperty("inputRole", QStringLiteral("form"));
    serialFormLayout->setWidget(0, QFormLayout::FieldRole, m_portCombo);
    auto *baudRateLabel = new QLabel(m_serialGroup);
    baudRateLabel->setObjectName(QStringLiteral("baudRateLabel"));
    baudRateLabel->setText(QStringLiteral("Baud Rate"));
    serialFormLayout->setWidget(1, QFormLayout::LabelRole, baudRateLabel);
    m_baudRateCombo = new QComboBox(m_serialGroup);
    m_baudRateCombo->setObjectName(QStringLiteral("baudRateCombo"));
    m_baudRateCombo->setProperty("inputRole", QStringLiteral("form"));
    m_baudRateCombo->addItems(Style::Serial::baudRateLabels());
    serialFormLayout->setWidget(1, QFormLayout::FieldRole, m_baudRateCombo);
    serialGroupLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    auto *serialButtonRow = new QHBoxLayout();
    serialButtonRow->setObjectName(QStringLiteral("serialButtonRow"));
    serialButtonRow->setSpacing(10);
    serialGroupLayout->addLayout(serialButtonRow);
    m_scanButton = new QPushButton(m_serialGroup);
    m_scanButton->setObjectName(QStringLiteral("scanButton"));
    m_scanButton->setMinimumSize(QSize(0, 32));
    m_scanButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_scanButton->setText(QStringLiteral("Scan"));
    m_scanButton->setProperty("buttonRole", QStringLiteral("primary"));
    serialButtonRow->addWidget(m_scanButton);
    m_connectButton = new QPushButton(m_serialGroup);
    m_connectButton->setObjectName(QStringLiteral("connectButton"));
    m_connectButton->setMinimumSize(QSize(0, 32));
    m_connectButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_connectButton->setText(QStringLiteral("Connect"));
    m_connectButton->setProperty("buttonRole", QStringLiteral("primary"));
    serialButtonRow->addWidget(m_connectButton);
    serialButtonRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    auto *serialActionRow = new QHBoxLayout();
    serialActionRow->setObjectName(QStringLiteral("serialActionRow"));
    serialActionRow->setSpacing(10);
    serialGroupLayout->addLayout(serialActionRow);
    m_resetButton = new QPushButton(m_serialGroup);
    m_resetButton->setObjectName(QStringLiteral("resetButton"));
    m_resetButton->setMinimumSize(QSize(0, 32));
    m_resetButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_resetButton->setText(tr("Reset Device"));
    m_resetButton->setProperty("buttonRole", QStringLiteral("secondary"));
    serialActionRow->addWidget(m_resetButton);
    m_motorTestButton = new QPushButton(m_serialGroup);
    m_motorTestButton->setObjectName(QStringLiteral("motorTestButton"));
    m_motorTestButton->setMinimumSize(QSize(0, 32));
    m_motorTestButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_motorTestButton->setText(tr("Motor Test"));
    m_motorTestButton->setProperty("buttonRole", QStringLiteral("secondary"));
    serialActionRow->addWidget(m_motorTestButton);
    serialActionRow->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    makeCard(m_pmicGroup, QStringLiteral("pmicGroup"), QStringLiteral("PMIC"));
    upperLayout->addWidget(m_pmicGroup, 0, 2);
    auto *pmicGroupLayout = new QVBoxLayout(m_pmicGroup);
    pmicGroupLayout->setObjectName(QStringLiteral("pmicGroupLayout"));
    pmicGroupLayout->setSpacing(10);
    pmicGroupLayout->setContentsMargins(24, 8, 24, 24);
    auto *pmicFormLayout = new QFormLayout();
    pmicFormLayout->setObjectName(QStringLiteral("pmicFormLayout"));
    pmicFormLayout->setHorizontalSpacing(16);
    pmicFormLayout->setVerticalSpacing(10);
    pmicGroupLayout->addLayout(pmicFormLayout);
    auto addVoltageRow = [&](int row, const QString &labelName, const QString &labelText, QDoubleSpinBox *&spin,
                             const QString &spinName, const QString &rowName, const QString &unitName, const QString &spacerName) {
        auto *label = new QLabel(m_pmicGroup);
        label->setObjectName(labelName);
        label->setText(labelText);
        pmicFormLayout->setWidget(row, QFormLayout::LabelRole, label);
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(rowName);
        rowLayout->setSpacing(10);
        spin = new QDoubleSpinBox(m_pmicGroup);
        spin->setObjectName(spinName);
        spin->setProperty("inputRole", QStringLiteral("form"));
        rowLayout->addWidget(spin);
        auto *unitLabel = new QLabel(m_pmicGroup);
        unitLabel->setObjectName(unitName);
        unitLabel->setText(QStringLiteral("V"));
        rowLayout->addWidget(unitLabel);
        auto *spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        Q_UNUSED(spacerName);
        rowLayout->addItem(spacer);
        pmicFormLayout->setLayout(row, QFormLayout::FieldRole, rowLayout);
    };
    addVoltageRow(0, QStringLiteral("drvddLabel"), QStringLiteral("DRVDD"), m_drvddSpin, QStringLiteral("drvddSpin"), QStringLiteral("drvddRow"), QStringLiteral("drvddUnitLabel"), QStringLiteral("drvddSpacer"));
    addVoltageRow(1, QStringLiteral("vcmvddLabel"), QStringLiteral("VCMVDD"), m_vcmvddSpin, QStringLiteral("vcmvddSpin"), QStringLiteral("vcmvddRow"), QStringLiteral("vcmvddUnitLabel"), QStringLiteral("vcmvddSpacer"));
    addVoltageRow(2, QStringLiteral("iovddLabel"), QStringLiteral("IOVDD"), m_iovddSpin, QStringLiteral("iovddSpin"), QStringLiteral("iovddRow"), QStringLiteral("iovddUnitLabel"), QStringLiteral("iovddSpacer"));
    pmicGroupLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    m_pmicConfigButton = new QPushButton(m_pmicGroup);
    m_pmicConfigButton->setObjectName(QStringLiteral("pmicConfigButton"));
    m_pmicConfigButton->setMinimumSize(QSize(0, 32));
    m_pmicConfigButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_pmicConfigButton->setText(tr("配置 PMIC"));
    m_pmicConfigButton->setProperty("buttonRole", QStringLiteral("primary"));
    auto *pmicButtonRow = new QHBoxLayout();
    pmicButtonRow->setObjectName(QStringLiteral("pmicButtonRow"));
    pmicButtonRow->setSpacing(10);
    pmicGroupLayout->addLayout(pmicButtonRow);
    pmicButtonRow->addWidget(m_pmicConfigButton);
    m_pmicDisableButton = new QPushButton(m_pmicGroup);
    m_pmicDisableButton->setObjectName(QStringLiteral("pmicDisableButton"));
    m_pmicDisableButton->setMinimumSize(QSize(0, 32));
    m_pmicDisableButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_pmicDisableButton->setText(tr("PMIC Off"));
    m_pmicDisableButton->setProperty("buttonRole", QStringLiteral("secondary"));
    pmicButtonRow->addWidget(m_pmicDisableButton);

    auto *lowerArea = new QWidget(m_mainSplitter);
    lowerArea->setObjectName(QStringLiteral("lowerArea"));
    auto *lowerLayout = new QVBoxLayout(lowerArea);
    lowerLayout->setObjectName(QStringLiteral("lowerLayout"));
    lowerLayout->setSpacing(16);
    lowerLayout->setContentsMargins(0, 0, 0, 0);
    auto *configFileRow = new QWidget(lowerArea);
    configFileRow->setObjectName(QStringLiteral("configFileRow"));
    auto *configFileLayout = new QHBoxLayout(configFileRow);
    configFileLayout->setObjectName(QStringLiteral("configFileLayout"));
    configFileLayout->setSpacing(10);
    configFileLayout->setContentsMargins(0, 0, 0, 0);
    lowerLayout->addWidget(configFileRow);
    auto *configFileLabel = new QLabel(configFileRow);
    configFileLabel->setObjectName(QStringLiteral("configFileLabel"));
    configFileLabel->setText(QStringLiteral("Config File"));
    configFileLayout->addWidget(configFileLabel);
    m_fileCombo = new QComboBox(configFileRow);
    m_fileCombo->setObjectName(QStringLiteral("fileCombo"));
    m_fileCombo->setEditable(true);
    m_fileCombo->setProperty("inputRole", QStringLiteral("form"));
    configFileLayout->addWidget(m_fileCombo);
    m_browseButton = new QPushButton(configFileRow);
    m_browseButton->setObjectName(QStringLiteral("browseButton"));
    m_browseButton->setMinimumSize(QSize(0, 32));
    m_browseButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_browseButton->setText(QStringLiteral("Browse"));
    m_browseButton->setProperty("buttonRole", QStringLiteral("secondary"));
    configFileLayout->addWidget(m_browseButton);
    m_writeButton = new QPushButton(configFileRow);
    m_writeButton->setObjectName(QStringLiteral("writeButton"));
    m_writeButton->setMinimumSize(QSize(0, 32));
    m_writeButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_writeButton->setText(QStringLiteral("Write"));
    m_writeButton->setProperty("buttonRole", QStringLiteral("primary"));
    configFileLayout->addWidget(m_writeButton);
    m_readButton = new QPushButton(configFileRow);
    m_readButton->setObjectName(QStringLiteral("readButton"));
    m_readButton->setMinimumSize(QSize(0, 32));
    m_readButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_readButton->setText(QStringLiteral("Read"));
    m_readButton->setProperty("buttonRole", QStringLiteral("primary"));
    configFileLayout->addWidget(m_readButton);
    lowerLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    for (int column = 0; column < 4; ++column) {
        upperLayout->setColumnStretch(column, 1);
    }
    for (int row = 0; row < 3; ++row) {
        upperLayout->setRowStretch(row, 1);
    }

    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 1);
}
