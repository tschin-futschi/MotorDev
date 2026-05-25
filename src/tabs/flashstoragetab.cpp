// =============================================================================
// @file    flashstoragetab.cpp
// @brief   FLASH 文件存储 Tab 实现
// =============================================================================
#include "tabs/flashstoragetab.h"

#include "services/flashstoreservice.h"
#include "ui/style_constants.h"
#include "widgets/fwflashlogpanel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

using namespace MotorDev;

FlashStorageTab::FlashStorageTab(SerialManager *serialManager, QWidget *parent)
    : QWidget(parent) {
    setupUi();
    m_service = new FlashStoreService(serialManager, this);
    connectSignals();
    updateButtonsEnabled();
    updateCapacityLabel(0, 0);  // 显示"未知"直到首次 INFO 响应
    m_stageLabel->setText(tr("空闲"));
}

FlashStorageTab::~FlashStorageTab() = default;

// -----------------------------------------------------------------------------
// UI 构建
// -----------------------------------------------------------------------------

void FlashStorageTab::setupUi() {
    setObjectName(QStringLiteral("FlashStorageTab"));

    auto *root = new QVBoxLayout(this);
    root->setSpacing(Style::Size::ContentSpacing);
    root->setContentsMargins(Style::Size::ContentPadding,
                              Style::Size::ContentPadding,
                              Style::Size::ContentPadding,
                              Style::Size::ContentPadding);

    // -------- Row 1: 容量行 --------
    auto *capRow = new QHBoxLayout();
    capRow->setSpacing(8);
    capRow->setContentsMargins(0, 0, 0, 0);

    m_capacityLabel = new QLabel(this);
    m_capacityLabel->setObjectName(QStringLiteral("flashStoreCapacityLabel"));
    QPalette capPal = m_capacityLabel->palette();
    capPal.setColor(QPalette::WindowText, Style::Color::FwFlashStageLabelFg);
    m_capacityLabel->setPalette(capPal);
    capRow->addWidget(m_capacityLabel, 1);

    m_refreshBtn = new QPushButton(tr("刷新容量"), this);
    m_refreshBtn->setObjectName(QStringLiteral("flashStoreRefreshBtn"));
    capRow->addWidget(m_refreshBtn);
    root->addLayout(capRow);

    // -------- Row 2: 操作行 --------
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->setContentsMargins(0, 0, 0, 0);

    m_uploadBtn = new QPushButton(tr("上传文件..."), this);
    m_uploadBtn->setObjectName(QStringLiteral("flashStoreUploadBtn"));
    m_uploadBtn->setMinimumSize(QSize(Style::Size::FwFlashStartButtonW,
                                       Style::Size::FwFlashStartButtonH));
    btnRow->addWidget(m_uploadBtn);

    m_downloadBtn = new QPushButton(tr("下载到本地..."), this);
    m_downloadBtn->setObjectName(QStringLiteral("flashStoreDownloadBtn"));
    m_downloadBtn->setMinimumSize(QSize(Style::Size::FwFlashStartButtonW,
                                         Style::Size::FwFlashStartButtonH));
    btnRow->addWidget(m_downloadBtn);

    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setObjectName(QStringLiteral("flashStoreCancelBtn"));
    m_cancelBtn->setMinimumSize(QSize(Style::Size::FwFlashCancelButtonW,
                                       Style::Size::FwFlashCancelButtonH));
    btnRow->addWidget(m_cancelBtn);

    m_wipeBtn = new QPushButton(tr("清空 FLASH"), this);
    m_wipeBtn->setObjectName(QStringLiteral("flashStoreWipeBtn"));
    m_wipeBtn->setMinimumSize(QSize(Style::Size::FwFlashStartButtonW,
                                     Style::Size::FwFlashStartButtonH));
    btnRow->addWidget(m_wipeBtn);

    m_stageLabel = new QLabel(this);
    m_stageLabel->setObjectName(QStringLiteral("flashStoreStageLabel"));
    QPalette stagePal = m_stageLabel->palette();
    stagePal.setColor(QPalette::WindowText, Style::Color::FwFlashStageLabelFg);
    m_stageLabel->setPalette(stagePal);
    m_stageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    btnRow->addWidget(m_stageLabel, 1);
    root->addLayout(btnRow);

    // -------- Row 3: 进度条（复刻 FwFlashControlPanel 风格）--------
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("flashStoreProgress"));
    m_progress->setMinimumHeight(Style::Size::FwFlashProgressH);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setAlignment(Qt::AlignCenter);
    m_progress->setStyleSheet(QStringLiteral(
        "QProgressBar#flashStoreProgress {"
        "  border: 1px solid %1;"
        "  border-radius: 5px;"
        "  background: %2;"
        "  text-align: center;"
        "  color: %3;"
        "}"
        "QProgressBar#flashStoreProgress::chunk {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                                     stop:0 %4, stop:1 %5);"
        "  border-radius: 4px;"
        "  margin: 1px;"
        "}")
        .arg(Style::Color::DefaultBorder.name())
        .arg(Style::Color::WindowBackground.name())
        .arg(Style::Color::AppText.name())
        .arg(Style::Color::FwFlashProgressChunkStart.name())
        .arg(Style::Color::FwFlashProgressChunkEnd.name()));
    root->addWidget(m_progress);

    // -------- Row 4: 日志面板（复用 FwFlashLogPanel）--------
    m_logPanel = new FwFlashLogPanel(this);
    root->addWidget(m_logPanel, 1);
}

// -----------------------------------------------------------------------------
// 信号槽
// -----------------------------------------------------------------------------

void FlashStorageTab::connectSignals() {
    connect(m_refreshBtn,  &QPushButton::clicked, this, &FlashStorageTab::onRefreshClicked);
    connect(m_uploadBtn,   &QPushButton::clicked, this, &FlashStorageTab::onUploadClicked);
    connect(m_downloadBtn, &QPushButton::clicked, this, &FlashStorageTab::onDownloadClicked);
    connect(m_cancelBtn,   &QPushButton::clicked, this, &FlashStorageTab::onCancelClicked);
    connect(m_wipeBtn,     &QPushButton::clicked, this, &FlashStorageTab::onWipeClicked);

    connect(m_service, &FlashStoreService::stateChanged,    this, &FlashStorageTab::onServiceStateChanged);
    connect(m_service, &FlashStoreService::stageMessage,    this, &FlashStorageTab::onServiceStage);
    connect(m_service, &FlashStoreService::progressUpdated, this, &FlashStorageTab::onServiceProgress);
    connect(m_service, &FlashStoreService::logMessage,      this, &FlashStorageTab::onServiceLog);
    connect(m_service, &FlashStoreService::finished,        this, &FlashStorageTab::onServiceFinished);
    connect(m_service, &FlashStoreService::infoUpdated,     this, &FlashStorageTab::onServiceInfoUpdated);
    connect(m_service, &FlashStoreService::readCompleted,   this, &FlashStorageTab::onServiceReadCompleted);
}

// -----------------------------------------------------------------------------
// UI 状态
// -----------------------------------------------------------------------------

void FlashStorageTab::updateButtonsEnabled() {
    const bool busy = m_service != nullptr && m_service->isBusy();
    m_uploadBtn->setEnabled(!busy);
    m_downloadBtn->setEnabled(!busy);
    m_refreshBtn->setEnabled(!busy);
    m_wipeBtn->setEnabled(!busy);
    m_cancelBtn->setEnabled(busy);
}

QString FlashStorageTab::humanBytes(quint64 v) {
    if (v < 1024ULL) {
        return QStringLiteral("%1 B").arg(v);
    } else if (v < 1024ULL * 1024ULL) {
        return QStringLiteral("%1 KB").arg(QString::number(v / 1024.0, 'f', 2));
    } else {
        return QStringLiteral("%1 MB").arg(QString::number(v / (1024.0 * 1024.0), 'f', 2));
    }
}

void FlashStorageTab::updateCapacityLabel(quint32 totalCapacity, quint32 usedSize) {
    if (totalCapacity == 0) {
        m_capacityLabel->setText(tr("容量：未知（点击刷新查询）"));
        return;
    }
    const quint32 freeSize = (usedSize <= totalCapacity) ? (totalCapacity - usedSize) : 0U;
    m_capacityLabel->setText(tr("容量：已用 %1 / 总 %2 | 剩余 %3")
                                 .arg(humanBytes(usedSize),
                                      humanBytes(totalCapacity),
                                      humanBytes(freeSize)));
}

// -----------------------------------------------------------------------------
// 按钮槽
// -----------------------------------------------------------------------------

void FlashStorageTab::onUploadClicked() {
    const QString startDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("选择要上传的文件"), startDir, tr("All Files (*.*)"));
    if (path.isEmpty()) return;

    const QFileInfo fi(path);
    constexpr qint64 kMax = 917488;
    if (fi.size() <= 0) {
        m_logPanel->appendError(tr("文件为空"));
        return;
    }
    if (fi.size() > kMax) {
        m_logPanel->appendError(
            tr("文件 %1 字节超出 STM32 Flash 存储区上限（%2 字节，约 896 KB）")
                .arg(fi.size()).arg(kMax));
        return;
    }
    m_service->startWrite(path);
}

void FlashStorageTab::onDownloadClicked() {
    const QString startDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择下载目录"), startDir);
    if (dir.isEmpty()) return;
    m_service->startRead(dir);
}

void FlashStorageTab::onCancelClicked() {
    m_service->cancel();
}

void FlashStorageTab::onRefreshClicked() {
    m_service->refreshInfo();
}

void FlashStorageTab::onWipeClicked() {
    // 危险操作二次确认。默认按钮 = No，防 Enter 键误触发。
    const QMessageBox::StandardButton ret = QMessageBox::warning(
        this,
        tr("确认清空"),
        tr("此操作将清空 STM32 上的 Flash 文件存储区（896 KB），\n"
           "且**不可恢复**。\n\n"
           "确定继续吗？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    m_service->startWipe();
}

// -----------------------------------------------------------------------------
// Service 信号槽
// -----------------------------------------------------------------------------

void FlashStorageTab::onServiceStateChanged(FlashStoreService::State /*newState*/) {
    updateButtonsEnabled();
}

void FlashStorageTab::onServiceStage(const QString &message) {
    m_stageLabel->setText(message);
}

void FlashStorageTab::onServiceProgress(qint64 done, qint64 total) {
    if (total <= 0) {
        m_progress->setValue(0);
        return;
    }
    const int pct = static_cast<int>(qBound<qint64>(qint64{0}, done * 100 / total, qint64{100}));
    m_progress->setValue(pct);
}

void FlashStorageTab::onServiceLog(FlashStoreService::LogLevel level, const QString &message) {
    switch (level) {
    case FlashStoreService::LogLevel::Info:  m_logPanel->appendInfo(message); break;
    case FlashStoreService::LogLevel::Warn:  m_logPanel->appendWarn(message); break;
    case FlashStoreService::LogLevel::Error: m_logPanel->appendError(message); break;
    case FlashStoreService::LogLevel::Ok:    m_logPanel->appendOk(message); break;
    }
}

void FlashStorageTab::onServiceFinished(bool /*success*/, const QString & /*summary*/) {
    updateButtonsEnabled();
}

void FlashStorageTab::onServiceInfoUpdated(quint32 totalCapacity, quint32 usedSize) {
    updateCapacityLabel(totalCapacity, usedSize);
}

void FlashStorageTab::onServiceReadCompleted(const QString &savedFilePath) {
    m_logPanel->appendOk(tr("已保存到：%1（改回原扩展名即可使用）").arg(savedFilePath));
}
