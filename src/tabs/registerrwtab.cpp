// =============================================================================
// @file    registerrwtab.cpp
// @brief   寄存器读写页面实现 — UI 构建、信号槽连接、配置持久化
// =============================================================================
#include "tabs/registerrwtab.h"

#include "services/batchregisterservice.h"
#include "services/commanddispatcher.h"
#include "services/registerservice.h"
#include "ui/style_constants.h"
#include "widgets/registertable.h"
#include "widgets/sidebar.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QSpacerItem>
#include <QToolButton>
#include <QVBoxLayout>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

RegisterRwTab::RegisterRwTab(CommandDispatcher *dispatcher, QWidget *parent)
    : QWidget(parent) {
    m_service = new RegisterService(dispatcher, this);
    m_batchService = new BatchRegisterService(dispatcher, this);
    setupUi();
    connectSignals();

    // 浏览对话框默认目录：Documents
    m_batchLastBrowseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    // 从持久化文件加载上次的寄存器配置
    m_registerTable->loadConfig(configFilePath());
}

RegisterRwTab::~RegisterRwTab() {
    // 批量读写浮窗为 Qt::Tool 顶级窗口（parent=nullptr），手动 delete
    delete m_batchPanel;
    m_batchPanel = nullptr;
}

// =============================================================================
// 事件过滤：浮窗 Close 时同步 toggle 按钮状态
// =============================================================================

bool RegisterRwTab::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_batchPanel && event->type() == QEvent::Close) {
        // 用户点击窗口关闭按钮 → 取消 toggle，同步 UI 状态
        // 注：setChecked(false) 会触发 toggled 信号，lambda 内 hide() 但浮窗已被 close 路径隐藏，无副作用
        if (m_batchToggleBtn != nullptr && m_batchToggleBtn->isChecked()) {
            m_batchToggleBtn->setChecked(false);
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// 信号槽连接
// =============================================================================

void RegisterRwTab::connectSignals() {
    // -------------------------------------------------------------------------
    // 表格行操作 → Service
    // -------------------------------------------------------------------------

    // 单行读取请求
    connect(m_registerTable, &RegisterTable::readRowRequested, this, [this](int globalRow, quint16 addr) {
        m_service->readSingleRow(globalRow, addr);
    });

    // 单行写入请求
    connect(m_registerTable, &RegisterTable::writeRowRequested, this, [this](int globalRow, quint16 addr, qint16 value) {
        m_service->writeSingleRow(globalRow, addr, value);
    });

    // 表格配置变化 → 自动保存
    connect(m_registerTable, &RegisterTable::configChanged, this, [this]() {
        m_registerTable->saveConfig(configFilePath());
    });

    // -------------------------------------------------------------------------
    // 全量读取/写入（Sidebar）— 与批量读写浮窗全局互斥（决策 #8）
    // -------------------------------------------------------------------------

    // 全部读取：收集所有有地址的行，发起批量读取
    // 上限用 RegisterTable::totalRows()，覆盖动态扩展行
    connect(m_readAllButton, &QPushButton::clicked, this, [this]() {
        if (isBusy()) return;
        setBusyOwner(BusyOwner::SidebarAll);
        QVector<RegisterService::RowRequest> rows;
        const int total = m_registerTable->totalRows();
        for (int row = 0; row < total; ++row) {
            if (m_registerTable->rowHasAddress(row)) {
                RegisterService::RowRequest req;
                req.globalRow = row;
                req.addr = m_registerTable->rowAddress(row);
                rows.append(req);
            }
        }
        m_service->readBatch(rows);
    });

    // 全部写入：收集所有有地址且有值的行，发起批量写入
    connect(m_writeAllButton, &QPushButton::clicked, this, [this]() {
        if (isBusy()) return;
        setBusyOwner(BusyOwner::SidebarAll);
        QVector<RegisterService::RowRequest> rows;
        const int total = m_registerTable->totalRows();
        for (int row = 0; row < total; ++row) {
            if (m_registerTable->rowHasAddress(row) && m_registerTable->rowHasValue(row)) {
                RegisterService::RowRequest req;
                req.globalRow = row;
                req.addr = m_registerTable->rowAddress(row);
                req.value = m_registerTable->rowValue(row);
                rows.append(req);
            }
        }
        m_service->writeBatch(rows);
    });

    // -------------------------------------------------------------------------
    // Hex / Dec 模式切换
    // -------------------------------------------------------------------------
    connect(m_hexButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) m_registerTable->setValueMode(RegisterTable::ValueMode::Hex);
    });
    connect(m_decButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) m_registerTable->setValueMode(RegisterTable::ValueMode::Dec);
    });

    // -------------------------------------------------------------------------
    // 批量读写浮窗 toggle（参考示波器 ShowRegister 模式：独立 Qt::Tool 浮动窗口）
    // 按钮文字始终为「批量读写」，按下/弹起状态由 QToolButton checked 视觉表达。
    // -------------------------------------------------------------------------
    connect(m_batchToggleBtn, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_batchPanel->show();
            m_batchPanel->raise();
            m_batchPanel->activateWindow();
        } else {
            m_batchPanel->hide();
        }
    });

    // -------------------------------------------------------------------------
    // Service → UI：操作结果回调
    // -------------------------------------------------------------------------

    connect(m_service, &RegisterService::rowPending, m_registerTable, &RegisterTable::markRowPending);
    connect(m_service, &RegisterService::rowReadOk, m_registerTable, &RegisterTable::updateRowValue);
    connect(m_service, &RegisterService::rowReadError, m_registerTable, &RegisterTable::markRowError);

    // 写入成功/失败 → 表格按钮闪烁反馈
    connect(m_service, &RegisterService::rowWriteOk, this, [this](int globalRow) {
        m_registerTable->markWriteButtonFeedback(globalRow, true);
    });
    connect(m_service, &RegisterService::rowWriteError, this, [this](int globalRow) {
        m_registerTable->markWriteButtonFeedback(globalRow, false);
    });

    // Sidebar 全部读/写 完成 → 释放互斥；自动保存读取结果
    // 注：m_service 的 queueFinished 也会在单行读写时触发，但此时 m_busyOwner != SidebarAll
    // 不进释放分支，无副作用。
    connect(m_service, &RegisterService::queueFinished, this, [this](bool wasWrite) {
        if (m_busyOwner == BusyOwner::SidebarAll) {
            setBusyOwner(BusyOwner::None);
        }
        if (!wasWrite) m_registerTable->saveConfig(configFilePath());
    });

    // -------------------------------------------------------------------------
    // 批量读写浮窗：浏览 + 操作按钮 → BatchRegisterService
    // -------------------------------------------------------------------------

    for (int i = 0; i < 4; ++i) {
        connect(m_batchBrowseBtn[i], &QPushButton::clicked, this, [this, i]() {
            onBatchBrowseClicked(i);
        });
        connect(m_batchBtn[i], &QPushButton::clicked, this, [this, i]() {
            onBatchActionClicked(i);
        });
    }

    // BatchRegisterService 信号 → UI 渲染层
    connect(m_batchService, &BatchRegisterService::stageMessage,
            this, &RegisterRwTab::onBatchServiceStageMessage);
    connect(m_batchService, &BatchRegisterService::logMessage,
            this, &RegisterRwTab::onBatchServiceLog);
    connect(m_batchService, &BatchRegisterService::finished,
            this, &RegisterRwTab::onBatchServiceFinished);
}

// =============================================================================
// 外部状态接口
// =============================================================================

/// @brief 串口连接状态变化时，联动启用/禁用操作按钮
void RegisterRwTab::setConnected(bool connected) {
    m_readAllButton->setEnabled(connected);
    m_writeAllButton->setEnabled(connected);
    m_registerTable->setConnected(connected);
}

/// @brief 获取寄存器配置的持久化文件路径（AppData/registers.json）
QString RegisterRwTab::configFilePath() const {
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dirPath);
    return dirPath + QStringLiteral("/registers.json");
}

// =============================================================================
// UI 构建
// =============================================================================

void RegisterRwTab::setupUi() {
    setObjectName(QStringLiteral("RegisterRwTab"));

    // 顶层水平布局：侧边栏 | 主内容区
    auto *topLayout = new QHBoxLayout(this);
    topLayout->setObjectName(QStringLiteral("topLayout"));
    topLayout->setSpacing(0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    // --- 侧边栏 ---
    m_sidebarContent = new QWidget(this);
    m_sidebarContent->setObjectName(QStringLiteral("sidebarContent"));
    auto *sidebarLayout = new QVBoxLayout(m_sidebarContent);
    sidebarLayout->setObjectName(QStringLiteral("sidebarLayout"));
    sidebarLayout->setSpacing(8);
    sidebarLayout->setContentsMargins(10, 8, 10, 8);

    m_readAllButton = new QPushButton(m_sidebarContent);
    m_readAllButton->setObjectName(QStringLiteral("readAllButton"));
    m_readAllButton->setMinimumSize(QSize(0, 32));
    m_readAllButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_readAllButton->setProperty("buttonRole", QStringLiteral("primary-sidebar"));
    m_readAllButton->setText(tr("全部读取"));
    sidebarLayout->addWidget(m_readAllButton);

    m_writeAllButton = new QPushButton(m_sidebarContent);
    m_writeAllButton->setObjectName(QStringLiteral("writeAllButton"));
    m_writeAllButton->setMinimumSize(QSize(0, 32));
    m_writeAllButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, 32));
    m_writeAllButton->setProperty("buttonRole", QStringLiteral("write-sidebar"));
    m_writeAllButton->setText(tr("全部写入"));
    sidebarLayout->addWidget(m_writeAllButton);

    sidebarLayout->addItem(new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    // Hex/Dec 模式切换
    auto *valueModeLabel = new QLabel(m_sidebarContent);
    valueModeLabel->setObjectName(QStringLiteral("valueModeLabel"));
    valueModeLabel->setText(tr("数值表示"));
    sidebarLayout->addWidget(valueModeLabel);

    auto *modeRow = new QHBoxLayout();
    modeRow->setObjectName(QStringLiteral("modeRow"));
    modeRow->setSpacing(10);
    modeRow->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->addLayout(modeRow);

    m_decButton = new QPushButton(m_sidebarContent);
    m_decButton->setObjectName(QStringLiteral("decButton"));
    m_decButton->setCheckable(true);
    m_decButton->setProperty("buttonRole", QStringLiteral("mode"));
    m_decButton->setText(tr("DEC"));
    modeRow->addWidget(m_decButton);

    m_hexButton = new QPushButton(m_sidebarContent);
    m_hexButton->setObjectName(QStringLiteral("hexButton"));
    m_hexButton->setCheckable(true);
    m_hexButton->setChecked(true);
    m_hexButton->setProperty("buttonRole", QStringLiteral("mode"));
    m_hexButton->setText(tr("HEX"));
    modeRow->addWidget(m_hexButton);

    sidebarLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    auto *sidebar = new Sidebar(tr("读写"), m_sidebarContent, this);
    topLayout->addWidget(sidebar);

    // --- 主内容区 ---
    m_mainContent = new QWidget(this);
    m_mainContent->setObjectName(QStringLiteral("mainContent"));
    auto *mainLayout = new QVBoxLayout(m_mainContent);
    mainLayout->setObjectName(QStringLiteral("mainLayout"));
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    topLayout->addWidget(m_mainContent);

    // 上方：寄存器表格（stretch=1，折叠批量面板时自动占满剩余空间）
    m_registerTable = new RegisterTable(m_mainContent);
    m_registerTable->setObjectName(QStringLiteral("registerTable"));
    mainLayout->addWidget(m_registerTable, 1);

    // 下方：常驻工具条（右对齐 toggle 按钮，与示波器 ShowRegister 同款机制：
    //                  点按钮弹出独立 Qt::Tool 浮窗，再点或关闭浮窗收回）
    auto *toolbarRow = new QHBoxLayout();
    toolbarRow->setObjectName(QStringLiteral("registerRwBatchToolbar"));
    toolbarRow->setSpacing(0);
    toolbarRow->setContentsMargins(0, 0, 0, 0);
    toolbarRow->addStretch(1);

    m_batchToggleBtn = new QToolButton(m_mainContent);
    m_batchToggleBtn->setObjectName(QStringLiteral("registerRwBatchToggleBtn"));
    m_batchToggleBtn->setCheckable(true);
    m_batchToggleBtn->setChecked(false);
    m_batchToggleBtn->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    m_batchToggleBtn->setText(tr("批量读写"));
    toolbarRow->addWidget(m_batchToggleBtn);
    mainLayout->addLayout(toolbarRow);

    // 批量读写浮窗（Qt::Tool 顶级窗口，parent=nullptr，析构手动 delete）
    m_batchPanel = new QWidget(nullptr, Qt::Tool);
    m_batchPanel->setObjectName(QStringLiteral("registerRwBatchPanel"));
    m_batchPanel->setWindowTitle(tr("批量读写"));
    m_batchPanel->setAttribute(Qt::WA_DeleteOnClose, false);
    m_batchPanel->installEventFilter(this);
    m_batchPanel->resize(600, 220);
    m_batchPanel->hide();

    auto *batchLayout = new QVBoxLayout(m_batchPanel);
    batchLayout->setObjectName(QStringLiteral("batchLayout"));
    batchLayout->setSpacing(10);
    batchLayout->setContentsMargins(Style::Size::ContentSpacing,
                                    Style::Size::ContentSpacing,
                                    Style::Size::ContentSpacing,
                                    Style::Size::ContentSpacing);

    // 4 组批量操作行（每行 3 列：[操作按钮] [文件路径 只读] [浏览按钮]）
    // 前 2 行为"批量写入"，后 2 行为"批量读出"
    const struct {
        const char *buttonName;
        const char *pathName;
        const char *browseName;
        const char *buttonText;
    } rowSpecs[4] = {
        {"batchBtn0", "batchPathEdit0", "batchBrowseBtn0", "批量写入"},
        {"batchBtn1", "batchPathEdit1", "batchBrowseBtn1", "批量写入"},
        {"batchBtn2", "batchPathEdit2", "batchBrowseBtn2", "批量读出"},
        {"batchBtn3", "batchPathEdit3", "batchBrowseBtn3", "批量读出"},
    };

    for (int i = 0; i < 4; ++i) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(QStringLiteral("batchRow%1").arg(i));
        rowLayout->setSpacing(10);
        batchLayout->addLayout(rowLayout);

        m_batchBtn[i] = new QPushButton(m_batchPanel);
        m_batchBtn[i]->setObjectName(QString::fromLatin1(rowSpecs[i].buttonName));
        m_batchBtn[i]->setMinimumSize(QSize(96, 32));
        m_batchBtn[i]->setMaximumSize(QSize(96, 32));
        m_batchBtn[i]->setProperty("buttonRole", QStringLiteral("primary-sidebar"));
        m_batchBtn[i]->setText(tr(rowSpecs[i].buttonText));
        rowLayout->addWidget(m_batchBtn[i]);

        m_batchPathEdit[i] = new QLineEdit(m_batchPanel);
        m_batchPathEdit[i]->setObjectName(QString::fromLatin1(rowSpecs[i].pathName));
        m_batchPathEdit[i]->setReadOnly(true);
        m_batchPathEdit[i]->setProperty("inputRole", QStringLiteral("form"));
        rowLayout->addWidget(m_batchPathEdit[i], 1);

        m_batchBrowseBtn[i] = new QPushButton(m_batchPanel);
        m_batchBrowseBtn[i]->setObjectName(QString::fromLatin1(rowSpecs[i].browseName));
        m_batchBrowseBtn[i]->setMinimumSize(QSize(96, 32));
        m_batchBrowseBtn[i]->setMaximumSize(QSize(96, 32));
        m_batchBrowseBtn[i]->setProperty("buttonRole", QStringLiteral("secondary"));
        m_batchBrowseBtn[i]->setText(tr("浏览"));
        rowLayout->addWidget(m_batchBrowseBtn[i]);
    }

    // 浮窗底部：状态文字（进度 / 完成 / 错误）
    m_batchStatusLabel = new QLabel(m_batchPanel);
    m_batchStatusLabel->setObjectName(QStringLiteral("registerRwBatchStatusLabel"));
    {
        QFont f = m_batchStatusLabel->font();
        f.setPixelSize(11);
        m_batchStatusLabel->setFont(f);
        QPalette pal = m_batchStatusLabel->palette();
        pal.setColor(QPalette::WindowText, Style::Color::FwFlashStageLabelFg);
        m_batchStatusLabel->setPalette(pal);
    }
    m_batchStatusLabel->setWordWrap(false);
    m_batchStatusLabel->setText(QString());
    m_batchStatusLabel->setMinimumHeight(20);
    batchLayout->addWidget(m_batchStatusLabel);

    // DEC/HEX 互斥按钮组
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_decButton);
    modeGroup->addButton(m_hexButton);
}

// =============================================================================
// 批量读写浮窗 UI 行为（业务逻辑在 BatchRegisterService）
// =============================================================================

void RegisterRwTab::onBatchBrowseClicked(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= 4) return;
    if (isBusy()) return;

    const QString caption = (slotIndex < 2)
        ? tr("选择批量写入的配置文件")
        : tr("选择批量读出的配置文件");
    const QString filter = tr("配置文件 (*.txt);;所有文件 (*.*)");
    const QString path = QFileDialog::getOpenFileName(
        m_batchPanel, caption, m_batchLastBrowseDir, filter);
    if (path.isEmpty()) return;

    m_batchPathEdit[slotIndex]->setText(path);
    m_batchLastBrowseDir = QFileInfo(path).absolutePath();
}

void RegisterRwTab::onBatchActionClicked(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= 4) return;
    if (isBusy()) return;

    const QString path = m_batchPathEdit[slotIndex]->text().trimmed();
    setBusyOwner(slotIndexToOwner(slotIndex));

    if (slotIndex < 2) {
        m_batchService->startWrite(slotIndex, path);
    } else {
        m_batchService->startRead(slotIndex, path);
    }
}

// -----------------------------------------------------------------------------
// BatchRegisterService 信号回调（UI 渲染层）
// -----------------------------------------------------------------------------

void RegisterRwTab::onBatchServiceStageMessage(const QString &message) {
    if (m_batchStatusLabel != nullptr) {
        m_batchStatusLabel->setText(message);
    }
}

void RegisterRwTab::onBatchServiceLog(BatchRegisterService::LogLevel level, const QString &message) {
    // Service 内部已 qCInfo / qCWarning 走全局 LogPanel；此处仅作为未来局部日志展示 hook
    Q_UNUSED(level);
    Q_UNUSED(message);
}

void RegisterRwTab::onBatchServiceFinished(bool success, const QString &summary) {
    Q_UNUSED(success);
    Q_UNUSED(summary);
    // 释放批量浮窗占用的互斥位（如果是浮窗任务）
    if (m_busyOwner == BusyOwner::BatchSlot1 || m_busyOwner == BusyOwner::BatchSlot2
        || m_busyOwner == BusyOwner::BatchSlot3 || m_busyOwner == BusyOwner::BatchSlot4) {
        setBusyOwner(BusyOwner::None);
    }
}

// -----------------------------------------------------------------------------
// 互斥锁
// -----------------------------------------------------------------------------

void RegisterRwTab::setBusyOwner(BusyOwner owner) {
    if (m_busyOwner == owner) return;
    m_busyOwner = owner;
    updateBusyUi();
}

void RegisterRwTab::updateBusyUi() {
    const bool busy = isBusy();
    for (int i = 0; i < 4; ++i) {
        if (m_batchBtn[i] != nullptr) m_batchBtn[i]->setEnabled(!busy);
        if (m_batchBrowseBtn[i] != nullptr) m_batchBrowseBtn[i]->setEnabled(!busy);
    }
    if (m_readAllButton != nullptr) m_readAllButton->setEnabled(!busy);
    if (m_writeAllButton != nullptr) m_writeAllButton->setEnabled(!busy);
}

RegisterRwTab::BusyOwner RegisterRwTab::slotIndexToOwner(int slotIndex) {
    switch (slotIndex) {
    case 0: return BusyOwner::BatchSlot1;
    case 1: return BusyOwner::BatchSlot2;
    case 2: return BusyOwner::BatchSlot3;
    case 3: return BusyOwner::BatchSlot4;
    default: return BusyOwner::None;
    }
}
