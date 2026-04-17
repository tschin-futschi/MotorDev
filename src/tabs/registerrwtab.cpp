#include "tabs/registerrwtab.h"

#include "protocol/motor_protocol.h"
#include "serialmanager.h"
#include "ui_registerrwtab.h"
#include "ui/style_constants.h"
#include "widgets/registertable.h"
#include "widgets/sidebar.h"

#include <QButtonGroup>
#include <QDir>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QDebug>
#include <QMetaObject>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <utility>

using namespace MotorDev;

RegisterRwTab::RegisterRwTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::RegisterRwTab>())
    , m_serialManager(serialManager) {
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    ui->setupUi(this);

    auto *topLayout = qobject_cast<QHBoxLayout *>(layout());
    if (topLayout != nullptr) {
        topLayout->removeWidget(ui->sidebarContent);
        auto *sidebar = new Sidebar(tr("读写"), ui->sidebarContent, this);
        topLayout->insertWidget(0, sidebar);
    }

    ui->mainSplitter->setStretchFactor(0, 5);
    ui->mainSplitter->setStretchFactor(1, 3);

    m_registerTable = ui->registerTable;
    m_readAllButton = ui->readAllButton;
    m_writeAllButton = ui->writeAllButton;
    m_decButton = ui->decButton;
    m_hexButton = ui->hexButton;
    m_batchBtn[0] = ui->batchBtn0; m_batchBtn[1] = ui->batchBtn1; m_batchBtn[2] = ui->batchBtn2; m_batchBtn[3] = ui->batchBtn3;
    m_batchDescEdit[0] = ui->batchDescEdit0; m_batchDescEdit[1] = ui->batchDescEdit1; m_batchDescEdit[2] = ui->batchDescEdit2; m_batchDescEdit[3] = ui->batchDescEdit3;
    m_batchPathEdit[0] = ui->batchPathEdit0; m_batchPathEdit[1] = ui->batchPathEdit1; m_batchPathEdit[2] = ui->batchPathEdit2; m_batchPathEdit[3] = ui->batchPathEdit3;
    m_batchBrowseBtn[0] = ui->batchBrowseBtn0; m_batchBrowseBtn[1] = ui->batchBrowseBtn1; m_batchBrowseBtn[2] = ui->batchBrowseBtn2; m_batchBrowseBtn[3] = ui->batchBrowseBtn3;

    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_decButton);
    modeGroup->addButton(m_hexButton);

    connectSignals();
    m_registerTable->loadConfig(configFilePath());
}

RegisterRwTab::~RegisterRwTab() = default;

void RegisterRwTab::connectSignals() {
    connect(m_registerTable, &RegisterTable::readRowRequested, this, &RegisterRwTab::onReadRowRequested);
    connect(m_registerTable, &RegisterTable::writeRowRequested, this, &RegisterRwTab::onWriteRowRequested);
    connect(m_registerTable, &RegisterTable::configChanged, this, &RegisterRwTab::onConfigChanged);
    connect(m_readAllButton, &QPushButton::clicked, this, &RegisterRwTab::onReadAllClicked);
    connect(m_writeAllButton, &QPushButton::clicked, this, &RegisterRwTab::onWriteAllClicked);
    connect(m_hexButton, &QPushButton::toggled, this, [this](bool checked) { if (checked) m_registerTable->setValueMode(RegisterTable::ValueMode::Hex); });
    connect(m_decButton, &QPushButton::toggled, this, [this](bool checked) { if (checked) m_registerTable->setValueMode(RegisterTable::ValueMode::Dec); });
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
        if (m_timeoutTimer != nullptr) m_timeoutTimer->stop();
        m_pendingRow = -1;
        m_pendingQueue.clear();
    }
}

void RegisterRwTab::onReadRowRequested(int globalRow, quint16 addr) {
    if (m_serialManager == nullptr) return;
    qDebug().noquote() << QStringLiteral("[RW] Read  row=%1 addr=0x%2").arg(globalRow).arg(addr, 4, 16, QLatin1Char('0')).toUpper();
    m_pendingQueue.clear(); m_pendingRow = globalRow; m_isWriteOp = false; m_registerTable->markRowPending(globalRow);
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection, Q_ARG(uint8_t, MotorProtocol::CmdReadRegister), Q_ARG(QByteArray, MotorProtocol::encodeReadRegister(addr)));
    m_timeoutTimer->start(500);
}

void RegisterRwTab::onWriteRowRequested(int globalRow, quint16 addr, qint16 value) {
    if (m_serialManager == nullptr) return;
    qDebug().noquote() << QStringLiteral("[RW] Write row=%1 addr=0x%2 val=0x%3").arg(globalRow).arg(addr, 4, 16, QLatin1Char('0')).arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0')).toUpper();
    m_pendingQueue.clear(); m_pendingRow = globalRow; m_isWriteOp = true;
    QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection, Q_ARG(uint8_t, MotorProtocol::CmdWriteRegister), Q_ARG(QByteArray, MotorProtocol::encodeWriteRegister(addr, value)));
    m_timeoutTimer->start(500);
}

void RegisterRwTab::onReadAllClicked() {
    m_pendingQueue.clear(); m_isWriteOp = false;
    for (int row = 0; row < Style::Size::TableGroupCount * Style::Size::TableRowCount; ++row) if (m_registerTable->rowHasAddress(row)) m_pendingQueue.enqueue(row);
    processNextInQueue();
}

void RegisterRwTab::onWriteAllClicked() {
    m_pendingQueue.clear(); m_isWriteOp = true;
    for (int row = 0; row < Style::Size::TableGroupCount * Style::Size::TableRowCount; ++row) if (m_registerTable->rowHasAddress(row) && m_registerTable->rowHasValue(row)) m_pendingQueue.enqueue(row);
    processNextInQueue();
}

void RegisterRwTab::onFrameReceived(uint8_t cmd, uint8_t seq, const QByteArray &data) {
    Q_UNUSED(seq);
    if (m_pendingRow < 0) return;
    if (cmd == MotorProtocol::CmdReadRegister && !m_isWriteOp) {
        m_timeoutTimer->stop();
        qint16 value = 0;
        if (MotorProtocol::decodeReadRegisterResponse(data, &value)) { qDebug().noquote() << QStringLiteral("[RW] Read  OK   row=%1 val=0x%2").arg(m_pendingRow).arg(static_cast<quint16>(value), 4, 16, QLatin1Char('0')).toUpper(); m_registerTable->updateRowValue(m_pendingRow, value); }
        else { qDebug().noquote() << QStringLiteral("[RW] Read  FAIL row=%1 (decode error)").arg(m_pendingRow); m_registerTable->markRowError(m_pendingRow); }
        processNextInQueue();
    } else if (cmd == MotorProtocol::CmdWriteRegister && m_isWriteOp) {
        m_timeoutTimer->stop(); qDebug().noquote() << QStringLiteral("[RW] Write ACK  row=%1").arg(m_pendingRow); m_registerTable->markWriteButtonFeedback(m_pendingRow, true); processNextInQueue();
    } else if (cmd == MotorProtocol::CmdErrorResponse) {
        m_timeoutTimer->stop();
        const uint8_t errorCode = MotorProtocol::decodeErrorCode(data);
        qDebug().noquote() << QStringLiteral("[RW] Error row=%1 code=0x%2").arg(m_pendingRow).arg(errorCode, 2, 16, QLatin1Char('0')).toUpper();
        if (m_isWriteOp) m_registerTable->markWriteButtonFeedback(m_pendingRow, false);
        else m_registerTable->markRowError(m_pendingRow);
        processNextInQueue();
    }
}

void RegisterRwTab::onSerialError(const QString &message) {
    if (m_timeoutTimer != nullptr) m_timeoutTimer->stop();
    if (m_pendingRow >= 0) {
        if (m_isWriteOp) m_registerTable->markWriteButtonFeedback(m_pendingRow, false);
        else m_registerTable->markRowError(m_pendingRow);
    }
    m_pendingRow = -1;
    m_pendingQueue.clear();
    Q_UNUSED(message);
}

void RegisterRwTab::onConfigChanged() { m_registerTable->saveConfig(configFilePath()); }

void RegisterRwTab::processNextInQueue() {
    if (m_serialManager == nullptr) { m_pendingRow = -1; m_pendingQueue.clear(); return; }
    if (m_pendingQueue.isEmpty()) {
        if (m_timeoutTimer != nullptr) m_timeoutTimer->stop();
        qDebug().noquote() << QStringLiteral("[RW] Queue done (%1)").arg(m_isWriteOp ? QStringLiteral("write") : QStringLiteral("read"));
        m_pendingRow = -1;
        if (!m_isWriteOp) m_registerTable->saveConfig(configFilePath());
        return;
    }
    m_pendingRow = m_pendingQueue.dequeue();
    const quint16 addr = m_registerTable->rowAddress(m_pendingRow);
    if (m_isWriteOp) {
        const qint16 value = m_registerTable->rowValue(m_pendingRow);
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection, Q_ARG(uint8_t, MotorProtocol::CmdWriteRegister), Q_ARG(QByteArray, MotorProtocol::encodeWriteRegister(addr, value)));
    } else {
        m_registerTable->markRowPending(m_pendingRow);
        QMetaObject::invokeMethod(m_serialManager, "sendCommand", Qt::QueuedConnection, Q_ARG(uint8_t, MotorProtocol::CmdReadRegister), Q_ARG(QByteArray, MotorProtocol::encodeReadRegister(addr)));
    }
    m_timeoutTimer->start(500);
}

void RegisterRwTab::onTimeout() {
    if (m_pendingRow < 0) return;
    if (m_isWriteOp) m_registerTable->markWriteButtonFeedback(m_pendingRow, false);
    else m_registerTable->markRowError(m_pendingRow);
    processNextInQueue();
}

QString RegisterRwTab::configFilePath() const {
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dirPath);
    return dirPath + QStringLiteral("/registers.json");
}
