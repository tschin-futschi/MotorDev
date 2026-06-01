// =============================================================================
// @file    fwfileinfopanel.cpp
// @brief   固件信息视图实现 — v2 起作为内嵌 QWidget，由父卡片提供 GroupBox 外框
// =============================================================================
#include "widgets/fwfileinfopanel.h"

#include "protocol/firmware_parser.h"
#include "ui/style_constants.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace MotorDev;

namespace {

QLabel *makeLabel(QWidget *parent, const QString &text, bool valueRole) {
    auto *l = new QLabel(text, parent);
    l->setObjectName(valueRole ? QStringLiteral("infoValue") : QStringLiteral("infoLabel"));
    QFont f = l->font();
    if (valueRole) f.setFamily(QString::fromLatin1(Style::Font::Monospace));
    f.setPixelSize(11);
    l->setFont(f);
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText,
               valueRole ? Style::Color::FwFlashFileInfoValueFg
                         : Style::Color::FwFlashFileInfoLabelFg);
    l->setPalette(p);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QString formatSize(qint64 bytes) {
    // 自由函数无 tr()，用 QCoreApplication::translate 归入 FwFileInfoPanel 上下文
    return QCoreApplication::translate("FwFileInfoPanel", "%1 字节 (%2 KB)")
        .arg(bytes)
        .arg(QString::number(bytes / 1024.0, 'f', 1));
}

QString formatHex32(quint32 v) {
    return QStringLiteral("0x%1").arg(v, 8, 16, QLatin1Char('0')).toUpper().replace("0X", "0x");
}

}  // namespace

FwFileInfoPanel::FwFileInfoPanel(QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("FwFileInfoPanel"));
    setupUi();
    clear();
}

FwFileInfoPanel::~FwFileInfoPanel() = default;

void FwFileInfoPanel::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("infoStack"));
    root->addWidget(m_stack);

    // 页 0：空状态——单行灰色提示
    auto *emptyPage = new QWidget(m_stack);
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    m_emptyLabel = new QLabel(tr("请先选择固件文件"), emptyPage);
    m_emptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QPalette emptyPal = m_emptyLabel->palette();
    emptyPal.setColor(QPalette::WindowText, Style::Color::MutedText);
    m_emptyLabel->setPalette(emptyPal);
    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setPixelSize(11);
    m_emptyLabel->setFont(emptyFont);
    emptyLayout->addWidget(m_emptyLabel);
    m_stack->addWidget(emptyPage);

    // 页 1：合法字段（网格 2 列）
    auto *validPage = new QWidget(m_stack);
    auto *grid = new QGridLayout(validPage);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(Style::Size::FormSpacing);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);

    int row = 0;
    m_fileNameLabel = makeLabel(validPage, tr("文件名"), false);
    grid->addWidget(m_fileNameLabel, row, 0, Qt::AlignLeft);
    m_fileNameValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_fileNameValue, row++, 1, Qt::AlignLeft);

    m_fileSizeLabel = makeLabel(validPage, tr("文件大小"), false);
    grid->addWidget(m_fileSizeLabel, row, 0, Qt::AlignLeft);
    m_fileSizeValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_fileSizeValue, row++, 1, Qt::AlignLeft);

    m_formatLabel = makeLabel(validPage, tr("文件格式"), false);
    grid->addWidget(m_formatLabel, row, 0, Qt::AlignLeft);
    m_formatValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_formatValue, row++, 1, Qt::AlignLeft);

    m_crc32Label = makeLabel(validPage, tr("CRC32"), false);
    grid->addWidget(m_crc32Label, row, 0, Qt::AlignLeft);
    m_crc32Value = makeLabel(validPage, QString(), true);
    grid->addWidget(m_crc32Value, row++, 1, Qt::AlignLeft);

    m_segCountLabel = makeLabel(validPage, tr("段数"), false);
    grid->addWidget(m_segCountLabel, row, 0, Qt::AlignLeft);
    m_segCountValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_segCountValue, row++, 1, Qt::AlignLeft);

    m_addrRangeLabel = makeLabel(validPage, tr("地址范围"), false);
    grid->addWidget(m_addrRangeLabel, row, 0, Qt::AlignLeft);
    m_addrRangeValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_addrRangeValue, row++, 1, Qt::AlignLeft);

    m_effectiveLabel = makeLabel(validPage, tr("有效字节"), false);
    grid->addWidget(m_effectiveLabel, row, 0, Qt::AlignLeft);
    m_effectiveValue = makeLabel(validPage, QString(), true);
    grid->addWidget(m_effectiveValue, row++, 1, Qt::AlignLeft);

    m_paddingLabel = makeLabel(validPage, tr("自动补齐"), false);
    grid->addWidget(m_paddingLabel, row, 0, Qt::AlignLeft);
    m_paddingValue = makeLabel(validPage, QString(), true);
    m_paddingValue->setWordWrap(true);
    grid->addWidget(m_paddingValue, row++, 1, Qt::AlignLeft);

    grid->setRowStretch(row, 1);
    m_stack->addWidget(validPage);

    // 页 2：错误
    auto *errorPage = new QWidget(m_stack);
    auto *errorLayout = new QVBoxLayout(errorPage);
    errorLayout->setContentsMargins(0, 0, 0, 0);
    m_errorLabel = new QLabel(errorPage);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QPalette errPal = m_errorLabel->palette();
    errPal.setColor(QPalette::WindowText, Style::Color::FwFlashFileInfoErrorFg);
    m_errorLabel->setPalette(errPal);
    QFont errFont = m_errorLabel->font();
    errFont.setPixelSize(11);
    m_errorLabel->setFont(errFont);
    errorLayout->addWidget(m_errorLabel);
    errorLayout->addStretch();
    m_stack->addWidget(errorPage);
}

void FwFileInfoPanel::clear() {
    m_stack->setCurrentIndex(0);
}

void FwFileInfoPanel::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void FwFileInfoPanel::retranslateUi() {
    if (m_emptyLabel != nullptr)     m_emptyLabel->setText(tr("请先选择固件文件"));
    if (m_fileNameLabel != nullptr)  m_fileNameLabel->setText(tr("文件名"));
    if (m_fileSizeLabel != nullptr)  m_fileSizeLabel->setText(tr("文件大小"));
    if (m_formatLabel != nullptr)    m_formatLabel->setText(tr("文件格式"));
    if (m_crc32Label != nullptr)     m_crc32Label->setText(tr("CRC32"));
    if (m_segCountLabel != nullptr)  m_segCountLabel->setText(tr("段数"));
    if (m_addrRangeLabel != nullptr) m_addrRangeLabel->setText(tr("地址范围"));
    if (m_effectiveLabel != nullptr) m_effectiveLabel->setText(tr("有效字节"));
    if (m_paddingLabel != nullptr)   m_paddingLabel->setText(tr("自动补齐"));
}

void FwFileInfoPanel::setInfo(const FirmwareInfo &info) {
    if (!info.valid) {
        m_errorLabel->setText(tr("解析失败：%1").arg(info.errorMessage));
        m_stack->setCurrentIndex(2);
        return;
    }

    m_fileNameValue->setText(info.fileName);
    m_fileSizeValue->setText(formatSize(info.fileSizeBytes));

    QString formatText;
    switch (info.format) {
    case FirmwareFormat::Bin:        formatText = tr("Binary"); break;
    case FirmwareFormat::Hex:        formatText = tr("Intel HEX"); break;
    case FirmwareFormat::Hl9788Hex:  formatText = tr("Dongwoon HL9788N hex"); break;
    case FirmwareFormat::Dw9786Hex:  formatText = tr("Dongwoon DW9786 hex"); break;
    default:                          formatText = tr("Unknown"); break;
    }
    m_formatValue->setText(formatText);
    m_crc32Value->setText(formatHex32(info.crc32));

    const bool isIntelHex = info.format == FirmwareFormat::Hex;
    const bool isHl9788   = info.format == FirmwareFormat::Hl9788Hex;
    const bool isDw9786   = info.format == FirmwareFormat::Dw9786Hex;
    const bool isDwCustom = isHl9788 || isDw9786;

    // 段数 / 地址范围：仅 Intel HEX 有意义
    m_segCountLabel->setVisible(isIntelHex);
    m_segCountValue->setVisible(isIntelHex);
    m_addrRangeLabel->setVisible(isIntelHex);
    m_addrRangeValue->setVisible(isIntelHex);

    // 有效字节：Intel HEX 与 DW vendor hex 都有意义
    const bool showEffective = isIntelHex || isDwCustom;
    m_effectiveLabel->setVisible(showEffective);
    m_effectiveValue->setVisible(showEffective);

    // 自动补齐行：仅 DW vendor hex 且触发补齐时显示
    const bool showPadding = isDwCustom && info.paddingApplied;
    m_paddingLabel->setVisible(showPadding);
    m_paddingValue->setVisible(showPadding);

    if (isIntelHex) {
        m_segCountValue->setText(QString::number(info.segments.size()));
        m_addrRangeValue->setText(QStringLiteral("%1 - %2")
                                      .arg(formatHex32(info.minAddress))
                                      .arg(formatHex32(info.maxAddress)));
        m_effectiveValue->setText(tr("%1 字节").arg(info.effectiveBytes));
    } else if (isDwCustom) {
        m_effectiveValue->setText(tr("%1 字节").arg(info.effectiveBytes));
        if (info.paddingApplied) {
            // 行数计算（期望行数 / footer 占位）已下沉 FirmwareParser，UI 仅展示
            m_paddingValue->setText(
                tr("%1 原始行 + %2 填充 0 行 + footer (CRC32=%3, 末行全 0)")
                    .arg(info.originalLines)
                    .arg(info.paddingZeroLines)
                    .arg(formatHex32(info.footerCrc32)));
        }
    }

    m_stack->setCurrentIndex(1);
}
