// =============================================================================
// @file    fwflashtab.cpp
// @brief   固件烧录页面实现（v3）— 顶部紧凑 IC 行 + 左右分栏 QSplitter
//
// 布局调整自 v2：
// - "目标 IC" 不再独立 GroupBox，改为左栏顶部一行紧凑控件
// - QSplitter 首次显示比例 1:1（用户拖动手柄后比例保持，由 stretchFactor 维持）
// - 进度条改为渐变绿色 QSS（在 FwFlashControlPanel 中实现）
// =============================================================================
#include "tabs/fwflashtab.h"

#include "devicecontext.h"
#include "protocol/firmware_parser.h"
#include "services/flashstrategies/aw_local_isp_strategy.h"
#include "services/flashstrategy.h"
#include "services/flashstrategyregistry.h"
#include "ui/repolish.h"
#include "ui/style_constants.h"
#include "widgets/fwfileinfopanel.h"
#include "widgets/fwflashcontrolpanel.h"
#include "widgets/fwflashlogpanel.h"

#include <QByteArray>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QShowEvent>
#include <QSplitter>
#include <QTime>
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

/// 把补齐后的 DW vendor hex 文本另存到原始 hex 文件所在目录。
/// 支持 Hl9788Hex (16384 行) 与 Dw9786Hex (10240 行) 两种格式，拆字规则按 info.format 切换。
/// 文件名：<stem>--00000000--<HHMMSS>.hex（ASCII 双减号 + 字面 8 个 0 + PC 本地时分秒）
/// 仅在 info.paddingApplied 时由调用方触发；返回是否成功 + 输出路径或错误信息。
bool saveDwPaddedHex(const FirmwareInfo &info,
                     const QString &originalPath,
                     QString *outSavedPath,
                     QString *outErrorMessage) {
    int kExpectedWords = 0;
    bool highFirst = false;
    switch (info.format) {
    case FirmwareFormat::Hl9788Hex:
        kExpectedWords = 32768;  // 16384 lines
        highFirst = false;       // parser: out[2i]=low, out[2i+1]=high
        break;
    case FirmwareFormat::Dw9786Hex:
        kExpectedWords = 20480;  // 10240 lines
        highFirst = true;        // parser: out[2i]=high, out[2i+1]=low
        break;
    default:
        if (outErrorMessage) *outErrorMessage = QStringLiteral("不支持的固件格式");
        return false;
    }
    const int kExpectedLines = kExpectedWords / 2;
    constexpr int kLineBytes = 9;  // 8 hex 字符 + LF

    if (info.data.size() != kExpectedWords * static_cast<int>(sizeof(quint16))) {
        if (outErrorMessage) *outErrorMessage = QStringLiteral("固件数据大小异常");
        return false;
    }
    const auto *outWords = reinterpret_cast<const quint16 *>(info.data.constData());

    // 反推 32-bit 行内容（与 parser 拆字规则对称）：
    //   HL9788N: parser out[2i]=low, out[2i+1]=high → reverse: val = (out[2i+1]<<16) | out[2i]
    //   DW9786:  parser out[2i]=high, out[2i+1]=low → reverse: val = (out[2i]<<16) | out[2i+1]
    QByteArray textBuf;
    textBuf.resize(kExpectedLines * kLineBytes);
    static constexpr char kHexTable[] = "0123456789ABCDEF";
    char *p = textBuf.data();
    for (int i = 0; i < kExpectedLines; ++i) {
        const quint32 hi = highFirst ? outWords[2 * i] : outWords[2 * i + 1];
        const quint32 lo = highFirst ? outWords[2 * i + 1] : outWords[2 * i];
        const quint32 v = (hi << 16) | lo;
        p[0] = kHexTable[(v >> 28) & 0xFu];
        p[1] = kHexTable[(v >> 24) & 0xFu];
        p[2] = kHexTable[(v >> 20) & 0xFu];
        p[3] = kHexTable[(v >> 16) & 0xFu];
        p[4] = kHexTable[(v >> 12) & 0xFu];
        p[5] = kHexTable[(v >>  8) & 0xFu];
        p[6] = kHexTable[(v >>  4) & 0xFu];
        p[7] = kHexTable[v & 0xFu];
        p[8] = '\n';
        p += kLineBytes;
    }

    const QFileInfo fi(originalPath);
    const QString stem = fi.completeBaseName();
    const QString dir  = fi.absolutePath();
    const QString suffix = fi.suffix().isEmpty() ? QStringLiteral("hex") : fi.suffix();
    const QString hhmmss = QTime::currentTime().toString(QStringLiteral("HHmmss"));
    // 统一用 ASCII 双减号分隔
    const QString baseName = stem + QStringLiteral("--00000000--") + hhmmss;

    QString candidate = dir + QLatin1Char('/') + baseName + QLatin1Char('.') + suffix;
    for (int n = 1; QFileInfo::exists(candidate) && n <= 1000; ++n) {
        candidate = dir + QLatin1Char('/') + baseName + QLatin1Char('_')
                    + QString::number(n) + QLatin1Char('.') + suffix;
    }

    QSaveFile saver(candidate);
    if (!saver.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outErrorMessage) *outErrorMessage = saver.errorString();
        return false;
    }
    if (saver.write(textBuf) != static_cast<qint64>(textBuf.size())) {
        if (outErrorMessage) *outErrorMessage = saver.errorString();
        saver.cancelWriting();
        return false;
    }
    if (!saver.commit()) {
        if (outErrorMessage) *outErrorMessage = saver.errorString();
        return false;
    }
    if (outSavedPath) *outSavedPath = candidate;
    return true;
}

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

FwFlashTab::FwFlashTab(SerialManager *serialManager, DeviceContext *deviceContext, QWidget *parent)
    : QWidget(parent)
    , m_deviceContext(deviceContext) {
    setupUi();

    // FwFlashLogPanel 在 setupUi() 中创建；构造 logSink lambda 把 AwLocalIspStrategy 的
    // 4 级日志 marshal 到 GUI 线程并转发到面板的 4 个 append 槽。QPointer 防御面板
    // 先于 strategy 销毁。
    QPointer<FwFlashLogPanel> logPanelPtr(m_logPanel);
    auto logSink = [logPanelPtr](AwLocalIspStrategy::LogLevel lv, const QString &msg) {
        if (logPanelPtr.isNull()) return;
        QMetaObject::invokeMethod(logPanelPtr.data(), [logPanelPtr, lv, msg]() {
            if (logPanelPtr.isNull()) return;
            switch (lv) {
            case AwLocalIspStrategy::LogLevel::Info:  logPanelPtr->appendInfo(msg); break;
            case AwLocalIspStrategy::LogLevel::Warn:  logPanelPtr->appendWarn(msg); break;
            case AwLocalIspStrategy::LogLevel::Error: logPanelPtr->appendError(msg); break;
            case AwLocalIspStrategy::LogLevel::Ok:    logPanelPtr->appendOk(msg); break;
            }
        }, Qt::QueuedConnection);
    };

    m_registry = std::make_unique<FlashStrategyRegistry>(serialManager, logSink);
    m_service = new FwFlashService(m_registry.get(), serialManager, this);

    connectSignals();
    rebuildIcCombo();

    syncIcFromContext();   // 目标 IC 跟随配置页 Select IC（会触发 onIcChanged）
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
    // 目标 IC 只读跟随配置页 Select IC（DeviceContext），不在本页单独选择
    m_icCombo->setEnabled(false);
    m_icCombo->setToolTip(tr("目标 IC 跟随配置页 Select IC，请在配置页修改"));
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
    // 目标 IC 单向跟随配置页 Select IC（DeviceContext 为唯一来源）
    if (m_deviceContext != nullptr) {
        connect(m_deviceContext, &DeviceContext::icTypeChanged,
                this, [this](MotorIcType) { syncIcFromContext(); });
    }

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
    // 目标 IC 始终跟随配置页一个有效选择，故不再设"请选择目标 IC..."占位项
    m_icCombo->blockSignals(true);
    m_icCombo->clear();
    for (FlashStrategy *s : m_registry->all()) {
        m_icCombo->addItem(s->icModel(), s->icModel());
    }
    m_icCombo->setCurrentIndex(0);
    m_icCombo->blockSignals(false);
}

/// @brief 按配置页 Select IC（DeviceContext）选中对应的目标 IC，单向只读跟随。
void FwFlashTab::syncIcFromContext() {
    if (m_deviceContext == nullptr) {
        return;
    }
    const QString icModel = DeviceContext::motorIcTypeToString(m_deviceContext->icType());
    const int idx = m_icCombo->findData(icModel);
    if (idx < 0) {
        return;   // 该 IC 无对应烧录策略（正常不会发生）
    }
    if (idx == m_icCombo->currentIndex()) {
        onIcChanged(idx);                 // 索引未变，手动刷新描述/启用态
    } else {
        m_icCombo->setCurrentIndex(idx);  // 变化 → currentIndexChanged 自动触发 onIcChanged
    }
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

    // 根据当前 IC combo 选择给 parser 传 hint：DW9786 / DW9788 hex 行内格式
    // 相同（每行 8 hex），仅拆字规则与预期行数不同，无法靠文件内容嗅探区分。
    IcHint icHint = IcHint::Auto;
    const QString icModelSelected = m_icCombo->currentData().toString();
    if (icModelSelected == QStringLiteral("DW9786")) {
        icHint = IcHint::Dw9786;
    } else if (icModelSelected == QStringLiteral("DW9788")) {
        icHint = IcHint::Hl9788;
    }

    const FirmwareInfo info = FirmwareParser::parseFile(path, icHint);
    m_infoPanel->setInfo(info);

    if (!info.valid) {
        m_currentFileValid = false;
        m_currentFirmwareData.clear();
        m_currentFirmwareTotal = 0;
        m_logPanel->appendError(tr("文件解析失败：%1").arg(info.errorMessage));
        updateStartEnabled();
        return;
    }

    // AW 本地 ISP 上限校验（STM32 端 SRAM1 单缓冲 64 KB）
    // 注意：64 KB = 65536 本身已 4 字节对齐，下面的补零最多 +3 字节，
    // 原文件 ≤ 65536 时补零后仍 ≤ 65536，不会因补零导致越界。
    constexpr qint64 kMaxIspBytes = 64LL * 1024LL;
    if (info.data.size() > kMaxIspBytes) {
        m_currentFileValid = false;
        m_currentFirmwareData.clear();
        m_currentFirmwareTotal = 0;
        m_logPanel->appendError(
            tr("固件 %1 字节超出 AW 本地 ISP 上限（64 KB），无法烧录")
                .arg(info.data.size()));
        updateStartEnabled();
        return;
    }

    // 4 字节对齐处理：若原始数据非对齐，末尾补 0xFF 至 4 字节边界。
    // 选 0xFF 与 Flash 擦除态一致，写入等价于不写；HEX 解析器段间填充也用 0xFF。
    // 信息面板继续显示原始字节数与 CRC32（用作固件版本标识）；
    // 实际写入 STM32 / 进度条 total 基于补零后的字节数；
    // strategy 入口的对齐校验保留作为防御性兜底，正常路径下永不触发。
    QByteArray firmwareData = info.data;
    const int padBytes = static_cast<int>((4 - firmwareData.size() % 4) % 4);
    if (padBytes > 0) {
        firmwareData.append(padBytes, static_cast<char>(0xFF));
    }

    m_currentFileValid = true;
    m_currentFirmwareData = firmwareData;
    m_currentFirmwareTotal = firmwareData.size();
    m_logPanel->appendInfo(
        tr("已加载固件：%1（%2 字节，CRC32 0x%3）")
            .arg(info.fileName)
            .arg(info.data.size())
            .arg(info.crc32, 8, 16, QLatin1Char('0')));
    const bool isDwPadded = (info.format == FirmwareFormat::Hl9788Hex ||
                              info.format == FirmwareFormat::Dw9786Hex) &&
                             info.paddingApplied;
    if (isDwPadded) {
        const int expectedLines = (info.format == FirmwareFormat::Hl9788Hex) ? 16384 : 10240;
        const QString icLabel = (info.format == FirmwareFormat::Hl9788Hex)
                                    ? QStringLiteral("HL9788N hex")
                                    : QStringLiteral("DW9786 hex");
        m_logPanel->appendWarn(
            tr("%1 仅 %2 行（< %3），已自动补齐：填 0 + footer CRC32 0x%4，"
               "实际烧入 %5 字节")
                .arg(icLabel)
                .arg(info.originalLines)
                .arg(expectedLines)
                .arg(info.footerCrc32, 8, 16, QLatin1Char('0'))
                .arg(info.data.size()));

        QString savedPath, saveErr;
        const bool savedOk = saveDwPaddedHex(info, m_currentFilePath,
                                              &savedPath, &saveErr);
        if (savedOk) {
            m_logPanel->appendOk(
                tr("已保存补齐烧录文件：%1")
                    .arg(QDir::toNativeSeparators(savedPath)));
        } else {
            m_logPanel->appendWarn(
                tr("补齐烧录文件保存失败（不影响继续烧录）：%1").arg(saveErr));
        }
    }
    if (padBytes > 0) {
        m_logPanel->appendWarn(
            tr("固件 %1 字节非 4 字节对齐，已自动末尾补 %2 字节 0xFF → %3 字节后写入")
                .arg(info.data.size())
                .arg(padBytes)
                .arg(firmwareData.size()));
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
    // 已选文件 + 切到 DW 系列 IC 时，必须重新按对应 IcHint 解析（DW9786 / DW9788
    // hex 行内格式相同但拆字 + 行数不同，旧解析结果用错 IC 会让 strategy size
    // 校验失败或烧错数据）
    if (!m_currentFilePath.isEmpty() &&
        (icModel == QStringLiteral("DW9786") || icModel == QStringLiteral("DW9788"))) {
        parseAndShowFile(m_currentFilePath);
        return;
    }
    updateStartEnabled();
}

void FwFlashTab::updateStartEnabled() {
    const bool icSelected = !m_icCombo->currentData().toString().isEmpty();
    const bool busy = m_service->isBusy();
    m_controlPanel->setStartEnabled(!busy && icSelected && m_currentFileValid);
    m_controlPanel->setCancelEnabled(busy);

    // m_icCombo 始终只读（跟随配置页），不参与 busy 启用切换
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
