#include "tabs/configtab.h"

#include "devicecontext.h"
#include "serialmanager.h"
#include "ui/style_constants.h"
#include "widgets/sidebar.h"

#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSplitter>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QLabel *makeFormLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
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
        "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:11px; padding:0 12px; }")
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
        "QGroupBox { background:%1; border:%2px solid %3; margin-top:12px; color:%4; font-size:12px; font-weight:500; }"
        "QGroupBox::title { subcontrol-origin: margin; left:%5px; padding:0 4px; }")
                             .arg(Style::Color::PanelBackground.name())
                             .arg(Style::Size::BorderThin)
                             .arg(Style::Color::DefaultBorder.name())
                             .arg(Style::Color::TopBarValueText.name())
                             .arg(Style::Size::ContentPadding - 4));
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
    lowerLayout->addWidget(createConfigFileGroup());
    lowerLayout->addStretch();

    splitter->addWidget(upperArea);
    splitter->addWidget(lowerArea);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

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

    if (m_deviceContext->slaveId() != 0x00) {
        m_slaveIdEdit->setText(QStringLiteral("0x%1").arg(m_deviceContext->slaveId(), 2, 16, QLatin1Char('0')).toUpper());
    } else {
        m_slaveIdEdit->clear();
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

    connect(m_icCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        const MotorIcType type = motorIcTypeFromIndex(index);
        qDebug().noquote() << QStringLiteral("IC type changed to %1").arg(motorIcTypeToString(type));
        m_deviceContext->setIcType(type);
    });

    connect(m_deviceContext, &DeviceContext::icTypeChanged, this, [this](MotorIcType type) {
        const QSignalBlocker blocker(m_icCombo);
        m_icCombo->setCurrentIndex(indexFromMotorIcType(type));
    });

    connect(m_slaveIdEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString text = m_slaveIdEdit->text().trimmed();
        if (text.isEmpty()) {
            qDebug() << "Slave ID cleared";
            m_deviceContext->setSlaveId(0x00);
            return;
        }

        bool ok = false;
        const uint value = text.toUInt(&ok, 16);
        if (!ok || value > 0x7F) {
            qWarning().noquote() << QStringLiteral("Invalid slave ID: %1").arg(text);
            return;
        }

        qDebug().noquote() << QStringLiteral("Slave ID set to 0x%1")
                                  .arg(value, 2, 16, QLatin1Char('0'))
                                  .toUpper();
        m_deviceContext->setSlaveId(static_cast<uint8_t>(value));
    });

    connect(m_deviceContext, &DeviceContext::slaveIdChanged, this, [this](uint8_t id) {
        const QSignalBlocker blocker(m_slaveIdEdit);
        if (id == 0x00) {
            m_slaveIdEdit->clear();
            return;
        }

        m_slaveIdEdit->setText(QStringLiteral("0x%1").arg(id, 2, 16, QLatin1Char('0')).toUpper());
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
    layout->setAlignment(Qt::AlignTop);

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setHorizontalSpacing(Style::Size::ContentSpacing);
    formLayout->setVerticalSpacing(Style::Size::FormSpacing);

    m_icCombo = makeCombo({QStringLiteral("AW86006"), QStringLiteral("DW9786"), QStringLiteral("DW9788")}, group);
    formLayout->addRow(makeFormLabel(tr("Select IC"), group), m_icCombo);

    m_slaveIdEdit = new QLineEdit(group);
    m_slaveIdEdit->setPlaceholderText(QStringLiteral("0x18"));
    m_slaveIdEdit->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    m_slaveIdEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^(0x)?[0-9A-Fa-f]{1,2}$")),
        m_slaveIdEdit));
    m_slaveIdEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; }")
                                      .arg(Style::Color::White.name())
                                      .arg(Style::Size::BorderThin)
                                      .arg(Style::Color::InputBorder.name())
                                      .arg(Style::Color::TopBarValueText.name()));
    formLayout->addRow(makeFormLabel(QStringLiteral("Slave ID"), group), m_slaveIdEdit);

    layout->addLayout(formLayout);
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
    layout->setAlignment(Qt::AlignTop);

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
    layout->setAlignment(Qt::AlignTop);

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
    m_pmicConfigButton = makePrimaryButton(tr("配置 PMIC"), group);
    layout->addWidget(m_pmicConfigButton, 0, Qt::AlignLeft);
    return group;
}

QGroupBox *ConfigTab::createConfigFileGroup() {
    auto *group = makePanelGroup(tr("Config File"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::GroupBoxTopMargin,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    layout->setSpacing(Style::Size::ContentSpacing);
    layout->setAlignment(Qt::AlignTop);

    auto *row = new QHBoxLayout;
    row->setSpacing(Style::Size::FormSpacing);
    m_fileCombo = makeCombo({}, group);
    m_fileCombo->setEditable(true);
    m_fileCombo->setInsertPolicy(QComboBox::NoInsert);
    m_fileCombo->setPlaceholderText(tr("Select config file"));
    row->addWidget(m_fileCombo, 1);

    m_browseButton = new QPushButton(tr("Browse"), group);
    m_browseButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_browseButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:%1; border:%2px solid %3; border-radius:5px; color:%4; font-size:11px; padding:0 12px; }")
                                       .arg(Style::Color::White.name())
                                       .arg(Style::Size::BorderThin)
                                       .arg(Style::Color::DefaultBorder.name())
                                       .arg(Style::Color::TopBarValueText.name()));
    row->addWidget(m_browseButton);

    m_writeButton = makePrimaryButton(tr("Write"), group);
    m_readButton = makePrimaryButton(tr("Read"), group);
    row->addWidget(m_writeButton);
    row->addWidget(m_readButton);
    layout->addLayout(row);
    return group;
}
