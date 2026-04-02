#include "tabs/registerrwtab.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "ui/style_constants.h"
#include "widgets/registertable.h"
#include "widgets/sidebar.h"

#include <QButtonGroup>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDebug>
#include <QMetaObject>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {
QString makePrimarySidebarButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:%2px solid %3; color:%3; font-size:11px; border-radius:5px; }"
               "QPushButton:hover { background:%3; color:%4; }"
               "QPushButton:disabled { background:%5; border:%2px solid %6; color:%7; }")
        .arg(Style::Color::LightGreen.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::PrimaryGreen.name())
        .arg(Style::Color::White.name())
        .arg(Style::Color::TopBarBackground.name())
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::MutedText.name());
}

QString makeWriteSidebarButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:%2px solid %3; color:%3; font-size:11px; border-radius:5px; }"
               "QPushButton:hover { background:%3; color:%4; }"
               "QPushButton:disabled { background:%5; border:%2px solid %6; color:%7; }")
        .arg(Style::Color::WriteButtonBackground.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::WriteButtonForeground.name())
        .arg(Style::Color::White.name())
        .arg(Style::Color::TopBarBackground.name())
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::MutedText.name());
}

QString makeModeButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:%2px solid %3; color:%4; font-size:11px; border-radius:4px; padding:0 8px; }"
               "QPushButton:hover { border:%2px solid %5; color:%5; }"
               "QPushButton:checked { background:%6; border:%2px solid %5; color:%5; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(Style::Color::PrimaryGreen.name())
        .arg(Style::Color::LightGreen.name());
}

QString makeSecondaryButtonStyle() {
    return QStringLiteral(
               "QPushButton { background:%1; border:%2px solid %3; color:%4; font-size:11px; border-radius:5px; padding:0 10px; }"
               "QPushButton:hover { background:%5; }"
               "QPushButton:pressed { background:%6; padding:1px 10px 0 10px; }"
               "QPushButton:disabled { background:%7; border:%2px solid %8; color:%9; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(Style::Color::SecondaryButtonHover.name())
        .arg(Style::Color::SecondaryButtonPressed.name())
        .arg(Style::Color::DisabledBackground.name())
        .arg(Style::Color::DisabledBorder.name())
        .arg(Style::Color::DisabledText.name());
}

QString makePanelGroupStyle() {
    return QStringLiteral(
               "QGroupBox { background:%1; border:%2px solid %3; padding-top:24px; color:%4; font-size:13px; font-weight:500; }"
               "QGroupBox::title { subcontrol-origin: padding; subcontrol-position: top left; padding:6px 10px 4px 10px; }")
        .arg(Style::Color::PanelBackground.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::TopBarValueText.name());
}

QString makeBatchLineEditStyle(bool readOnly) {
    return QStringLiteral(
               "QLineEdit { background:%1; border:%2px solid %3; padding:4px 6px; color:%4; }"
               "QLineEdit:read-only { background:%5; color:%6; }")
        .arg(Style::Color::White.name())
        .arg(Style::Size::BorderThin)
        .arg(Style::Color::InputBorder.name())
        .arg(Style::Color::TopBarValueText.name())
        .arg(readOnly ? Style::Color::TopBarBackground.name() : Style::Color::White.name())
        .arg(Style::Color::MutedText.name());
}
} // namespace

RegisterRwTab::RegisterRwTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent)
    , m_serialManager(serialManager) {
    setupUi();
    connectSignals();
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_registerTable->loadConfig(configFilePath());
}

void RegisterRwTab::setupUi() {
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

    m_readAllButton = new QPushButton(tr("全部读取"), sidebarContent);
    m_readAllButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_readAllButton->setStyleSheet(makePrimarySidebarButtonStyle());

    m_writeAllButton = new QPushButton(tr("全部写入"), sidebarContent);
    m_writeAllButton->setFixedHeight(Style::Size::SidebarButtonHeight);
    m_writeAllButton->setStyleSheet(makeWriteSidebarButtonStyle());

    auto *valueModeLabel = new QLabel(tr("数值表示"), sidebarContent);
    valueModeLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px;")
                                      .arg(Style::Color::MutedText.name()));

    auto *modeRow = new QHBoxLayout;
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(Style::Size::FormSpacing);
    m_decButton = new QPushButton(QStringLiteral("DEC"), sidebarContent);
    m_hexButton = new QPushButton(QStringLiteral("HEX"), sidebarContent);
    m_decButton->setCheckable(true);
    m_hexButton->setCheckable(true);
    m_decButton->setStyleSheet(makeModeButtonStyle());
    m_hexButton->setStyleSheet(makeModeButtonStyle());

    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_decButton);
    modeGroup->addButton(m_hexButton);
    m_hexButton->setChecked(true);

    modeRow->addWidget(m_decButton);
    modeRow->addWidget(m_hexButton);

    sidebarLayout->addWidget(m_readAllButton);
    sidebarLayout->addWidget(m_writeAllButton);
    sidebarLayout->addSpacing(Style::Size::FormSpacing);
    sidebarLayout->addWidget(valueModeLabel);
    sidebarLayout->addLayout(modeRow);
    sidebarLayout->addStretch();

    layout->addWidget(new Sidebar(tr("读写"), sidebarContent, this));

    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);

    auto *mainSplitter = new QSplitter(Qt::Vertical, contentWidget);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setHandleWidth(Style::Size::SplitterHandleWidth);
    mainSplitter->setStyleSheet(QStringLiteral("QSplitter::handle { background:%1; }")
                                    .arg(Style::Color::WindowBackground.name()));

    auto *topWidget = new QWidget(mainSplitter);
    auto *topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    m_registerTable = new RegisterTable(topWidget);
    topLayout->addWidget(m_registerTable, 1);

    auto *bottomWidget = new QWidget(mainSplitter);
    auto *bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(Style::Size::ContentSpacing);

    auto *batchGroup = new QGroupBox(tr("批量读写"), bottomWidget);
    batchGroup->setStyleSheet(makePanelGroupStyle());
    auto *batchLayout = new QVBoxLayout(batchGroup);
    batchLayout->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    batchLayout->setSpacing(Style::Size::FormSpacing);

    for (int index = 0; index < 4; ++index) {
        auto *rowLayout = new QHBoxLayout;
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(Style::Size::FormSpacing);

        const QString actionText = index < 2 ? tr("批量写入") : tr("批量读出");
        m_batchBtn[index] = new QPushButton(actionText, batchGroup);
        m_batchBtn[index]->setFixedHeight(Style::Size::SidebarButtonHeight);
        m_batchBtn[index]->setFixedWidth(Style::Size::LanguageComboWidth);
        m_batchBtn[index]->setStyleSheet(makePrimarySidebarButtonStyle());

        m_batchDescEdit[index] = new QLineEdit(batchGroup);
        m_batchDescEdit[index]->setMinimumHeight(Style::Size::SidebarComboMinHeight);
        m_batchDescEdit[index]->setStyleSheet(makeBatchLineEditStyle(false));

        m_batchPathEdit[index] = new QLineEdit(batchGroup);
        m_batchPathEdit[index]->setReadOnly(true);
        m_batchPathEdit[index]->setMinimumHeight(Style::Size::SidebarComboMinHeight);
        m_batchPathEdit[index]->setStyleSheet(makeBatchLineEditStyle(true));

        m_batchBrowseBtn[index] = new QPushButton(tr("浏览"), batchGroup);
        m_batchBrowseBtn[index]->setFixedHeight(Style::Size::SidebarButtonHeight);
        m_batchBrowseBtn[index]->setFixedWidth(Style::Size::LanguageComboWidth);
        m_batchBrowseBtn[index]->setStyleSheet(makeSecondaryButtonStyle());

        rowLayout->addWidget(m_batchBtn[index]);
        rowLayout->addWidget(m_batchDescEdit[index], 1);
        rowLayout->addWidget(m_batchPathEdit[index], 1);
        rowLayout->addWidget(m_batchBrowseBtn[index]);
        batchLayout->addLayout(rowLayout);
    }
    batchLayout->addStretch();

    auto *placeholderGroup1 = new QGroupBox(QString(), bottomWidget);
    placeholderGroup1->setStyleSheet(makePanelGroupStyle());
    auto *placeholderLayout1 = new QVBoxLayout(placeholderGroup1);
    placeholderLayout1->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    placeholderLayout1->addStretch();

    auto *placeholderGroup2 = new QGroupBox(QString(), bottomWidget);
    placeholderGroup2->setStyleSheet(makePanelGroupStyle());
    auto *placeholderLayout2 = new QVBoxLayout(placeholderGroup2);
    placeholderLayout2->setContentsMargins(
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding,
        Style::Size::ContentPadding);
    placeholderLayout2->addStretch();

    bottomLayout->addWidget(batchGroup, 5);
    bottomLayout->addWidget(placeholderGroup1, 3);
    bottomLayout->addWidget(placeholderGroup2, 3);

    mainSplitter->addWidget(topWidget);
    mainSplitter->addWidget(bottomWidget);
    mainSplitter->setStretchFactor(0, 5);
    mainSplitter->setStretchFactor(1, 3);

    contentLayout->addWidget(mainSplitter, 1);
    layout->addWidget(contentWidget, 1);
}

void RegisterRwTab::connectSignals() {
    connect(m_registerTable, &RegisterTable::readRowRequested, this, &RegisterRwTab::onReadRowRequested);
    connect(m_registerTable, &RegisterTable::writeRowRequested, this, &RegisterRwTab::onWriteRowRequested);
    connect(m_registerTable, &RegisterTable::configChanged, this, &RegisterRwTab::onConfigChanged);
    connect(m_readAllButton, &QPushButton::clicked, this, &RegisterRwTab::onReadAllClicked);
    connect(m_writeAllButton, &QPushButton::clicked, this, &RegisterRwTab::onWriteAllClicked);
    connect(m_hexButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_registerTable->setValueMode(RegisterTable::ValueMode::Hex);
        }
    });
    connect(m_decButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_registerTable->setValueMode(RegisterTable::ValueMode::Dec);
        }
    });

    if (m_serialManager != nullptr) {
        connect(m_serialManager, &SerialManager::frameReceived, this, &RegisterRwTab::onFrameReceived);
        connect(m_serialManager, &SerialManager::errorOccurred, this, &RegisterRwTab::onSerialError);
    }
    connect(m_timeoutTimer, &QTimer::timeout, this, &RegisterRwTab::onTimeout);
}

void RegisterRwTab::setConnected(bool connected) {
    m_readAllButton->setEnabled(connected);
    m_writeAllButton->setEnabled(connected);
    m_registerTable->setConnected(connected);
    if (!connected) {
        if (m_timeoutTimer != nullptr) {
            m_timeoutTimer->stop();
        }
        m_pendingRow = -1;
        m_pendingQueue.clear();
    }
}

void RegisterRwTab::onReadRowRequested(int globalRow, quint16 addr) {
    if (m_serialManager == nullptr) {
        return;
    }

    qDebug().noquote() << QStringLiteral("[RW] Read  row=%1 addr=0x%2")
                              .arg(globalRow)
                              .arg(addr, 4, 16, QLatin1Char('0'))
                              .toUpper();
    m_pendingQueue.clear();
    m_pendingRow = globalRow;
    m_isWriteOp = false;
    m_registerTable->markRowPending(globalRow);

    QMetaObject::invokeMethod(
        m_serialManager,
        "sendCommand",
        Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdReadRegister),
        Q_ARG(QByteArray, MotorProtocol::encodeReadRegister(addr)));
    m_timeoutTimer->start(500);
}

void RegisterRwTab::onWriteRowRequested(int globalRow, quint16 addr, qint16 value) {
    if (m_serialManager == nullptr) {
        return;
    }

    qDebug().noquote() << QStringLiteral("[RW] Write row=%1 addr=0x%2 val=0x%3")
                              .arg(globalRow)
                              .arg(addr, 4, 16, QLatin1Char('0'))
                              .arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0'))
                              .toUpper();
    m_pendingQueue.clear();
    m_pendingRow = globalRow;
    m_isWriteOp = true;

    QMetaObject::invokeMethod(
        m_serialManager,
        "sendCommand",
        Qt::QueuedConnection,
        Q_ARG(uint8_t, MotorProtocol::CmdWriteRegister),
        Q_ARG(QByteArray, MotorProtocol::encodeWriteRegister(addr, value)));
    m_timeoutTimer->start(500);
}

void RegisterRwTab::onReadAllClicked() {
    m_pendingQueue.clear();
    m_isWriteOp = false;
    for (int row = 0; row < Style::Size::TableGroupCount * Style::Size::TableRowCount; ++row) {
        if (m_registerTable->rowHasAddress(row)) {
            m_pendingQueue.enqueue(row);
        }
    }
    processNextInQueue();
}

void RegisterRwTab::onWriteAllClicked() {
    m_pendingQueue.clear();
    m_isWriteOp = true;
    for (int row = 0; row < Style::Size::TableGroupCount * Style::Size::TableRowCount; ++row) {
        if (m_registerTable->rowHasAddress(row) && m_registerTable->rowHasValue(row)) {
            m_pendingQueue.enqueue(row);
        }
    }
    processNextInQueue();
}

void RegisterRwTab::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);

    if (m_pendingRow < 0) {
        return;
    }

    if (cmd == MotorProtocol::CmdReadRegister && !m_isWriteOp) {
        m_timeoutTimer->stop();
        qint16 value = 0;
        if (MotorProtocol::decodeReadRegisterResponse(data, &value)) {
            qDebug().noquote() << QStringLiteral("[RW] Read  OK   row=%1 val=0x%2")
                                      .arg(m_pendingRow)
                                      .arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0'))
                                      .toUpper();
            m_registerTable->updateRowValue(m_pendingRow, value);
        } else {
            qDebug().noquote() << QStringLiteral("[RW] Read  FAIL row=%1 (decode error)")
                                      .arg(m_pendingRow);
            m_registerTable->markRowError(m_pendingRow);
        }
        processNextInQueue();
    } else if (cmd == MotorProtocol::CmdWriteRegister && m_isWriteOp) {
        m_timeoutTimer->stop();
        qDebug().noquote() << QStringLiteral("[RW] Write ACK  row=%1")
                                  .arg(m_pendingRow);
        m_registerTable->markWriteButtonFeedback(m_pendingRow, true);
        processNextInQueue();
    } else if (cmd == MotorProtocol::CmdErrorResponse) {
        m_timeoutTimer->stop();
        const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
        qDebug().noquote() << QStringLiteral("[RW] Error row=%1 code=0x%2")
                                  .arg(m_pendingRow)
                                  .arg(errorCode, 2, 16, QLatin1Char('0'))
                                  .toUpper();
        if (m_isWriteOp) {
            m_registerTable->markWriteButtonFeedback(m_pendingRow, false);
        } else {
            m_registerTable->markRowError(m_pendingRow);
        }
        processNextInQueue();
    }
}

void RegisterRwTab::onSerialError(const QString &message) {
    if (m_timeoutTimer != nullptr) {
        m_timeoutTimer->stop();
    }
    if (m_pendingRow >= 0) {
        if (m_isWriteOp) {
            m_registerTable->markWriteButtonFeedback(m_pendingRow, false);
        } else {
            m_registerTable->markRowError(m_pendingRow);
        }
    }
    m_pendingRow = -1;
    m_pendingQueue.clear();
    Q_UNUSED(message);
}

void RegisterRwTab::onConfigChanged() {
    m_registerTable->saveConfig(configFilePath());
}

void RegisterRwTab::processNextInQueue() {
    if (m_serialManager == nullptr) {
        m_pendingRow = -1;
        m_pendingQueue.clear();
        return;
    }

    if (m_pendingQueue.isEmpty()) {
        if (m_timeoutTimer != nullptr) {
            m_timeoutTimer->stop();
        }
        qDebug().noquote() << QStringLiteral("[RW] Queue done (%1)")
                                  .arg(m_isWriteOp ? QStringLiteral("write") : QStringLiteral("read"));
        m_pendingRow = -1;
        if (!m_isWriteOp) {
            m_registerTable->saveConfig(configFilePath());
        }
        return;
    }

    m_pendingRow = m_pendingQueue.dequeue();
    const quint16 addr = m_registerTable->rowAddress(m_pendingRow);
    if (m_isWriteOp) {
        const qint16 value = m_registerTable->rowValue(m_pendingRow);
        QMetaObject::invokeMethod(
            m_serialManager,
            "sendCommand",
            Qt::QueuedConnection,
            Q_ARG(uint8_t, MotorProtocol::CmdWriteRegister),
            Q_ARG(QByteArray, MotorProtocol::encodeWriteRegister(addr, value)));
        m_timeoutTimer->start(500);
    } else {
        m_registerTable->markRowPending(m_pendingRow);
        QMetaObject::invokeMethod(
            m_serialManager,
            "sendCommand",
            Qt::QueuedConnection,
            Q_ARG(uint8_t, MotorProtocol::CmdReadRegister),
            Q_ARG(QByteArray, MotorProtocol::encodeReadRegister(addr)));
        m_timeoutTimer->start(500);
    }
}

void RegisterRwTab::onTimeout() {
    if (m_pendingRow < 0) {
        return;
    }

    if (m_isWriteOp) {
        m_registerTable->markWriteButtonFeedback(m_pendingRow, false);
    } else {
        m_registerTable->markRowError(m_pendingRow);
    }

    processNextInQueue();
}

QString RegisterRwTab::configFilePath() const {
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dirPath);
    return dirPath + QStringLiteral("/registers.json");
}
