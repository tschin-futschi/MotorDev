// =============================================================================
// @file    registerrwtab.cpp
// @brief   寄存器读写页面实现 — UI 构建、信号槽连接、配置持久化
// =============================================================================
#include "tabs/registerrwtab.h"

#include "services/batchregisterservice.h"
#include "services/blockreadservice.h"
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
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLineEdit>
#include <QDebug>
#include <QLabel>
#include <QProgressBar>
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
    m_blockReadService = new BlockReadService(dispatcher, this);
    setupUi();
    connectSignals();

    // 浏览对话框默认目录：Documents
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_batchLastBrowseDir = docs;
    m_blockReadLastDir = docs;
    if (m_blockReadDirEdit != nullptr) {
        m_blockReadDirEdit->setText(m_blockReadLastDir);
    }
    // 寄存器表内容不再自动加载/保存；改由配置页「配置文件」Read/Write 手动统一存取
}

RegisterRwTab::~RegisterRwTab() {
    // 两个浮窗均为 Qt::Tool 顶级窗口（parent=nullptr），手动 delete
    delete m_batchPanel;
    m_batchPanel = nullptr;
    delete m_blockReadPanel;
    m_blockReadPanel = nullptr;
}

// =============================================================================
// 事件过滤：浮窗 Close 时同步 toggle 按钮状态
// =============================================================================

bool RegisterRwTab::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Close) {
        // 用户点击窗口关闭按钮 → 取消对应 toggle，同步 UI 状态
        // 注：setChecked(false) 会触发 toggled 信号，lambda 内 hide() 但浮窗已被 close 路径隐藏，无副作用
        // 注：关闭块读取浮窗不会取消运行中的任务（任务后台继续，完成后文件正常落盘 — 决策 D6 隔壁 Risk 项）
        if (watched == m_batchPanel
            && m_batchToggleBtn != nullptr && m_batchToggleBtn->isChecked()) {
            m_batchToggleBtn->setChecked(false);
        } else if (watched == m_blockReadPanel
                   && m_blockReadToggleBtn != nullptr && m_blockReadToggleBtn->isChecked()) {
            m_blockReadToggleBtn->setChecked(false);
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

    // 页面清除：清空表格所有描述/地址/值（即时，无确认框；持久化由 RegisterTable 内部触发）
    connect(m_clearPageBtn, &QToolButton::clicked, this, [this]() {
        if (m_registerTable != nullptr) {
            m_registerTable->clearAll();
        }
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

    // Sidebar 全部读/写 完成 → 释放互斥
    // 注：m_service 的 queueFinished 也会在单行读写时触发，但此时 m_busyOwner != SidebarAll
    // 不进释放分支，无副作用。读取结果不再自动保存（改由配置页手动 Write）。
    connect(m_service, &RegisterService::queueFinished, this, [this](bool wasWrite) {
        Q_UNUSED(wasWrite);
        if (m_busyOwner == BusyOwner::SidebarAll) {
            setBusyOwner(BusyOwner::None);
        }
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

    // -------------------------------------------------------------------------
    // 块读取浮窗 toggle + 操作按钮 + Service 信号
    // -------------------------------------------------------------------------
    connect(m_blockReadToggleBtn, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_blockReadPanel->show();
            m_blockReadPanel->raise();
            m_blockReadPanel->activateWindow();
        } else {
            m_blockReadPanel->hide();
        }
    });

    connect(m_blockReadBrowseBtn, &QPushButton::clicked,
            this, &RegisterRwTab::onBlockReadBrowseDirClicked);
    connect(m_blockReadStartBtn, &QPushButton::clicked,
            this, &RegisterRwTab::onBlockReadStartClicked);
    connect(m_blockReadCancelBtn, &QPushButton::clicked,
            this, &RegisterRwTab::onBlockReadCancelClicked);

    connect(m_blockReadService, &BlockReadService::stateChanged,
            this, &RegisterRwTab::onBlockReadServiceStateChanged);
    connect(m_blockReadService, &BlockReadService::progress,
            this, &RegisterRwTab::onBlockReadServiceProgress);
    connect(m_blockReadService, &BlockReadService::stageMessage,
            this, &RegisterRwTab::onBlockReadServiceStageMessage);
    connect(m_blockReadService, &BlockReadService::logMessage,
            this, &RegisterRwTab::onBlockReadServiceLog);
    connect(m_blockReadService, &BlockReadService::finished,
            this, &RegisterRwTab::onBlockReadServiceFinished);
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

// =============================================================================
// 配置文件存取：寄存器表 section 采集 / 回填（转发给内部 RegisterTable）
// =============================================================================

QJsonArray RegisterRwTab::collectRegisterRows() const {
    return m_registerTable->toJsonRows();
}

void RegisterRwTab::applyRegisterRows(const QJsonArray &rows) {
    m_registerTable->fromJsonRows(rows);
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
    m_readAllButton->setMinimumSize(QSize(0, Style::Size::SidebarButtonHeight));
    m_readAllButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::SidebarButtonHeight));
    m_readAllButton->setProperty("buttonRole", QStringLiteral("primary-sidebar"));
    m_readAllButton->setText(tr("全部读取"));
    sidebarLayout->addWidget(m_readAllButton);

    m_writeAllButton = new QPushButton(m_sidebarContent);
    m_writeAllButton->setObjectName(QStringLiteral("writeAllButton"));
    m_writeAllButton->setMinimumSize(QSize(0, Style::Size::SidebarButtonHeight));
    m_writeAllButton->setMaximumSize(QSize(QWIDGETSIZE_MAX, Style::Size::SidebarButtonHeight));
    m_writeAllButton->setProperty("buttonRole", QStringLiteral("write-sidebar"));
    m_writeAllButton->setText(tr("全部写入"));
    sidebarLayout->addWidget(m_writeAllButton);

    sidebarLayout->addItem(new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    // Hex/Dec 模式切换
    m_valueModeLabel = new QLabel(m_sidebarContent);
    m_valueModeLabel->setObjectName(QStringLiteral("valueModeLabel"));
    m_valueModeLabel->setText(tr("数值表示"));
    sidebarLayout->addWidget(m_valueModeLabel);

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
    m_sidebar = new Sidebar(tr("读写"), m_sidebarContent, this);
    topLayout->addWidget(m_sidebar);

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

    // 页面清除：一次性动作按钮（非 toggle），清空表格所有描述/地址/值
    m_clearPageBtn = new QToolButton(m_mainContent);
    m_clearPageBtn->setObjectName(QStringLiteral("registerRwClearPageBtn"));
    m_clearPageBtn->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    m_clearPageBtn->setText(tr("页面清除"));
    toolbarRow->addWidget(m_clearPageBtn);
    toolbarRow->addSpacing(10);

    m_batchToggleBtn = new QToolButton(m_mainContent);
    m_batchToggleBtn->setObjectName(QStringLiteral("registerRwBatchToggleBtn"));
    m_batchToggleBtn->setCheckable(true);
    m_batchToggleBtn->setChecked(false);
    m_batchToggleBtn->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    m_batchToggleBtn->setText(tr("批量读写"));
    toolbarRow->addWidget(m_batchToggleBtn);

    // 紧挨「批量读写」放「块读取」按钮（仿同款风格：QToolButton checkable + 再点关闭）
    toolbarRow->addSpacing(10);
    m_blockReadToggleBtn = new QToolButton(m_mainContent);
    m_blockReadToggleBtn->setObjectName(QStringLiteral("registerRwBlockReadToggleBtn"));
    m_blockReadToggleBtn->setCheckable(true);
    m_blockReadToggleBtn->setChecked(false);
    m_blockReadToggleBtn->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    m_blockReadToggleBtn->setText(tr("块读取"));
    toolbarRow->addWidget(m_blockReadToggleBtn);

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
        m_batchBtn[i]->setMinimumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
        m_batchBtn[i]->setMaximumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
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
        m_batchBrowseBtn[i]->setMinimumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
        m_batchBrowseBtn[i]->setMaximumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
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

    // -------------------------------------------------------------------------
    // 块读取浮窗（独立 Qt::Tool 顶级窗口）
    // 布局（垂直）：
    //   起始地址：[__________]
    //   寄存器个数：[__________]
    //   保存目录：[__________________] [浏览...]
    //   [============进度条============] N%
    //   状态文字
    //   [开始读取]   [取消]
    // -------------------------------------------------------------------------
    m_blockReadPanel = new QWidget(nullptr, Qt::Tool);
    m_blockReadPanel->setObjectName(QStringLiteral("registerRwBlockReadPanel"));
    m_blockReadPanel->setWindowTitle(tr("块读取"));
    m_blockReadPanel->setAttribute(Qt::WA_DeleteOnClose, false);
    m_blockReadPanel->installEventFilter(this);
    m_blockReadPanel->resize(520, 240);
    m_blockReadPanel->hide();

    auto *blockLayout = new QVBoxLayout(m_blockReadPanel);
    blockLayout->setObjectName(QStringLiteral("blockReadLayout"));
    blockLayout->setSpacing(10);
    blockLayout->setContentsMargins(Style::Size::ContentSpacing,
                                     Style::Size::ContentSpacing,
                                     Style::Size::ContentSpacing,
                                     Style::Size::ContentSpacing);

    auto *form = new QFormLayout();
    form->setObjectName(QStringLiteral("blockReadForm"));
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    m_blockReadStartEdit = new QLineEdit(m_blockReadPanel);
    m_blockReadStartEdit->setObjectName(QStringLiteral("blockReadStartEdit"));
    m_blockReadStartEdit->setPlaceholderText(tr("0xB002 或 B002"));
    m_blockReadStartEdit->setProperty("inputRole", QStringLiteral("form"));
    m_blockStartLabel = new QLabel(tr("起始地址："), m_blockReadPanel);
    form->addRow(m_blockStartLabel, m_blockReadStartEdit);

    m_blockReadCountEdit = new QLineEdit(m_blockReadPanel);
    m_blockReadCountEdit->setObjectName(QStringLiteral("blockReadCountEdit"));
    m_blockReadCountEdit->setPlaceholderText(tr("十进制，例如 100"));
    m_blockReadCountEdit->setProperty("inputRole", QStringLiteral("form"));
    m_blockCountLabel = new QLabel(tr("寄存器个数："), m_blockReadPanel);
    form->addRow(m_blockCountLabel, m_blockReadCountEdit);

    // 保存目录行：[只读 LineEdit] [浏览按钮]
    auto *dirRow = new QHBoxLayout();
    dirRow->setObjectName(QStringLiteral("blockReadDirRow"));
    dirRow->setSpacing(8);
    m_blockReadDirEdit = new QLineEdit(m_blockReadPanel);
    m_blockReadDirEdit->setObjectName(QStringLiteral("blockReadDirEdit"));
    m_blockReadDirEdit->setReadOnly(true);
    m_blockReadDirEdit->setProperty("inputRole", QStringLiteral("form"));
    dirRow->addWidget(m_blockReadDirEdit, 1);
    m_blockReadBrowseBtn = new QPushButton(m_blockReadPanel);
    m_blockReadBrowseBtn->setObjectName(QStringLiteral("blockReadBrowseBtn"));
    m_blockReadBrowseBtn->setMinimumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
    m_blockReadBrowseBtn->setMaximumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
    m_blockReadBrowseBtn->setProperty("buttonRole", QStringLiteral("secondary"));
    m_blockReadBrowseBtn->setText(tr("浏览..."));
    dirRow->addWidget(m_blockReadBrowseBtn);
    m_blockDirLabel = new QLabel(tr("保存目录："), m_blockReadPanel);
    form->addRow(m_blockDirLabel, dirRow);

    blockLayout->addLayout(form);

    // 进度条
    m_blockReadProgressBar = new QProgressBar(m_blockReadPanel);
    m_blockReadProgressBar->setObjectName(QStringLiteral("blockReadProgressBar"));
    m_blockReadProgressBar->setRange(0, 100);
    m_blockReadProgressBar->setValue(0);
    m_blockReadProgressBar->setTextVisible(true);
    blockLayout->addWidget(m_blockReadProgressBar);

    // 状态文字
    m_blockReadStatusLabel = new QLabel(m_blockReadPanel);
    m_blockReadStatusLabel->setObjectName(QStringLiteral("blockReadStatusLabel"));
    {
        QFont f = m_blockReadStatusLabel->font();
        f.setPixelSize(11);
        m_blockReadStatusLabel->setFont(f);
        QPalette pal = m_blockReadStatusLabel->palette();
        pal.setColor(QPalette::WindowText, Style::Color::FwFlashStageLabelFg);
        m_blockReadStatusLabel->setPalette(pal);
    }
    m_blockReadStatusLabel->setWordWrap(false);
    m_blockReadStatusLabel->setText(QString());
    m_blockReadStatusLabel->setMinimumHeight(20);
    blockLayout->addWidget(m_blockReadStatusLabel);

    // 操作按钮行：开始 / 取消
    auto *btnRow = new QHBoxLayout();
    btnRow->setObjectName(QStringLiteral("blockReadBtnRow"));
    btnRow->setSpacing(10);
    btnRow->addStretch(1);
    m_blockReadStartBtn = new QPushButton(m_blockReadPanel);
    m_blockReadStartBtn->setObjectName(QStringLiteral("blockReadStartBtn"));
    m_blockReadStartBtn->setMinimumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
    m_blockReadStartBtn->setMaximumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
    m_blockReadStartBtn->setProperty("buttonRole", QStringLiteral("primary-sidebar"));
    m_blockReadStartBtn->setText(tr("开始读取"));
    btnRow->addWidget(m_blockReadStartBtn);

    m_blockReadCancelBtn = new QPushButton(m_blockReadPanel);
    m_blockReadCancelBtn->setObjectName(QStringLiteral("blockReadCancelBtn"));
    m_blockReadCancelBtn->setMinimumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
    m_blockReadCancelBtn->setMaximumSize(QSize(Style::Size::RegisterPopupButtonW, Style::Size::SidebarButtonHeight));
    m_blockReadCancelBtn->setProperty("buttonRole", QStringLiteral("secondary"));
    m_blockReadCancelBtn->setText(tr("取消"));
    m_blockReadCancelBtn->setEnabled(false);  // 空闲时禁用
    btnRow->addWidget(m_blockReadCancelBtn);
    blockLayout->addLayout(btnRow);

    retranslateUi();  // 统一初始化可见文字（语言切换时由 changeEvent 再次调用）
}

// =============================================================================
// 语言切换：即时刷新本页（含两浮窗）所有可见文字
// =============================================================================

void RegisterRwTab::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void RegisterRwTab::retranslateUi() {
    if (m_sidebar != nullptr) m_sidebar->setTitle(tr("读写"));
    if (m_readAllButton != nullptr) m_readAllButton->setText(tr("全部读取"));
    if (m_writeAllButton != nullptr) m_writeAllButton->setText(tr("全部写入"));
    if (m_valueModeLabel != nullptr) m_valueModeLabel->setText(tr("数值表示"));
    if (m_decButton != nullptr) m_decButton->setText(tr("DEC"));
    if (m_hexButton != nullptr) m_hexButton->setText(tr("HEX"));
    if (m_clearPageBtn != nullptr) m_clearPageBtn->setText(tr("页面清除"));
    if (m_batchToggleBtn != nullptr) m_batchToggleBtn->setText(tr("批量读写"));
    if (m_blockReadToggleBtn != nullptr) m_blockReadToggleBtn->setText(tr("块读取"));

    // 批量读写浮窗
    if (m_batchPanel != nullptr) m_batchPanel->setWindowTitle(tr("批量读写"));
    for (int i = 0; i < 4; ++i) {
        if (m_batchBtn[i] != nullptr) m_batchBtn[i]->setText(i < 2 ? tr("批量写入") : tr("批量读出"));
        if (m_batchBrowseBtn[i] != nullptr) m_batchBrowseBtn[i]->setText(tr("浏览"));
    }

    // 块读取浮窗
    if (m_blockReadPanel != nullptr) m_blockReadPanel->setWindowTitle(tr("块读取"));
    if (m_blockReadStartEdit != nullptr) m_blockReadStartEdit->setPlaceholderText(tr("0xB002 或 B002"));
    if (m_blockReadCountEdit != nullptr) m_blockReadCountEdit->setPlaceholderText(tr("十进制，例如 100"));
    if (m_blockStartLabel != nullptr) m_blockStartLabel->setText(tr("起始地址："));
    if (m_blockCountLabel != nullptr) m_blockCountLabel->setText(tr("寄存器个数："));
    if (m_blockDirLabel != nullptr) m_blockDirLabel->setText(tr("保存目录："));
    if (m_blockReadBrowseBtn != nullptr) m_blockReadBrowseBtn->setText(tr("浏览..."));
    if (m_blockReadStartBtn != nullptr) m_blockReadStartBtn->setText(tr("开始读取"));
    if (m_blockReadCancelBtn != nullptr) m_blockReadCancelBtn->setText(tr("取消"));
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

// =============================================================================
// 块读取浮窗 UI 行为（业务逻辑在 BlockReadService；与全局互斥位 m_busyOwner 解耦）
// =============================================================================

namespace {
/// @brief 解析 hex 地址：接受 `0xB002` 与 `B002` 两种格式
/// @return true=解析成功；输出 quint16 地址
bool parseHexAddressFlexible(const QString &text, quint16 *out) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || out == nullptr) return false;
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        trimmed.remove(0, 2);
    }
    bool ok = false;
    const quint16 v = trimmed.toUShort(&ok, 16);
    if (!ok) return false;
    *out = v;
    return true;
}
}  // namespace

void RegisterRwTab::onBlockReadBrowseDirClicked() {
    if (m_blockReadService != nullptr && m_blockReadService->isBusy()) return;

    const QString dir = QFileDialog::getExistingDirectory(
        m_blockReadPanel,
        tr("选择 CSV 保存目录"),
        m_blockReadLastDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    m_blockReadLastDir = dir;
    if (m_blockReadDirEdit != nullptr) {
        m_blockReadDirEdit->setText(dir);
    }
}

void RegisterRwTab::onBlockReadStartClicked() {
    if (m_blockReadService == nullptr) return;
    if (m_blockReadService->isBusy()) return;

    quint16 startAddr = 0;
    if (!parseHexAddressFlexible(m_blockReadStartEdit->text(), &startAddr)) {
        m_blockReadStatusLabel->setText(tr("起始地址格式错误（应为 0xXXXX 或 XXXX）"));
        return;
    }

    bool ok = false;
    const int count = m_blockReadCountEdit->text().trimmed().toInt(&ok);
    if (!ok || count < 1) {
        m_blockReadStatusLabel->setText(tr("寄存器个数必须为正整数"));
        return;
    }

    const quint32 endAddr = static_cast<quint32>(startAddr) + 2u * static_cast<quint32>(count - 1);
    if (endAddr > 0xFFFEu) {
        const QString hex = QString::number(startAddr, 16).toUpper().rightJustified(4, QLatin1Char('0'));
        m_blockReadStatusLabel->setText(
            tr("地址范围越界：起始 0x%1 + %2 个寄存器超过 0xFFFE 上限").arg(hex).arg(count));
        return;
    }

    if (m_blockReadLastDir.isEmpty() || !QFileInfo(m_blockReadLastDir).isDir()) {
        m_blockReadStatusLabel->setText(tr("请先选择保存目录"));
        return;
    }

    // 重置进度条 + 提交任务
    if (m_blockReadProgressBar != nullptr) {
        m_blockReadProgressBar->setRange(0, count);
        m_blockReadProgressBar->setValue(0);
    }
    m_blockReadService->start(startAddr, count, m_blockReadLastDir);
}

void RegisterRwTab::onBlockReadCancelClicked() {
    if (m_blockReadService != nullptr) {
        m_blockReadService->cancel();
    }
}

// -----------------------------------------------------------------------------
// BlockReadService 信号回调（UI 渲染层）
// -----------------------------------------------------------------------------

void RegisterRwTab::onBlockReadServiceStateChanged(BlockReadService::State state) {
    // 按钮启用态：Reading / WritingFile 时禁用 Start / 输入字段 / 浏览，启用 Cancel
    const bool busy = (state == BlockReadService::State::Reading
                       || state == BlockReadService::State::WritingFile);
    if (m_blockReadStartBtn != nullptr) m_blockReadStartBtn->setEnabled(!busy);
    if (m_blockReadStartEdit != nullptr) m_blockReadStartEdit->setEnabled(!busy);
    if (m_blockReadCountEdit != nullptr) m_blockReadCountEdit->setEnabled(!busy);
    if (m_blockReadBrowseBtn != nullptr) m_blockReadBrowseBtn->setEnabled(!busy);
    if (m_blockReadCancelBtn != nullptr) {
        // 取消仅在 Reading 阶段有意义；WritingFile 已无法中断
        m_blockReadCancelBtn->setEnabled(state == BlockReadService::State::Reading);
    }
}

void RegisterRwTab::onBlockReadServiceProgress(int done, int total) {
    if (m_blockReadProgressBar != nullptr) {
        if (m_blockReadProgressBar->maximum() != total) {
            m_blockReadProgressBar->setRange(0, total);
        }
        m_blockReadProgressBar->setValue(done);
    }
}

void RegisterRwTab::onBlockReadServiceStageMessage(const QString &message) {
    if (m_blockReadStatusLabel != nullptr) {
        m_blockReadStatusLabel->setText(message);
    }
}

void RegisterRwTab::onBlockReadServiceLog(BlockReadService::LogLevel level, const QString &message) {
    // Service 内部已 qCInfo / qCWarning 走全局 LogPanel；此处仅作为未来局部日志展示 hook
    Q_UNUSED(level);
    Q_UNUSED(message);
}

void RegisterRwTab::onBlockReadServiceFinished(bool success, const QString &summary, const QString &savedPath) {
    Q_UNUSED(success);
    Q_UNUSED(summary);
    Q_UNUSED(savedPath);
    // 进度条与状态文字已由 progress / stageMessage 信号渲染；
    // 按钮启用态由 stateChanged 信号收口（finished 之后 service setState(Idle)）
    // 此处无需额外动作。
}
