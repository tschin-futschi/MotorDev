// =============================================================================
// @file    registerrwtab.cpp
// @brief   寄存器读写页面实现 — UI 构建、信号槽连接、配置持久化
// =============================================================================
#include "tabs/registerrwtab.h"

#include "services/commanddispatcher.h"
#include "services/registerservice.h"
#include "ui/style_constants.h"
#include "widgets/registertable.h"
#include "widgets/sidebar.h"

#include <QButtonGroup>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QStandardPaths>
#include <QSpacerItem>
#include <QVBoxLayout>

using namespace MotorDev;

// =============================================================================
// 构造 / 析构
// =============================================================================

RegisterRwTab::RegisterRwTab(CommandDispatcher *dispatcher, QWidget *parent)
    : QWidget(parent) {
    m_service = new RegisterService(dispatcher, this);
    setupUi();

    // 批量操作区域当前未实现，禁用所有控件
    for (int i = 0; i < 4; ++i) {
        m_batchBtn[i]->setEnabled(false);
        m_batchBtn[i]->setToolTip(tr("功能开发中"));
        m_batchBrowseBtn[i]->setEnabled(false);
        m_batchBrowseBtn[i]->setToolTip(tr("功能开发中"));
        m_batchDescEdit[i]->setEnabled(false);
        m_batchDescEdit[i]->setToolTip(tr("功能开发中"));
        m_batchPathEdit[i]->setEnabled(false);
        m_batchPathEdit[i]->setToolTip(tr("功能开发中"));
    }
    connectSignals();

    // 从持久化文件加载上次的寄存器配置
    m_registerTable->loadConfig(configFilePath());
}

RegisterRwTab::~RegisterRwTab() = default;

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
    // 全量读取/写入
    // -------------------------------------------------------------------------

    // 全部读取：收集所有有地址的行，发起批量读取
    connect(m_readAllButton, &QPushButton::clicked, this, [this]() {
        m_readAllButton->setEnabled(false);
        m_writeAllButton->setEnabled(false);
        QVector<RegisterService::RowRequest> rows;
        for (int row = 0; row < Style::Size::TableGroupCount * Style::Size::TableRowCount; ++row) {
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
        m_readAllButton->setEnabled(false);
        m_writeAllButton->setEnabled(false);
        QVector<RegisterService::RowRequest> rows;
        for (int row = 0; row < Style::Size::TableGroupCount * Style::Size::TableRowCount; ++row) {
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

    // 批量操作完成 → 恢复按钮，自动保存读取结果
    connect(m_service, &RegisterService::queueFinished, this, [this](bool wasWrite) {
        m_readAllButton->setEnabled(true);
        m_writeAllButton->setEnabled(true);
        if (!wasWrite) m_registerTable->saveConfig(configFilePath());
    });
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
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    topLayout->addWidget(m_mainContent);

    // 垂直分割器：上方表格 | 下方批量操作区
    m_mainSplitter = new QSplitter(Qt::Vertical, m_mainContent);
    m_mainSplitter->setObjectName(QStringLiteral("mainSplitter"));
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(8);
    mainLayout->addWidget(m_mainSplitter);

    // 上方：寄存器表格
    auto *topWidget = new QWidget(m_mainSplitter);
    topWidget->setObjectName(QStringLiteral("topWidget"));
    auto *topWidgetLayout = new QVBoxLayout(topWidget);
    topWidgetLayout->setObjectName(QStringLiteral("topWidgetLayout"));
    topWidgetLayout->setSpacing(0);
    topWidgetLayout->setContentsMargins(0, 0, 0, 0);
    m_registerTable = new RegisterTable(topWidget);
    m_registerTable->setObjectName(QStringLiteral("registerTable"));
    topWidgetLayout->addWidget(m_registerTable);

    // 下方：批量读写区域
    auto *bottomWidget = new QWidget(m_mainSplitter);
    bottomWidget->setObjectName(QStringLiteral("bottomWidget"));
    auto *bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setObjectName(QStringLiteral("bottomLayout"));
    bottomLayout->setSpacing(16);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // 批量读写卡片
    auto *batchGroup = new QGroupBox(bottomWidget);
    batchGroup->setObjectName(QStringLiteral("batchGroup"));
    batchGroup->setTitle(tr("批量读写"));
    batchGroup->setProperty("panelRole", QStringLiteral("card"));
    auto *batchLayout = new QVBoxLayout(batchGroup);
    batchLayout->setObjectName(QStringLiteral("batchLayout"));
    batchLayout->setSpacing(10);
    batchLayout->setContentsMargins(24, 24, 24, 24);
    bottomLayout->addWidget(batchGroup);

    // 4 组批量操作行：前 2 行为"批量写入"，后 2 行为"批量读出"
    const struct {
        const char *buttonName;
        const char *descName;
        const char *pathName;
        const char *browseName;
        const char *buttonText;
    } rowSpecs[4] = {
        {"batchBtn0", "batchDescEdit0", "batchPathEdit0", "batchBrowseBtn0", "批量写入"},
        {"batchBtn1", "batchDescEdit1", "batchPathEdit1", "batchBrowseBtn1", "批量写入"},
        {"batchBtn2", "batchDescEdit2", "batchPathEdit2", "batchBrowseBtn2", "批量读出"},
        {"batchBtn3", "batchDescEdit3", "batchPathEdit3", "batchBrowseBtn3", "批量读出"},
    };

    for (int i = 0; i < 4; ++i) {
        auto *rowLayout = new QHBoxLayout();
        rowLayout->setObjectName(QStringLiteral("batchRow%1").arg(i));
        rowLayout->setSpacing(10);
        batchLayout->addLayout(rowLayout);

        m_batchBtn[i] = new QPushButton(batchGroup);
        m_batchBtn[i]->setObjectName(QString::fromLatin1(rowSpecs[i].buttonName));
        m_batchBtn[i]->setMinimumSize(QSize(96, 32));
        m_batchBtn[i]->setMaximumSize(QSize(96, 32));
        m_batchBtn[i]->setProperty("buttonRole", QStringLiteral("primary-sidebar"));
        m_batchBtn[i]->setText(tr(rowSpecs[i].buttonText));
        rowLayout->addWidget(m_batchBtn[i]);

        m_batchDescEdit[i] = new QLineEdit(batchGroup);
        m_batchDescEdit[i]->setObjectName(QString::fromLatin1(rowSpecs[i].descName));
        m_batchDescEdit[i]->setProperty("inputRole", QStringLiteral("form"));
        rowLayout->addWidget(m_batchDescEdit[i]);

        m_batchPathEdit[i] = new QLineEdit(batchGroup);
        m_batchPathEdit[i]->setObjectName(QString::fromLatin1(rowSpecs[i].pathName));
        m_batchPathEdit[i]->setReadOnly(true);
        m_batchPathEdit[i]->setProperty("inputRole", QStringLiteral("form"));
        rowLayout->addWidget(m_batchPathEdit[i]);

        m_batchBrowseBtn[i] = new QPushButton(batchGroup);
        m_batchBrowseBtn[i]->setObjectName(QString::fromLatin1(rowSpecs[i].browseName));
        m_batchBrowseBtn[i]->setMinimumSize(QSize(96, 32));
        m_batchBrowseBtn[i]->setMaximumSize(QSize(96, 32));
        m_batchBrowseBtn[i]->setProperty("buttonRole", QStringLiteral("secondary"));
        m_batchBrowseBtn[i]->setText(tr("浏览"));
        rowLayout->addWidget(m_batchBrowseBtn[i]);
    }
    batchLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // 两个占位卡片（预留未来扩展）
    for (int index = 0; index < 2; ++index) {
        auto *placeholderGroup = new QGroupBox(bottomWidget);
        placeholderGroup->setObjectName(QStringLiteral("placeholderGroup%1").arg(index + 1));
        placeholderGroup->setTitle(QString());
        placeholderGroup->setProperty("panelRole", QStringLiteral("card"));
        auto *placeholderLayout = new QVBoxLayout(placeholderGroup);
        placeholderLayout->setObjectName(QStringLiteral("placeholderLayout%1").arg(index + 1));
        placeholderLayout->setContentsMargins(24, 24, 24, 24);
        placeholderLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
        bottomLayout->addWidget(placeholderGroup);
    }

    // 表格区占 5/8，批量区占 3/8
    m_mainSplitter->setStretchFactor(0, 5);
    m_mainSplitter->setStretchFactor(1, 3);

    // DEC/HEX 互斥按钮组
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_decButton);
    modeGroup->addButton(m_hexButton);
}
