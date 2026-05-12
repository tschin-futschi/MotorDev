// =============================================================================
// @file    fwflashtab.cpp
// @brief   固件烧录页面实现（v3）— 顶部紧凑 IC 行 + 左右分栏 60/40
//
// 布局调整自 v2：
// - "目标 IC" 不再独立 GroupBox，改为左栏顶部一行紧凑控件
// - QSplitter 默认比例 60/40，让固件文件区拓宽，操作日志区收窄
// - 进度条改为渐变绿色 QSS（在 FwFlashControlPanel 中实现）
// =============================================================================
#include "tabs/fwflashtab.h"

#include "protocol/firmware_parser.h"
#include "services/flashstrategies/aw_sdk_strategy.h"
#include "services/flashstrategy.h"
#include "services/flashstrategyregistry.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"
#include "widgets/fwfileinfopanel.h"
#include "widgets/fwflashcontrolpanel.h"
#include "widgets/fwflashlogpanel.h"

#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {

class GroupHoverFilter : public QObject {
public:
    explicit GroupHoverFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget == nullptr) return QObject::eventFilter(watched, event);
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

void enableCardHover(QWidget *widget, GroupHoverFilter *filter) {
    widget->setAttribute(Qt::WA_Hover, true);
    widget->installEventFilter(filter);
    applyPanelShadow(widget);
}

}  // namespace

FwFlashTab::FwFlashTab(CommandDispatcher *dispatcher, QWidget *parent)
    : QWidget(parent) {
    setupUi();

    // FwFlashLogPanel 在 setupUi() 中创建；构造 logSink lambda 把 AwSdkStrategy 的
    // 4 级日志 marshal 到 GUI 线程并转发到面板的 4 个 append 槽。QPointer 防御面板
    // 先于 strategy 销毁。
    QPointer<FwFlashLogPanel> logPanelPtr(m_logPanel);
    auto logSink = [logPanelPtr](AwSdkStrategy::LogLevel lv, const QString &msg) {
        if (logPanelPtr.isNull()) return;
        QMetaObject::invokeMethod(logPanelPtr.data(), [logPanelPtr, lv, msg]() {
            if (logPanelPtr.isNull()) return;
            switch (lv) {
            case AwSdkStrategy::LogLevel::Info:  logPanelPtr->appendInfo(msg); break;
            case AwSdkStrategy::LogLevel::Warn:  logPanelPtr->appendWarn(msg); break;
            case AwSdkStrategy::LogLevel::Error: logPanelPtr->appendError(msg); break;
            case AwSdkStrategy::LogLevel::Ok:    logPanelPtr->appendOk(msg); break;
            }
        }, Qt::QueuedConnection);
    };

    m_registry = std::make_unique<FlashStrategyRegistry>(dispatcher, logSink);
    m_service = new FwFlashService(m_registry.get(), this);

    connectSignals();
    rebuildIcCombo();

    onIcChanged(m_icCombo->currentIndex());
    updateStartEnabled();
    m_controlPanel->setStageText(tr("空闲"));
}

FwFlashTab::~FwFlashTab() = default;

void FwFlashTab::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 用户拖动 splitter 后比例由 stretchFactor 维持，无需再强制等分
}

void FwFlashTab::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // 仅第一次显示时设初始 1:1；之后用户拖动手柄改变的比例不再被覆盖
    if (m_splitter == nullptr || m_splitterInitialSized) return;
    const int total = m_splitter->width();
    if (total <= 100) return;
    const int half = total / 2;
    m_splitter->setSizes({half, half});
    m_splitterInitialSized = true;
}

// -----------------------------------------------------------------------------
// 外部依赖注入
// -----------------------------------------------------------------------------

void FwFlashTab::setStopScopeCallback(std::function<void()> cb) {
    m_service->setStopScopeCallback(std::move(cb));
}
void FwFlashTab::setStopGeneratorCallback(std::function<void()> cb) {
    m_service->setStopGeneratorCallback(std::move(cb));
}
void FwFlashTab::setStopCyclicWriteCallback(std::function<void()> cb) {
    m_service->setStopCyclicWriteCallback(std::move(cb));
}
void FwFlashTab::setDisablePmicCallback(std::function<void()> cb) {
    m_service->setDisablePmicCallback(std::move(cb));
}

// -----------------------------------------------------------------------------
// UI 构建
// -----------------------------------------------------------------------------

void FwFlashTab::setupUi() {
    setObjectName(QStringLiteral("FwFlashTab"));

    auto *topLayout = new QHBoxLayout(this);
    topLayout->setSpacing(0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    // --- 主内容区：QHBoxLayout 包 QSplitter，让用户能拖动手柄调宽 ---
    m_mainContent = new QWidget(this);
    m_mainContent->setObjectName(QStringLiteral("fwFlashMainContent"));
    auto *mainLayout = new QHBoxLayout(m_mainContent);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(Style::Size::ContentPadding,
                                    Style::Size::ContentPadding,
                                    Style::Size::ContentPadding,
                                    Style::Size::ContentPadding);

    m_splitter = new QSplitter(Qt::Horizontal, m_mainContent);
    m_splitter->setObjectName(QStringLiteral("fwFlashSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(Style::Size::SplitterHandleWidth);
    mainLayout->addWidget(m_splitter);

    auto *hoverFilter = new GroupHoverFilter(this);

    // ============================================================
    // 左栏：参数区
    // ============================================================
    m_leftPane = new QWidget(m_splitter);
    m_leftPane->setObjectName(QStringLiteral("fwFlashLeftPane"));
    m_leftPane->setMinimumWidth(280);
    auto *leftLayout = new QVBoxLayout(m_leftPane);
    leftLayout->setSpacing(Style::Size::ContentSpacing);
    leftLayout->setContentsMargins(0, 0, Style::Size::ContentSpacing / 2, 0);
    m_splitter->addWidget(m_leftPane);
    auto *leftPane = m_leftPane;

    // 顶部紧凑 IC 选择行（不带 GroupBox，节省垂直空间）
    auto *icRow = new QHBoxLayout();
    icRow->setContentsMargins(2, 0, 2, 0);
    icRow->setSpacing(8);

    auto *icLabel = new QLabel(tr("目标 IC："), leftPane);
    QFont normalFont = icLabel->font();
    normalFont.setPixelSize(11);
    icLabel->setFont(normalFont);
    icRow->addWidget(icLabel);

    m_icCombo = new QComboBox(leftPane);
    m_icCombo->setObjectName(QStringLiteral("fwFlashIcCombo"));
    m_icCombo->setMinimumWidth(Style::Size::FwFlashIcComboW);
    m_icCombo->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    icRow->addWidget(m_icCombo);

    m_icDescLabel = new QLabel(leftPane);
    m_icDescLabel->setObjectName(QStringLiteral("fwFlashIcDescLabel"));
    m_icDescLabel->setFont(normalFont);
    QPalette descPal = m_icDescLabel->palette();
    descPal.setColor(QPalette::WindowText, Style::Color::MutedText);
    m_icDescLabel->setPalette(descPal);
    icRow->addWidget(m_icDescLabel, 1);
    leftLayout->addLayout(icRow);

    // 固件文件卡片（占左栏大头）
    auto *fileCard = new QGroupBox(tr("固件文件"), leftPane);
    fileCard->setObjectName(QStringLiteral("fwFlashFileCard"));
    auto *fileCardLayout = new QVBoxLayout(fileCard);
    fileCardLayout->setSpacing(10);
    fileCardLayout->setContentsMargins(Style::Size::ContentSpacing,
                                        Style::Size::ContentSpacing + Style::Size::GroupBoxTopMargin,
                                        Style::Size::ContentSpacing,
                                        Style::Size::ContentSpacing);

    // 路径行
    auto *pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    pathRow->setContentsMargins(0, 0, 0, 0);

    m_pathEdit = new QLineEdit(fileCard);
    m_pathEdit->setObjectName(QStringLiteral("fwFlashPathEdit"));
    m_pathEdit->setReadOnly(true);
    m_pathEdit->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    m_pathEdit->setPlaceholderText(tr("点击「浏览...」选择 .bin 或 .hex 固件文件"));
    pathRow->addWidget(m_pathEdit, 1);

    m_browseBtn = new QPushButton(tr("浏览..."), fileCard);
    m_browseBtn->setObjectName(QStringLiteral("fwFlashBrowseBtn"));
    m_browseBtn->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    pathRow->addWidget(m_browseBtn);

    m_clearFileBtn = new QPushButton(tr("清空"), fileCard);
    m_clearFileBtn->setObjectName(QStringLiteral("fwFlashClearFileBtn"));
    m_clearFileBtn->setMinimumHeight(Style::Size::SidebarComboMinHeight);
    pathRow->addWidget(m_clearFileBtn);
    fileCardLayout->addLayout(pathRow);

    // 分隔线
    auto *divider = new QFrame(fileCard);
    divider->setObjectName(QStringLiteral("fwFlashFileDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    fileCardLayout->addWidget(divider);

    // 文件信息（内嵌 widget）
    m_infoPanel = new FwFileInfoPanel(fileCard);
    fileCardLayout->addWidget(m_infoPanel, 1);

    enableCardHover(fileCard, hoverFilter);
    leftLayout->addWidget(fileCard, 1);

    // ============================================================
    // 右栏：执行/输出区
    // ============================================================
    m_rightPane = new QWidget(m_splitter);
    m_rightPane->setObjectName(QStringLiteral("fwFlashRightPane"));
    m_rightPane->setMinimumWidth(280);
    auto *rightLayout = new QVBoxLayout(m_rightPane);
    rightLayout->setSpacing(Style::Size::ContentSpacing);
    rightLayout->setContentsMargins(Style::Size::ContentSpacing / 2, 0, 0, 0);
    m_splitter->addWidget(m_rightPane);
    auto *rightPane = m_rightPane;

    // 窗口 resize 时按当前比例分配空间（用户拖动后比例保持）
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    m_controlPanel = new FwFlashControlPanel(rightPane);
    enableCardHover(m_controlPanel, hoverFilter);
    rightLayout->addWidget(m_controlPanel);

    m_logPanel = new FwFlashLogPanel(rightPane);
    enableCardHover(m_logPanel, hoverFilter);
    rightLayout->addWidget(m_logPanel, 1);

    topLayout->addWidget(m_mainContent, 1);
}

// -----------------------------------------------------------------------------
// 信号槽连接
// -----------------------------------------------------------------------------

void FwFlashTab::connectSignals() {
    connect(m_browseBtn, &QPushButton::clicked, this, &FwFlashTab::onBrowseClicked);
    connect(m_clearFileBtn, &QPushButton::clicked, this, &FwFlashTab::onClearFileClicked);
    connect(m_icCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &FwFlashTab::onIcChanged);

    connect(m_controlPanel, &FwFlashControlPanel::startRequested,
            this, &FwFlashTab::onStartRequested);
    connect(m_controlPanel, &FwFlashControlPanel::cancelRequested,
            this, &FwFlashTab::onCancelRequested);

    connect(m_service, &FwFlashService::stateChanged, this, &FwFlashTab::onServiceStateChanged);
    connect(m_service, &FwFlashService::stageMessage, this, &FwFlashTab::onServiceStage);
    connect(m_service, &FwFlashService::progressUpdated, this, &FwFlashTab::onServiceProgress);
    connect(m_service, &FwFlashService::logMessage, this, &FwFlashTab::onServiceLog);
    connect(m_service, &FwFlashService::finished, this, &FwFlashTab::onServiceFinished);
}

void FwFlashTab::rebuildIcCombo() {
    m_icCombo->blockSignals(true);
    m_icCombo->clear();
    m_icCombo->addItem(tr("请选择目标 IC..."), QString());
    for (FlashStrategy *s : m_registry->all()) {
        m_icCombo->addItem(s->icModel(), s->icModel());
    }
    m_icCombo->setCurrentIndex(0);
    m_icCombo->blockSignals(false);
}

// -----------------------------------------------------------------------------
// 文件选择 / 解析
// -----------------------------------------------------------------------------

void FwFlashTab::onBrowseClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择固件文件"),
        QString(),
        tr("Firmware Files (*.bin *.hex);;Binary (*.bin);;Intel HEX (*.hex)"));
    if (path.isEmpty()) return;
    parseAndShowFile(path);
}

void FwFlashTab::onClearFileClicked() {
    clearFileState();
    m_logPanel->appendInfo(tr("已清空文件选择"));
}

void FwFlashTab::parseAndShowFile(const QString &path) {
    m_currentFilePath = path;
    m_pathEdit->setText(path);

    const FirmwareInfo info = FirmwareParser::parseFile(path);
    m_infoPanel->setInfo(info);

    if (!info.valid) {
        m_currentFileValid = false;
        m_currentFirmwareData.clear();
        m_currentFirmwareTotal = 0;
        m_logPanel->appendError(tr("文件解析失败：%1").arg(info.errorMessage));
    } else {
        m_currentFileValid = true;
        m_currentFirmwareData = info.data;
        m_currentFirmwareTotal = info.data.size();
        m_logPanel->appendInfo(
            tr("已加载固件：%1（%2 字节，CRC32 0x%3）")
                .arg(info.fileName)
                .arg(info.data.size())
                .arg(info.crc32, 8, 16, QLatin1Char('0')));
    }
    updateStartEnabled();
}

void FwFlashTab::clearFileState() {
    m_currentFilePath.clear();
    m_currentFileValid = false;
    m_currentFirmwareData.clear();
    m_currentFirmwareTotal = 0;
    m_pathEdit->clear();
    m_infoPanel->clear();
    updateStartEnabled();
}

// -----------------------------------------------------------------------------
// IC 选择 / 启用判断
// -----------------------------------------------------------------------------

void FwFlashTab::onIcChanged(int /*index*/) {
    const QString icModel = m_icCombo->currentData().toString();
    if (icModel.isEmpty()) {
        m_icDescLabel->clear();
    } else {
        FlashStrategy *s = m_registry->find(icModel);
        m_icDescLabel->setText(s != nullptr ? s->icDescription() : QString());
    }
    updateStartEnabled();
}

void FwFlashTab::updateStartEnabled() {
    const bool icSelected = !m_icCombo->currentData().toString().isEmpty();
    const bool busy = m_service->isBusy();
    m_controlPanel->setStartEnabled(!busy && icSelected && m_currentFileValid);
    m_controlPanel->setCancelEnabled(busy);

    m_icCombo->setEnabled(!busy);
    m_browseBtn->setEnabled(!busy);
    m_clearFileBtn->setEnabled(!busy && (m_currentFileValid || !m_currentFilePath.isEmpty()));
}

// -----------------------------------------------------------------------------
// 烧录启停
// -----------------------------------------------------------------------------

void FwFlashTab::onStartRequested() {
    const QString icModel = m_icCombo->currentData().toString();
    if (icModel.isEmpty() || !m_currentFileValid) {
        m_logPanel->appendWarn(tr("请先选择 IC 与有效固件文件"));
        return;
    }
    m_service->startFlash(icModel, m_currentFirmwareData, m_currentFirmwareTotal);
}

void FwFlashTab::onCancelRequested() {
    m_service->cancelFlash();
}

// -----------------------------------------------------------------------------
// 服务回调
// -----------------------------------------------------------------------------

void FwFlashTab::onServiceStateChanged(FwFlashService::State /*newState*/) {
    updateStartEnabled();
}

void FwFlashTab::onServiceStage(const QString &message) {
    m_controlPanel->setStageText(message);
}

void FwFlashTab::onServiceProgress(qint64 sent, qint64 total) {
    m_controlPanel->setProgress(sent, total);
}

void FwFlashTab::onServiceLog(FwFlashService::LogLevel level, const QString &message) {
    switch (level) {
    case FwFlashService::LogLevel::Info:  m_logPanel->appendInfo(message); break;
    case FwFlashService::LogLevel::Warn:  m_logPanel->appendWarn(message); break;
    case FwFlashService::LogLevel::Error: m_logPanel->appendError(message); break;
    case FwFlashService::LogLevel::Ok:    m_logPanel->appendOk(message); break;
    }
}

void FwFlashTab::onServiceFinished(bool /*success*/, const QString & /*summary*/) {
    updateStartEnabled();
}
