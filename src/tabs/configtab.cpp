#include "tabs/configtab.h"

#include "devicecontext.h"
#include "serialmanager.h"
#include "ui/style_constants.h"
#include "widgets/sidebar.h"

#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QDebug>
#include <QSplitter>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
constexpr uint8_t CmdSetMotorIcAddr = 0x02;
constexpr uint8_t CmdErrorResponse = 0x01;
constexpr uint8_t CmdI2cBusScan = 0x07;
constexpr uint8_t I2cBusIndex = 0x02;

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
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        } else if (event->type() == QEvent::HoverLeave) {
            widget->setProperty("hovered", false);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        }

        return QObject::eventFilter(watched, event);
    }
};

QLabel *makeFormLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("color:%1; font-size:13px;")
                             .arg(Style::Color::TopBarValueText.name()));
    return label;
}

QComboBox *makeCombo(const QStringList &items, QWidget *parent) {
    auto *combo = new QComboBox(parent);
    combo->addItems(items);
    combo->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; }")
                             .arg(Style::Color::White.name())
                             .arg(Style::Size::BorderThin)
                             .arg(Style::Color::InputBorder.name())
                             .arg(Style::Color::TopBarValueText.name()));
    return combo;
}

QDoubleSpinBox *makeVoltageSpinBox(QWidget *parent) {
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setRange(0.0, 10.0);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.1);
    spinBox->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    spinBox->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width:16px; }")
                                .arg(Style::Color::White.name())
                                .arg(Style::Size::BorderThin)
                                .arg(Style::Color::InputBorder.name())
                                .arg(Style::Color::TopBarValueText.name()));
    return spinBox;
}

QPushButton *makePrimaryButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setFixedHeight(Style::Size::SidebarButtonHeight);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
        "QPushButton:hover { background:#D5E8C4; }"
        "QPushButton:disabled { background:#E6E6E6; border:%2px solid #C9C9C9; color:#9A9A9A; }"
        "QPushButton:pressed { background:#C0DD97; padding:1px 12px 0 12px; }")
                              .arg(Style::Color::LightGreen.name())
                              .arg(Style::Size::BorderThin)
                              .arg(Style::Color::PrimaryGreen.name())
                              .arg(Style::Color::PrimaryGreen.name()));
    return button;
}

void applyPanelShadow(QWidget *widget) {
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setOffset(0, 1);
    effect->setBlurRadius(6);
    effect->setColor(QColor(44, 44, 42, 38));
    widget->setGraphicsEffect(effect);
}

QGroupBox *makePanelGroup(const QString &title, QWidget *parent) {
    auto *group = new QGroupBox(title, parent);
    group->setStyleSheet(QStringLiteral(
        "QGroupBox { background:%1; border:%2px solid %3; padding-top:28px; color:%4; font-size:14px; font-weight:500; }"
        "QGroupBox[hovered=\"true\"] { background:#f5f5f2; }"
        "QGroupBox::title { subcontrol-origin: padding; subcontrol-position: top center; padding:8px 0 4px 0; }")
                             .arg(Style::Color::PanelBackground.name())
                             .arg(Style::Size::BorderThin)
                             .arg(Style::Color::DefaultBorder.name())
                             .arg(Style::Color::TopBarValueText.name()));
    group->setAttribute(Qt::WA_Hover, true);
    group->setProperty("hovered", false);
    group->installEventFilter(new GroupHoverFilter(group));
    applyPanelShadow(group);
    return group;
}

MotorIcType motorIcTypeFromIndex(int index) {
    switch (index) {
    case 1:
        return MotorIcType::DW9786;
    case 2:
        return MotorIcType::DW9788;
    case 0:
    default:
        return MotorIcType::AW86006;
    }
}

int indexFromMotorIcType(MotorIcType type) {
    switch (type) {
    case MotorIcType::DW9786:
        return 1;
    case MotorIcType::DW9788:
        return 2;
    case MotorIcType::AW86006:
    default:
        return 0;
    }
}

QString motorIcTypeToString(MotorIcType type) {
    switch (type) {
    case MotorIcType::AW86006:
        return QStringLiteral("AW86006");
    case MotorIcType::DW9786:
        return QStringLiteral("DW9786");
    case MotorIcType::DW9788:
        return QStringLiteral("DW9788");
    }

    return QStringLiteral("Unknown");
}
}

ConfigTab::ConfigTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent)
    : QWidget(parent)
    , m_serialManager(serialManager)
    , m_deviceContext(deviceContext) {
    qRegisterMetaType<uint8_t>("uint8_t");
    setupUi();
    connectSignals();
}

void ConfigTab::setupUi() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *sidebarContent = new QWidget;
    auto *sidebarLayout = new QVBoxLayout(sidebarContent);
    sidebarLayout->setContentsMargins(
        Style::Size::SidebarContentHorizontalPadding,
        Style::Size::SidebarContentVerticalPadding,
        Style::Size::SidebarContentHorizontalPadding,
        Style::Size::SidebarContentVerticalPadding);
    sidebarLayout->setSpacing(Style::Size::SidebarSectionSpacing);
    sidebarLayout->addStretch();

    layout->addWidget(new Sidebar(tr("配置"), sidebarContent, this));

    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    contentLayout->setSpacing(Style::Size::ContentSpacing);

    auto *splitter = new QSplitter(Qt::Vertical, contentWidget);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(Style::Size::SplitterHandleWidth);
    splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background:%1; }")
                                .arg(Style::Color::WindowBackground.name()));

    auto *upperArea = new QWidget(splitter);
    auto *upperLayout = new QGridLayout(upperArea);
    upperLayout->setContentsMargins(0, 0, 0, 0);
    upperLayout->setHorizontalSpacing(Style::Size::ContentSpacing);
    upperLayout->setVerticalSpacing(Style::Size::ContentSpacing);

    upperLayout->addWidget(createIcGroup(), 0, 0);
    upperLayout->addWidget(createSerialGroup(), 0, 1);
    upperLayout->addWidget(createPmicGroup(), 0, 2);
    for (int column = 0; column < 4; ++column) {
        upperLayout->setColumnStretch(column, 1);
    }
    for (int row = 0; row < 3; ++row) {
        upperLayout->setRowStretch(row, 1);
    }

    auto *lowerArea = new QWidget(splitter);
    auto *lowerLayout = new QVBoxLayout(lowerArea);
    lowerLayout->setContentsMargins(0, 0, 0, 0);
    lowerLayout->setSpacing(Style::Size::ContentSpacing);
    lowerLayout->addWidget(createConfigFileRow());
    lowerLayout->addStretch();

    splitter->addWidget(upperArea);
    splitter->addWidget(lowerArea);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    contentLayout->addWidget(splitter);
    layout->addWidget(contentWidget, 1);
}

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

    connect(m_scanButton, &QPushButton::clicked, this, [this]() {
        qDebug() << "Scan started";
        refreshAvailablePorts();
    });

    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_serialManager == nullptr) {
            return;
        }

        if (m_isSerialConnected) {
            qDebug() << "Disconnecting...";
            QMetaObject::invokeMethod(m_serialManager, "closePort", Qt::QueuedConnection);
            return;
        }

        const QString portName = m_portCombo->currentText().trimmed();
        if (portName.isEmpty()) {
            qWarning() << "Connect failed: no port selected";
            return;
        }

        const qint32 baudRate = m_baudRateCombo->currentText().toInt();
        qDebug().noquote() << QStringLiteral("Connecting to %1 at %2...")
                                  .arg(portName)
                                  .arg(baudRate);
        QMetaObject::invokeMethod(
            m_serialManager,
            "openPort",
            Qt::QueuedConnection,
            Q_ARG(QString, portName),
            Q_ARG(qint32, baudRate));
    });

    connect(m_serialManager, &SerialManager::connected, this, [this]() {
        qDebug() << "Serial connected";
        setSerialControlsConnected(true);
    });

    connect(m_serialManager, &SerialManager::disconnected, this, [this]() {
        qDebug() << "Serial disconnected";
        setSerialControlsConnected(false);
    });

    connect(m_serialManager, &SerialManager::errorOccurred, this, [](const QString &message) {
        qWarning().noquote() << QStringLiteral("Serial error: %1").arg(message);
    });
    connect(m_serialManager, &SerialManager::frameReceived, this, &ConfigTab::onFrameReceived);

    connect(m_icCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        const MotorIcType type = motorIcTypeFromIndex(index);
        qDebug().noquote() << QStringLiteral("IC type changed to %1").arg(motorIcTypeToString(type));
        m_deviceContext->setIcType(type);
    });

    connect(m_deviceContext, &DeviceContext::icTypeChanged, this, [this](MotorIcType type) {
        const QSignalBlocker blocker(m_icCombo);
        m_icCombo->setCurrentIndex(indexFromMotorIcType(type));
    });

    connect(m_icScanButton, &QPushButton::clicked, this, [this]() {
        if (m_serialManager == nullptr || !m_isSerialConnected) {
            qWarning() << "I2C scan failed: serial not connected";
            return;
        }

        qDebug() << "I2C scan started on bus I2C2";
        QByteArray data;
        data.append(static_cast<char>(I2cBusIndex));
        QMetaObject::invokeMethod(
            m_serialManager,
            "sendCommand",
            Qt::QueuedConnection,
            Q_ARG(uint8_t, CmdI2cBusScan),
            Q_ARG(QByteArray, data));
    });

    connect(m_icConnectButton, &QPushButton::clicked, this, [this]() {
        if (m_serialManager == nullptr || !m_isSerialConnected) {
            qWarning() << "IC connect failed: serial not connected";
            return;
        }

        const QString addrText = m_slaveIdCombo->currentText().trimmed();
        if (addrText.isEmpty()) {
            qWarning() << "IC connect failed: no slave address selected";
            return;
        }

        bool ok = false;
        const uint addr = addrText.toUInt(&ok, 16);
        if (!ok || addr == 0 || addr > 0x7F) {
            qWarning().noquote() << QStringLiteral("IC connect failed: invalid address %1").arg(addrText);
            return;
        }

        qDebug().noquote() << QStringLiteral("Setting motor IC address to 0x%1")
                                  .arg(addr, 2, 16, QLatin1Char('0'))
                                  .toUpper();
        QByteArray data;
        data.append(static_cast<char>(addr));
        QMetaObject::invokeMethod(
            m_serialManager,
            "sendCommand",
            Qt::QueuedConnection,
            Q_ARG(uint8_t, CmdSetMotorIcAddr),
            Q_ARG(QByteArray, data));
    });

    connect(m_slaveIdCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            return;
        }

        bool ok = false;
        const uint value = text.toUInt(&ok, 16);
        if (ok && value <= 0x7F) {
            qDebug().noquote() << QStringLiteral("Slave ID set to 0x%1")
                                      .arg(value, 2, 16, QLatin1Char('0'))
                                      .toUpper();
            m_deviceContext->setSlaveId(static_cast<uint8_t>(value));
            return;
        }

        if (!ok || value > 0x7F) {
            qWarning().noquote() << QStringLiteral("Invalid slave ID: %1").arg(text);
        }
    });

    connect(m_deviceContext, &DeviceContext::slaveIdChanged, this, [this](uint8_t id) {
        const QSignalBlocker blocker(m_slaveIdCombo);
        if (id == 0x00) {
            m_slaveIdCombo->setCurrentIndex(-1);
            return;
        }

        const QString text = QStringLiteral("0x%1").arg(id, 2, 16, QLatin1Char('0')).toUpper();
        const int index = m_slaveIdCombo->findText(text);
        if (index >= 0) {
            m_slaveIdCombo->setCurrentIndex(index);
        }
    });
}

void ConfigTab::refreshAvailablePorts() {
    const QString currentPort = m_portCombo->currentText();
    const QStringList ports = SerialManager::availablePorts();

    const QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();
    m_portCombo->addItems(ports);

    const int currentIndex = m_portCombo->findText(currentPort);
    if (currentIndex >= 0) {
        m_portCombo->setCurrentIndex(currentIndex);
    }

    if (ports.isEmpty()) {
        qDebug() << "Scan found 0 ports";
        return;
    }

    qDebug().noquote() << QStringLiteral("Scan found %1 ports: %2")
                              .arg(ports.size())
                              .arg(ports.join(QStringLiteral(", ")));
}

void ConfigTab::setSerialControlsConnected(bool connected) {
    m_isSerialConnected = connected;
    m_connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_portCombo->setEnabled(!connected);
    m_baudRateCombo->setEnabled(!connected);
    m_scanButton->setEnabled(!connected);
    m_icScanButton->setEnabled(connected);
    m_icConnectButton->setEnabled(connected);
}

void ConfigTab::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);

    switch (cmd) {
    case CmdI2cBusScan:
        handleScanResponse(data);
        break;
    case CmdSetMotorIcAddr:
        handleSetAddrResponse();
        break;
    case CmdErrorResponse:
        handleErrorResponse(data);
        break;
    default:
        qDebug().noquote() << QStringLiteral("Unhandled frame: cmd=0x%1")
                                  .arg(cmd, 2, 16, QLatin1Char('0'))
                                  .toUpper();
        break;
    }
}

void ConfigTab::handleScanResponse(const QByteArray &data) {
    if (data.isEmpty()) {
        qWarning() << "I2C scan response: empty data";
        return;
    }

    const uint8_t count = static_cast<uint8_t>(data.at(0));
    qDebug().noquote() << QStringLiteral("I2C scan found %1 devices").arg(count);

    const QSignalBlocker blocker(m_slaveIdCombo);
    m_slaveIdCombo->clear();

    for (int i = 1; i <= count && i < data.size(); ++i) {
        const uint8_t addr = static_cast<uint8_t>(data.at(i));
        const QString addrStr = QStringLiteral("0x%1").arg(addr, 2, 16, QLatin1Char('0')).toUpper();
        m_slaveIdCombo->addItem(addrStr);
        qDebug().noquote() << QStringLiteral("  Device: %1").arg(addrStr);
    }

    if (count == 0) {
        qDebug() << "No I2C devices found";
    } else if (m_slaveIdCombo->count() > 0) {
        m_slaveIdCombo->setCurrentIndex(0);
    }
}

void ConfigTab::handleSetAddrResponse() {
    const QString addrText = m_slaveIdCombo->currentText().trimmed();
    bool ok = false;
    const uint addr = addrText.toUInt(&ok, 16);
    if (ok && addr <= 0x7F) {
        m_deviceContext->setSlaveId(static_cast<uint8_t>(addr));
    }

    qDebug().noquote() << QStringLiteral("Motor IC address set to %1").arg(addrText);
}

void ConfigTab::handleErrorResponse(const QByteArray &data) {
    uint8_t errorCode = 0;
    if (!data.isEmpty()) {
        errorCode = static_cast<uint8_t>(data.at(0));
    }

    QString errorName;
    switch (errorCode) {
    case 0x01:
        errorName = QStringLiteral("CRC check failed");
        break;
    case 0x02:
        errorName = QStringLiteral("Unknown command");
        break;
    case 0x03:
        errorName = QStringLiteral("Execution failed");
        break;
    default:
        errorName = QStringLiteral("Unknown error");
        break;
    }

    qWarning().noquote() << QStringLiteral("Error response: code=0x%1 (%2)")
                                .arg(errorCode, 2, 16, QLatin1Char('0'))
                                .toUpper()
                                .arg(errorName);
}

QGroupBox *ConfigTab::createIcGroup() {
    auto *group = makePanelGroup(tr("IC"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::GroupBoxTopMargin,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    layout->setSpacing(Style::Size::FormSpacing);

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setHorizontalSpacing(Style::Size::ContentSpacing);
    formLayout->setVerticalSpacing(Style::Size::FormSpacing);

    m_icCombo = makeCombo({QStringLiteral("AW86006"), QStringLiteral("DW9786"), QStringLiteral("DW9788")}, group);
    formLayout->addRow(makeFormLabel(tr("Select IC"), group), m_icCombo);

    m_slaveIdCombo = makeCombo({}, group);
    m_slaveIdCombo->setPlaceholderText(QStringLiteral("Scan first"));
    m_slaveIdCombo->setCurrentIndex(-1);
    formLayout->addRow(makeFormLabel(QStringLiteral("Slave ID"), group), m_slaveIdCombo);

    layout->addLayout(formLayout);
    layout->addStretch();

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(Style::Size::FormSpacing);
    m_icScanButton = makePrimaryButton(tr("Scan"), group);
    m_icConnectButton = makePrimaryButton(tr("Connect"), group);
    buttonRow->addWidget(m_icScanButton);
    buttonRow->addWidget(m_icConnectButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    return group;
}

QGroupBox *ConfigTab::createSerialGroup() {
    auto *group = makePanelGroup(tr("Serial"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::GroupBoxTopMargin,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    layout->setSpacing(Style::Size::FormSpacing);

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setHorizontalSpacing(Style::Size::ContentSpacing);
    formLayout->setVerticalSpacing(Style::Size::FormSpacing);

    m_portCombo = makeCombo({}, group);
    m_portCombo->setPlaceholderText(tr("Select COM"));
    m_baudRateCombo = makeCombo(
        {QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"),
         QStringLiteral("115200"), QStringLiteral("230400"), QStringLiteral("460800"), QStringLiteral("921600")},
        group);
    m_baudRateCombo->setCurrentText(QStringLiteral("115200"));

    formLayout->addRow(makeFormLabel(tr("Port"), group), m_portCombo);
    formLayout->addRow(makeFormLabel(tr("Baud Rate"), group), m_baudRateCombo);
    layout->addLayout(formLayout);
    layout->addStretch();

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(Style::Size::FormSpacing);
    m_scanButton = makePrimaryButton(tr("Scan"), group);
    m_connectButton = makePrimaryButton(tr("Connect"), group);
    buttonRow->addWidget(m_scanButton);
    buttonRow->addWidget(m_connectButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);
    return group;
}

QGroupBox *ConfigTab::createPmicGroup() {
    auto *group = makePanelGroup(tr("PMIC"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::GroupBoxTopMargin,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    layout->setSpacing(Style::Size::FormSpacing);

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setHorizontalSpacing(Style::Size::ContentSpacing);
    formLayout->setVerticalSpacing(Style::Size::FormSpacing);

    const QList<QString> labels = {
        QStringLiteral("DRVDD"),
        QStringLiteral("VCMVDD"),
        QStringLiteral("IOVDD")};
    QList<QDoubleSpinBox **> fields = {
        &m_drvddSpin,
        &m_vcmvddSpin,
        &m_iovddSpin};

    for (int i = 0; i < labels.size(); ++i) {
        auto *fieldRow = new QHBoxLayout;
        fieldRow->setSpacing(Style::Size::FormSpacing);
        *fields[i] = makeVoltageSpinBox(group);
        fieldRow->addWidget(*fields[i], 1);
        fieldRow->addWidget(new QLabel(QStringLiteral("V"), group));
        fieldRow->addStretch();
        formLayout->addRow(makeFormLabel(labels[i], group), fieldRow);
    }

    layout->addLayout(formLayout);
    layout->addStretch();
    m_pmicConfigButton = makePrimaryButton(tr("配置 PMIC"), group);
    layout->addWidget(m_pmicConfigButton, 0, Qt::AlignLeft);
    return group;
}

QWidget *ConfigTab::createConfigFileRow() {
    auto *rowWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(rowWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Style::Size::FormSpacing);

    auto *label = makeFormLabel(tr("Config File"), rowWidget);
    layout->addWidget(label);

    m_fileCombo = makeCombo({}, rowWidget);
    m_fileCombo->setEditable(true);
    m_fileCombo->setInsertPolicy(QComboBox::NoInsert);
    m_fileCombo->setPlaceholderText(tr("Select config file"));
    layout->addWidget(m_fileCombo, 1);

    m_browseButton = new QPushButton(tr("Browse"), rowWidget);
    m_browseButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_browseButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:13px; padding:0 12px; }"
        "QPushButton:hover { background:#f0f0f0; }"
        "QPushButton:pressed { background:#e0e0e0; padding:1px 12px 0 12px; }")
                                       .arg(Style::Color::White.name())
                                       .arg(Style::Size::BorderThin)
                                       .arg(Style::Color::DefaultBorder.name())
                                       .arg(Style::Color::TopBarValueText.name()));
    layout->addWidget(m_browseButton);

    m_writeButton = makePrimaryButton(tr("Write"), rowWidget);
    m_readButton = makePrimaryButton(tr("Read"), rowWidget);
    layout->addWidget(m_writeButton);
    layout->addWidget(m_readButton);

    return rowWidget;
}
